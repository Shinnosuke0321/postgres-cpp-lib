//
// Created by Shinnosuke Kawai on 4/19/25.
//

#pragma once
#include <database/connection.h>
#include <memory>
#include <libpq-fe.h>
#include <utility>
#include <optional>
#include <cstdlib>
#include <sys/poll.h>
#include "postgres_error.h"
#include <expected>
#include <random>
#include <format>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <future>
#include <deque>

namespace Database {
    inline std::optional<std::string> GetDatabaseUrl() {
        char* db_url = std::getenv("POSTGRES_DB_URL");

        if (!db_url || db_url[0] == '\0')
            return std::nullopt;

        std::string url(db_url);
        if (url.back() == '?' || url.back() == '&') {
            std::string conn_str = std::format("{}"
                                           "keepalives=1"
                                           "&keepalives_idle=30"
                                           "&keepalives_interval=10"
                                           "&keepalives_count=5", std::move(url));
            return std::move(conn_str);
        }
        std::string conn_str = std::format("{}"
                                           "?keepalives=1"
                                           "&keepalives_idle=30"
                                           "&keepalives_interval=10"
                                           "&keepalives_count=5", std::move(url));
        return std::move(conn_str);
    }

    struct PGConnDeleter {
        void operator()(PGconn* conn) const noexcept {
            if (conn) {
                PQfinish(conn);
            }
        }
    };
    struct PGResultDeleter {
        void operator()(PGresult* result) const noexcept {
            if (result) {
                PQclear(result);
            }
        }
    };
    using UniquePGConn = std::unique_ptr<PGconn, PGConnDeleter>;
    using UniquePGResult = std::unique_ptr<PGresult, PGResultDeleter>;
    using ResultCallback = std::function<void(UniquePGResult)>;
    using ErrorCallback = std::function<void(const PostgresErr &)>;

    class Postgres final: public Core::Database::IConnection {
    public:
        static std::expected<std::unique_ptr<Postgres>, Core::Database::ConnectionError> ConnectionFactory() noexcept;

        explicit Postgres(std::string&& uri);
        Postgres(std::string&& uri, bool heartbeat_enabled);
        ~Postgres() override;

        Postgres() = delete;
        Postgres(const Postgres&) = delete;
        Postgres& operator=(const Postgres&) = delete;
        Postgres(Postgres&& other) noexcept = delete;
        Postgres& operator=(Postgres&& other) noexcept = delete;

        std::future<std::expected<UniquePGResult, PostgresErr>> execute(std::string_view query, std::vector<std::string>&& params) const;
        template<typename ...Args>
        std::future<std::expected<UniquePGResult, PostgresErr>> execute(std::string_view query, Args&& ...params) const {
            std::size_t n = sizeof...(params);

        }
        void execute_async(std::string_view query,ResultCallback&& callback,ErrorCallback&& err_callback, std::vector<std::string>&& params) const noexcept;
    private:
        struct PGRequest {
            std::string query;
            std::vector<std::string> params;
            ResultCallback on_success;
            ErrorCallback on_error;
        };

        std::expected<void, Core::Database::ConnectionError> connect() noexcept;
        bool is_connected() const noexcept;
        std::optional<PostgresErr> attempt_reconnect(std::chrono::milliseconds timeout) const noexcept;
        std::expected<UniquePGResult, PostgresErr> execute_with_retry(const std::string& query, const std::vector<std::string>& params, std::chrono::milliseconds reconnect_timeout) const noexcept;
        std::expected<UniquePGResult, PostgresErr> execute_query(const std::string& query, const std::vector<std::string>& params) const noexcept;
        void query_worker(const std::stop_token &st) const noexcept;
        std::expected<void, PostgresErr> CheckForPollOut(const int& socket) const noexcept;
        std::expected<void, PostgresErr> CheckForPollIn(const int& socket) const noexcept;
        std::expected<UniquePGResult, PostgresErr> consume_result() const noexcept;

    private:
        std::string m_uri;
        bool m_heartbeat_enabled = false;
        UniquePGConn m_connection = nullptr;
        mutable std::mutex m_mutex;
        mutable std::condition_variable m_cv;
        mutable std::deque<PGRequest> m_requests;
        mutable std::jthread m_worker_thread;
    };
}

