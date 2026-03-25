//
// Created by Shinnosuke Kawai on 4/19/25.
//

#pragma once
#include <database/connection.h>
#include <memory>
#include <libpq-fe.h>
#include <utility>
#include <optional>
#include <print>
#include <sys/poll.h>
#include <random>
#include <format>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <deque>
#include <functional>
#include "transaction.h"

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
    using unique_pg_conn = std::unique_ptr<PGconn, conn_deleter>;

    class postgres_client : public Core::Database::IConnection, public query_executor {
    public:
        explicit postgres_client(std::string&& uri, std::size_t num_cb_threads = 2);
        postgres_client(std::string&& uri, bool heartbeat_enabled, std::size_t num_cb_threads = 2);
        ~postgres_client() override;

        postgres_client() = delete;
        postgres_client(const postgres_client&) = delete;
        postgres_client& operator=(const postgres_client&) = delete;
        postgres_client(postgres_client&& other) noexcept = delete;
        postgres_client& operator=(postgres_client&& other) noexcept = delete;

        std::expected<void, Core::Database::ConnectionError> connect() noexcept;
        bool is_connected() const noexcept;

        std::shared_ptr<transaction> create_transaction();

        template<typename... Args>
        std::future<std::expected<result::table, sql_error>> execute(std::string_view query, Args&& ...params) const {
            constexpr size_t n = sizeof...(params);
            const std::array<supported_type, n> param_arr = { internal::CreateSingleData(std::forward<Args>(params))... };
            pg_param_detail param_buffer = internal::MakePgParamBuffer(query, param_arr);
            return SendToWorker(std::move(param_buffer));
        }

        template<typename... Params>
        void execute_async(std::string_view query, result_callback callback, error_callback err_callback, Params&& ...params) const noexcept {
            constexpr size_t SIZE = sizeof...(params);
            const std::array<supported_type, SIZE> param_arr = { internal::CreateSingleData(std::forward<Params>(params))... };
            EnqueueAsync(internal::MakePgParamBuffer(query, param_arr), std::move(callback), std::move(err_callback));
        }

    private:
        struct query_request {
            pg_param_detail detail;
            result_callback on_success;
            error_callback on_error;
            // When true the DB worker calls on_success/on_error directly (execute() path).
            // When false they are dispatched through the callback pool (execute_async() path).
            bool direct_callback = false;

            query_request() = default;
            explicit query_request(pg_param_detail&& detail) noexcept: detail(std::move(detail)) {}
            query_request(query_request&& other) noexcept
            : detail(std::move(other.detail)),
              on_success(std::move(other.on_success)),
              on_error(std::move(other.on_error)) {
                other.on_success = nullptr;
                other.on_error = nullptr;
            }
            query_request& operator=(query_request&& other) noexcept {
                if (this != &other) {
                    detail = std::move(other.detail);
                    on_success = std::move(other.on_success);
                    on_error = std::move(other.on_error);
                    other.on_success = nullptr;
                    other.on_error = nullptr;
                }
                return *this;
            }
            query_request(const query_request&) = delete;
            query_request& operator=(const query_request&) = delete;
        };

    private:
        std::future<std::expected<result::table, sql_error>> SendToWorker(pg_param_detail&&) const override;
        void EnqueueAsync(pg_param_detail&&, result_callback&&, error_callback&&) const noexcept override;
        void QueryWorker(const std::stop_token &st) const noexcept;
        void PostCallback(std::function<void()> task) const noexcept;
        void CallbackWorker(const std::stop_token &st) const noexcept;
        std::expected<result::unique_pg_result, sql_error> ExecuteWithRetry(const pg_param_detail& param_detail, std::chrono::milliseconds reconnect_timeout) const noexcept;
        std::expected<result::unique_pg_result, sql_error> ExecuteQuery(const pg_param_detail& param_detail) const noexcept;

        std::optional<sql_error> AttemptReconnect(std::chrono::milliseconds timeout) const noexcept;
        std::expected<void, sql_error> CheckForPollOut(const int& socket) const noexcept;
        std::expected<void, sql_error> CheckForPollIn(const int& socket) const noexcept;
        std::expected<result::unique_pg_result, sql_error> ConsumeResult() const noexcept;

    private:
        friend class transaction;
        std::string m_uri;
        bool m_heartbeat_enabled = false;
        unique_pg_conn m_connection = nullptr;
        mutable std::mutex m_mutex;
        mutable std::condition_variable m_cv;
        mutable std::deque<query_request> m_requests;
        mutable std::jthread m_worker_thread;

        std::size_t m_num_cb_threads;
        mutable std::mutex m_cb_mutex;
        mutable std::condition_variable m_cb_cv;
        mutable std::deque<std::function<void()>> m_cb_queue;
        mutable std::vector<std::jthread> m_cb_workers;
    };
}
