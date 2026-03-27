//
// Created by Shinnosuke Kawai on 1/26/26.
//
#include "database/postgres_client.h"
#ifdef _WIN32
#define poll WSAPoll
#endif

namespace database {
    namespace {
        thread_local bool tl_is_callback_thread = false;
    }
}
namespace database {
    std::future<std::expected<result::table, sql_error>> postgres_client::SendToWorker(pg_param_detail&& query_detail) const {
        using Result = std::expected<result::table, sql_error>;
        auto prom = std::make_shared<std::promise<Result>>();
        auto future = prom->get_future();
        query_request request{std::move(query_detail)};
        request.direct_callback = true;
        request.on_success = [prom](result::table table) {
            try {
                prom->set_value(std::move(table));
            } catch (...) {}
        };
        request.on_error = [prom](const sql_error& err) {
            try {
                prom->set_value(std::unexpected(err));
            } catch (...) {}
        };
        {
            std::lock_guard sl(m_mutex);
            m_requests.emplace_back(std::move(request));
        }
        m_cv.notify_one();
        return future;
    }

    void postgres_client::EnqueueAsync(pg_param_detail&& detail, result_callback&& callback, error_callback&& err_callback) const noexcept {
        query_request request{};
        request.detail = std::move(detail);
        request.on_success = std::move(callback);
        request.on_error = std::move(err_callback);
        {
            std::lock_guard sl(m_mutex);
            m_requests.emplace_back(std::move(request));
        }
        m_cv.notify_one();
    }

    void postgres_client::PostCallback(std::function<void()> task) const noexcept {
        if (tl_is_callback_thread) {
            task();
            return;
        }
        if (m_idle_cb_workers.load(std::memory_order_acquire) == 0) {
            auto done = std::make_shared<std::atomic_bool>(false);
            std::jthread temp([done, task = std::move(task)]() mutable noexcept {
                tl_is_callback_thread = true;
                task();
                done->store(true, std::memory_order_release);
            });

            {
                std::lock_guard lk(m_temp_cb_mutex);
                reap_overflow_threads(m_overflow_cb_threads);
                m_overflow_cb_threads.push_back({
                    .done = done,
                    .thread = std::move(temp)
                });
            }
            return;
        }
        {
            std::lock_guard lk(m_cb_mutex);
            m_cb_queue.push_back(std::move(task));
        }
        m_cb_cv.notify_one();
    }

    void postgres_client::QueryWorker(const std::stop_token &st) const noexcept {
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution heartbeat_sec(60, 120);
        auto next_heartbeat = std::chrono::steady_clock::now() + std::chrono::seconds(heartbeat_sec(rng));
        while (!st.stop_requested()) {
            query_request item;
            {
                std::unique_lock sl(m_mutex);
                if (!m_heartbeat_enabled) {
                    m_cv.wait(sl, [&] { return st.stop_requested() || !m_requests.empty();});
                } else {
                    const auto now = std::chrono::steady_clock::now();
                    if (now >= next_heartbeat) {
                        sl.unlock();

                        constexpr std::string_view heartbeat_query = "SELECT 1";
                        auto timeout = std::chrono::milliseconds(5000);
                        pg_param_detail ping_detail{heartbeat_query, 0};
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
                    std::deque<query_request> pending_reqs;
                    pending_reqs.swap(m_requests);
                    sl.unlock();
                    for (auto& pending_item : pending_reqs) {
                        auto cb = std::move(pending_item.on_error);
                        if (pending_item.direct_callback) {
                            cb(sql_error::ShuttingDown("worker thread stopped"));
                        } else {
                            PostCallback([cb = std::move(cb)] { cb(sql_error::ShuttingDown("worker thread stopped")); });
                        }
                    }
                    break;
                }
                if (m_requests.empty()) {
                    continue;
                }
                item = std::move(m_requests.front());
                m_requests.pop_front();
            }

            std::expected<result::unique_pg_result, sql_error> result = ExecuteWithRetry(item.detail, std::chrono::milliseconds(5000));
            if (!result) {
                auto cb  = std::move(item.on_error);
                sql_error& err = result.error();
                if (item.direct_callback) {
                    cb(err);
                } else {
                    PostCallback([cb = std::move(cb), err] { cb(err); });
                }
                continue;
            }
            auto cb = std::move(item.on_success);
            if (item.direct_callback) {
                cb(result::table{std::move(result.value())});
            } else {
                auto shared_table = std::make_shared<result::table>(std::move(result.value()));
                PostCallback([cb = std::move(cb), shared_table]() mutable {
                    cb(std::move(*shared_table));
                });
            }
        }
    }

    void postgres_client::CallbackWorker(const std::stop_token& st) const noexcept {
        tl_is_callback_thread = true;
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lk(m_cb_mutex);
                m_idle_cb_workers.fetch_add(1, std::memory_order_release);
                m_cb_cv.wait(lk, [&] { return st.stop_requested() || !m_cb_queue.empty(); });
                m_idle_cb_workers.fetch_sub(1, std::memory_order_release);
                if (m_cb_queue.empty())
                    break; // stop requested and queue fully drained
                task = std::move(m_cb_queue.front());
                m_cb_queue.pop_front();
            }
            task();
        }
    }

    std::expected<result::unique_pg_result, sql_error> postgres_client::ExecuteWithRetry(const pg_param_detail& param_detail, const std::chrono::milliseconds reconnect_timeout) const noexcept {
        for (int attempts = 1; attempts <= 2; ++attempts) {
            if (!is_connected()) {
                if (std::optional<sql_error> error = AttemptReconnect(reconnect_timeout)) {
                    return std::unexpected(*error);
                }
            }

            std::expected<result::unique_pg_result, sql_error> exe_res = ExecuteQuery(param_detail);
            if (exe_res) {
                return exe_res;
            }
            sql_error& err = exe_res.error();
            if (err.get_type() == sql_error::type::BadConnection && attempts == 1) {
                if (auto error = AttemptReconnect(reconnect_timeout)) {
                    return std::unexpected(*error);
                }
                continue;
            }
            return std::unexpected(err);
        }
        return std::unexpected(sql_error::QueryFailed("unreachable"));
    }

    std::expected<result::unique_pg_result, sql_error> postgres_client::ExecuteQuery(const pg_param_detail& param_detail) const noexcept {
        const int sock = PQsocket(m_connection.get());
        if (sock < 0) {
            return std::unexpected(sql_error::SocketFailed("failed to get socket"));
        }
        const int ok = PQsendQueryParams(
            m_connection.get(),
            param_detail.query.c_str(),
            param_detail.count(),
            nullptr,
            param_detail.buffers.data(),
            param_detail.lengths.data(),
            param_detail.formats.data(),
            1);

        if (ok == 0) {
            const char* msg = PQerrorMessage(m_connection.get());
            return std::unexpected(sql_error::BadConnection(msg));
        }

        if (auto poll_out = CheckForPollOut(sock); !poll_out) {
            return std::unexpected(poll_out.error());
        }
        if (auto poll_in = CheckForPollIn(sock); !poll_in) {
            return std::unexpected(poll_in.error());
        }

        return ConsumeResult();
    }

    std::optional<sql_error> postgres_client::AttemptReconnect(const std::chrono::milliseconds timeout) const noexcept {
        if (!PQresetStart(m_connection.get()))
            return sql_error::FailedToReconnect("PQresetStart failed");

        const auto deadline = std::chrono::steady_clock::now() + timeout;

        // LOG_DEBUG << "Attempting to reconnect";
        while (true) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
                return sql_error::FailedToReconnect("timeout");

            const PostgresPollingStatusType st = PQresetPoll(m_connection.get());
            if (st == PGRES_POLLING_OK) {
                if (PQsetnonblocking(m_connection.get(), 1) != 0) {
                    const char* err = PQerrorMessage(m_connection.get());
                    return sql_error::FailedToReconnect(err ? err :"PQsetnonblocking failed");
                }
                // LOG_DEBUG << "Reconnected";
                return std::nullopt;
            }
            if (st == PGRES_POLLING_FAILED)
                return sql_error::FailedToReconnect("PQresetPoll failed");

            const int sock = PQsocket(m_connection.get());
            if (sock < 0)
                return sql_error::FailedToReconnect("PQsocket failed");
            short events = 0;
            if (st == PGRES_POLLING_READING)
                events = POLLIN;
            if (st == PGRES_POLLING_WRITING)
                events = POLLOUT;

            pollfd pfd{sock, events, 0};
            const int remaining = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
            const int pr = poll(&pfd, 1, remaining);
            if (pr <= 0)
                return sql_error::FailedToReconnect("poll failed");
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
                return sql_error::FailedToReconnect("socket error");
        }
    }

    std::expected<void, sql_error> postgres_client::CheckForPollOut(const int& socket) const noexcept {
        while (true) {
            const int flush = PQflush(m_connection.get());
            if (flush < 0) {
                return std::unexpected(sql_error::SocketFailed("failed to flush socket"));
            }
            if (flush == 0)
                return {};

            pollfd pfd = {socket, POLLOUT, 0};
            const int poll_res = poll(&pfd, 1, 5000);
            if (poll_res < 0) {
                return std::unexpected(sql_error::SocketFailed("Pollout event failed"));
            }
            if (poll_res == 0) {
                return std::unexpected(sql_error::SocketFailed("socket timed out"));
            }
            if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                return std::unexpected(sql_error::SocketFailed("Socket failed"));
            }
        }
    }

    std::expected<void, sql_error> postgres_client::CheckForPollIn(const int& socket) const noexcept {
        while (true) {
            pollfd pfd = {socket, POLLIN, 0};
            const int poll_res = poll(&pfd, 1, 5000);
            if (poll_res < 0) {
                return std::unexpected(sql_error::SocketFailed("failed to poll socket"));
            }
            if (poll_res == 0) {
                return std::unexpected(sql_error::SocketFailed("socket timed out"));
            }

            if (PQconsumeInput(m_connection.get()) == 0) {
                const char* err = PQerrorMessage(m_connection.get());
                return std::unexpected(sql_error::BadConnection(err));
            }

            if (PQisBusy(m_connection.get())) {
                continue;
            }
            return {};
        }
    }

    std::expected<result::unique_pg_result, sql_error> postgres_client::ConsumeResult() const noexcept {
        result::unique_pg_result result = nullptr;
        while (PGresult* r = PQgetResult(m_connection.get())) {
            result::unique_pg_result temp(r);
            const auto st = PQresultStatus(temp.get());
            if (st == PGRES_TUPLES_OK || st == PGRES_COMMAND_OK) {
                if (!result)
                    result = std::move(temp);
            } else {
                // Make sure we drain remaining results before returning error (optional but clean)
                std::string msg = PQresultErrorMessage(temp.get());
                sql_error err = sql_error::QueryFailed(PQresultErrorMessage(temp.get()));
                while (PGresult* r2 = PQgetResult(m_connection.get()))
                    PQclear(r2);
                return std::unexpected(std::move(err));
            }
        }
        if (!result) {
            return std::unexpected(sql_error::QueryFailed("no results received"));
        }
        return std::move(result);
    }
}
