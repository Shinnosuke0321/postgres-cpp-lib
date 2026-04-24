//
// Created by Shinnosuke Kawai on 4/19/25.
//

#pragma once
#include <memory>
#include <libpq-fe.h>
#include <list>
#include <utility>
#include <optional>
#include <print>
#include "core/memory/intrusive_ptr.h"
#include "database/connection.h"
#include "internal/type_detail.h"
#include "result/table.h"
#ifdef _WIN32
#include <winsock2.h>
#else
#endif
#include <random>
#include <format>
#include <future>
#include "error/pg_exception.h"
#include "pg_transport.h"

namespace postgres_cxx {
    struct options {
        bool keepalive = true;
        uint32_t keepalive_count = 5;
        uint32_t keepalive_interval = 10;
        uint32_t keepalive_idle = 30;
    };
    inline std::optional<std::string> GetDatabaseUrl(const std::optional<options> &options = std::nullopt) {
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

    class client : public database::IConnection, public core::ref_counted<client> {
    public:
        explicit client(std::string uri): m_uri(std::move(uri)), m_transport_ptr(smart_ptr::make_intrusive<pg_transport>()) {};
        ~client() override = default;

        client() = default;
        client(const client&) = delete;
        client& operator=(const client&) = delete;
        client(client&& other) noexcept = delete;
        client& operator=(client&& other) noexcept = delete;

        std::expected<void, core::error::exception> connect() const noexcept;
        bool is_connected() const noexcept;

        // std::shared_ptr<transaction> create_transaction();

        template<typename... Args>
        std::future<std::expected<result::table, error::pg_exception>> execute(std::string_view query, Args&& ...params) const {
            pg_param_detail pg_param_buffer = internal::MakePgParamBuffer(query, std::forward_as_tuple(std::forward<Args>(params)...));
        }

        // template<typename... Params>
        // void execute_async(std::string_view query, result_callback callback, error_callback err_callback, Params&& ...params) const noexcept {
        //
        // }

    private:
        std::string m_uri;
        smart_ptr::intrusive_ptr<pg_transport> m_transport_ptr;
    };
}
