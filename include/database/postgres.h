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
#include <variant>
#include "internal/type_detail.h"
#include "result/table.h"

namespace database {
    struct PGOptions {
        bool keepalive = true;
        uint32_t keepalive_count = 5;
        uint32_t keepalive_interval = 10;
        uint32_t keepalive_idle = 30;
    };
    inline std::optional<std::string> GetDatabaseUrl(const std::optional<PGOptions> &options = std::nullopt) {
        char* db_url = std::getenv("POSTGRES_DB_URL");

        if (!db_url || db_url[0] == '\0')
            return std::nullopt;

        if (options) {
            std::string body(db_url);
            body += (body.back() == '?' || body.back() == '&') ? "" : "&";
            std::string conn_str = std::format("{}"
                                           "keepalives={}"
                                           "&keepalives_idle={}"
                                           "&keepalives_interval={}"
                                           "&keepalives_count={}",
                                           std::move(body),
                                           std::to_string(options->keepalive),
                                           std::to_string(options->keepalive_idle),
                                           std::to_string(options->keepalive_interval),
                                           std::to_string(options->keepalive_count));
            return std::move(conn_str);
        }
        return db_url;
    }

    struct conn_deleter {
        void operator()(PGconn* conn) const noexcept {
            if (conn) {
                PQfinish(conn);
            }
        }
    };
    using UniquePGConn = std::unique_ptr<PGconn, conn_deleter>;
    using ResultCallback = std::function<void(result::table)>;
    using ErrorCallback = std::function<void(const PostgresErr&)>;

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


        std::expected<void, Core::Database::ConnectionError> connect() noexcept;
        bool is_connected() const noexcept;

        template<typename... Args>
        std::future<std::expected<result::table, PostgresErr>> execute(std::string_view query, Args&& ...params) const {
            constexpr size_t n = sizeof...(params);
            const std::array<SupportedType, n> param_arr = { internal::CreateSingleData(std::forward<Args>(params))... };
            PgParamDetail param_buffer = internal::MakePgParamBuffer(query, param_arr);
            return SendToWorker(std::move(param_buffer));
        }
        template<typename ...Params>
        void execute_async(std::string_view query,ResultCallback&& callback,ErrorCallback&& err_callback, Params&& ...params) const noexcept {
            constexpr size_t SIZE = sizeof...(params);
            const std::array<SupportedType, SIZE> param_arr = { internal::CreateSingleData(std::forward<Params>(params))... };
            PgParamDetail detail = internal::MakePgParamBuffer(query, param_arr);

            PGRequest request{};
            request.detail = std::move(detail);
            request.on_success = std::move(callback);
            request.on_error = std::move(err_callback);
            {
                std::lock_guard sl(m_mutex);
                m_requests.push_back(std::move(request));
            }
            m_cv.notify_one();
        }
    private:
        struct PGRequest {
            PgParamDetail detail;
            ResultCallback on_success;
            ErrorCallback on_error;

            PGRequest() = default;
            explicit PGRequest(PgParamDetail&& detail) noexcept: detail(std::move(detail)) {}
            PGRequest(PGRequest&& other) noexcept
            : detail(std::move(other.detail)),
              on_success(std::move(other.on_success)),
              on_error(std::move(other.on_error)) {
                other.on_success = nullptr;
                other.on_error = nullptr;
            }
            PGRequest& operator=(PGRequest&& other) noexcept {
                if (this != &other) {
                    detail = std::move(other.detail);
                    on_success = std::move(other.on_success);
                    on_error = std::move(other.on_error);
                    other.on_success = nullptr;
                    other.on_error = nullptr;
                }
                return *this;
            }
            PGRequest(const PGRequest&) = delete;
            PGRequest& operator=(const PGRequest&) = delete;
        };
    private:
        std::future<std::expected<result::table, PostgresErr>> SendToWorker(PgParamDetail&& query_detail) const;
        void QueryWorker(const std::stop_token &st) const noexcept;
        std::expected<result::unique_pg_result, PostgresErr> ExecuteWithRetry(const PgParamDetail& param_detail, std::chrono::milliseconds reconnect_timeout) const noexcept;
        std::expected<result::unique_pg_result, PostgresErr> ExecuteQuery(const PgParamDetail& param_detail) const noexcept;

        std::optional<PostgresErr> AttemptReconnect(std::chrono::milliseconds timeout) const noexcept;
        std::expected<void, PostgresErr> CheckForPollOut(const int& socket) const noexcept;
        std::expected<void, PostgresErr> CheckForPollIn(const int& socket) const noexcept;
        std::expected<result::unique_pg_result, PostgresErr> ConsumeResult() const noexcept;

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

