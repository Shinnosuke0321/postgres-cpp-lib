//
// Created by Shinnosuke Kawai on 1/26/26.
//
#include <print>
#include "database/postgres.h"

namespace Database {
    std::future<std::expected<UniquePGResult, PostgresErr>> Postgres::SendToWorker(PgParamDetail&& query_detail) const {
        using Result = std::expected<UniquePGResult, PostgresErr>;
        auto prom = std::make_shared<std::promise<Result>>();
        auto future = prom->get_future();
        PGRequest request{std::move(query_detail)};
        request.on_success = [prom](UniquePGResult reply) {
            try {
                prom->set_value(std::move(reply));
            } catch (...) {}
        };
        request.on_error = [prom](const PostgresErr& err) {
            try {
                prom->set_value(std::unexpected(err));
            } catch (...) {}
        };
        {
            std::lock_guard sl(m_mutex);
            m_requests.push_back(std::move(request));
        }
        m_cv.notify_one();
        return future;
    }

    void Postgres::QueryWorker(const std::stop_token &st) const noexcept {
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution heartbeat_sec(60, 120);
        auto next_heartbeat = std::chrono::steady_clock::now() + std::chrono::seconds(heartbeat_sec(rng));
        while (!st.stop_requested()) {
            PGRequest request;
            {
                std::unique_lock sl(m_mutex);
                if (!m_heartbeat_enabled) {
                    m_cv.wait(sl, [&] { return st.stop_requested() || !m_requests.empty();});
                } else {
                    const auto now = std::chrono::steady_clock::now();
                    if (now >= next_heartbeat) {
                        sl.unlock();

                        constexpr std::string_view heartbeat_query = "SELECT 1";
                        std::vector<std::string> empty_params{};
                        auto timeout = std::chrono::milliseconds(5000);
                        PgParamDetail ping_detail{heartbeat_query, 0};
                        if (auto heart_beat = ExecuteWithRetry(ping_detail, timeout)) {
                            std::println("Postgres: heartbeat successful");
                        } else {
                            std::println("Postgres: heartbeat failed");
                        }
                        next_heartbeat = std::chrono::steady_clock::now() + std::chrono::seconds(heartbeat_sec(rng));
                        continue;
                    }
                    m_cv.wait_for(sl, next_heartbeat - now, [&] {
                        return st.stop_requested() || !m_requests.empty();
                    });
                }
                if (st.stop_requested()) {
                    std::deque<PGRequest> pending_reqs;
                    pending_reqs.swap(m_requests);
                    sl.unlock();
                    for (auto& pending : pending_reqs) {
                        if (pending.on_error)
                            pending.on_error(PostgresErr::ShuttingDown("worker thread stopped"));
                    }
                    break;
                }
                if (m_requests.empty()) {
                    continue;
                }
                request = std::move(m_requests.front());
                m_requests.pop_front();
            }

            std::expected<UniquePGResult, PostgresErr> result = ExecuteWithRetry(request.detail,std::chrono::milliseconds(5000));
            if (!result) {
                request.on_error(result.error());
                continue;
            }
            request.on_success(std::move(result.value()));
        }
    }

    std::expected<UniquePGResult, PostgresErr> Postgres::ExecuteWithRetry(const PgParamDetail& param_detail, const std::chrono::milliseconds reconnect_timeout) const noexcept {
        for (int attempts = 1; attempts <= 2; ++attempts) {
            if (!is_connected()) {
                // LOG_DEBUG << "Connection is dead";
                if (std::optional<PostgresErr> error = AttemptReconnect(reconnect_timeout)) {
                    return std::unexpected(*error);
                }
            }

            std::expected<UniquePGResult, PostgresErr> exe_res = ExecuteQuery(param_detail);
            if (exe_res) {
                return exe_res;
            }
            PostgresErr& err = exe_res.error();
            if (err.get_type() == PostgresErr::Type::BadConnection && attempts == 1) {
                if (auto error = AttemptReconnect(reconnect_timeout)) {
                    return std::unexpected(*error);
                }
                continue;
            }
            return std::unexpected(err);
        }
        return std::unexpected(PostgresErr::QueryFailed("unreachable"));
    }

    std::expected<UniquePGResult, PostgresErr> Postgres::ExecuteQuery(const PgParamDetail& param_detail) const noexcept {
        const int sock = PQsocket(m_connection.get());
        if (sock < 0) {
            return std::unexpected(PostgresErr::SocketFailed("failed to get socket"));
        }
        // PrepareQueryParams(params, argc, arg_lengths, n_params);

        const int ok = PQsendQueryParams(
            m_connection.get(),
            param_detail.query.c_str(),
            param_detail.count(),
            nullptr,
            param_detail.buffers.data(),
            param_detail.lengths.data(),
            param_detail.formats.data(),
            0);

        if (ok == 0) {
            const char* msg = PQerrorMessage(m_connection.get());
            return std::unexpected(PostgresErr::BadConnection(msg));
        }

        if (auto poll_out = CheckForPollOut(sock); !poll_out) {
            return std::unexpected(poll_out.error());
        }
        if (auto poll_in = CheckForPollIn(sock); !poll_in) {
            return std::unexpected(poll_in.error());
        }

        return ConsumeResult();
    }

    std::optional<PostgresErr> Postgres::AttemptReconnect(const std::chrono::milliseconds timeout) const noexcept {
        if (!PQresetStart(m_connection.get()))
            return PostgresErr::FailedToReconnect("PQresetStart failed");

        const auto deadline = std::chrono::steady_clock::now() + timeout;

        // LOG_DEBUG << "Attempting to reconnect";
        while (true) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
                return PostgresErr::FailedToReconnect("timeout");

            const PostgresPollingStatusType st = PQresetPoll(m_connection.get());
            if (st == PGRES_POLLING_OK) {
                if (PQsetnonblocking(m_connection.get(), 1) != 0) {
                    const char* err = PQerrorMessage(m_connection.get());
                    return PostgresErr::FailedToReconnect(err ? err :"PQsetnonblocking failed");
                }
                // LOG_DEBUG << "Reconnected";
                return std::nullopt;
            }
            if (st == PGRES_POLLING_FAILED)
                return PostgresErr::FailedToReconnect("PQresetPoll failed");

            const int sock = PQsocket(m_connection.get());
            if (sock < 0)
                return PostgresErr::FailedToReconnect("PQsocket failed");
            short events = 0;
            if (st == PGRES_POLLING_READING)
                events = POLLIN;
            if (st == PGRES_POLLING_WRITING)
                events = POLLOUT;

            pollfd pfd{sock, events, 0};
            const int remaining = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
            const int pr = poll(&pfd, 1, remaining);
            if (pr <= 0)
                return PostgresErr::FailedToReconnect("poll failed");
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
                return PostgresErr::FailedToReconnect("socket error");
        }
    }

    std::expected<void, PostgresErr> Postgres::CheckForPollOut(const int& socket) const noexcept {
        while (true) {
            const int flush = PQflush(m_connection.get());
            if (flush < 0) {
                return std::unexpected(PostgresErr::SocketFailed("failed to flush socket"));
            }
            if (flush == 0)
                return {};

            pollfd pfd = {socket, POLLOUT, 0};
            const int poll_res = poll(&pfd, 1, 5000);
            if (poll_res < 0) {
                return std::unexpected(PostgresErr::SocketFailed("Pollout event failed"));
            }
            if (poll_res == 0) {
                return std::unexpected(PostgresErr::SocketFailed("socket timed out"));
            }
            if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                return std::unexpected(PostgresErr::SocketFailed("Socket failed"));
            }
        }
    }

    std::expected<void, PostgresErr> Postgres::CheckForPollIn(const int& socket) const noexcept {
        while (true) {
            pollfd pfd = {socket, POLLIN, 0};
            const int poll_res = poll(&pfd, 1, 5000);
            if (poll_res < 0) {
                return std::unexpected(PostgresErr::SocketFailed("failed to poll socket"));
            }
            if (poll_res == 0) {
                return std::unexpected(PostgresErr::SocketFailed("socket timed out"));
            }

            if (PQconsumeInput(m_connection.get()) == 0) {
                const char* err = PQerrorMessage(m_connection.get());
                return std::unexpected(PostgresErr::BadConnection(err));
            }

            if (PQisBusy(m_connection.get())) {
                continue;
            }
            return {};
        }
    }

    std::expected<UniquePGResult, PostgresErr> Postgres::ConsumeResult() const noexcept {
        UniquePGResult result = nullptr;
        while (PGresult* r = PQgetResult(m_connection.get())) {
            UniquePGResult temp(r);
            const auto st = PQresultStatus(temp.get());
            if (st == PGRES_TUPLES_OK || st == PGRES_COMMAND_OK) {
                if (!result)
                    result = std::move(temp);
            } else {
                // Make sure we drain remaining results before returning error (optional but clean)
                std::string msg = PQresultErrorMessage(temp.get());
                PostgresErr err = PostgresErr::QueryFailed(PQresultErrorMessage(temp.get()));
                while (PGresult* r2 = PQgetResult(m_connection.get()))
                    PQclear(r2);
                return std::unexpected(std::move(err));
            }
        }
        if (!result) {
            return std::unexpected(PostgresErr::QueryFailed("no results received"));
        }
        return std::move(result);
    }
}
