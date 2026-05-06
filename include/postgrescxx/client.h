//
// Created by Shinnosuke Kawai on 4/19/25.
//

#pragma once
#include <memory>
#include <list>
#include <utility>
#include <optional>
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
    class client: public postgres_cxx::IConnection, public core::ref_counted<client> {
    public:
        explicit client(std::string url) {
            if (auto res = pg_transport::make_transport(std::move(url)); !res) {
                m_init_err.emplace(std::move(res.error()));
            } else {
                m_transport_ptr = std::move(*res);
            }
        };
        client() = default;
        ~client() override = default;
        std::expected<void, core::error::exception> connect() const noexcept;
        bool is_connected() const noexcept;

        template<typename... Args>
        std::future<std::expected<result::table, error::pg_exception>> execute(const std::string_view query, Args&& ...params) const {
            auto promise = std::make_shared<std::promise<std::expected<result::table, error::pg_exception>>>();
            auto fut = promise->get_future();
            std::array<supported_type, sizeof...(Args)> param_array{std::forward<Args>(params)...};
            m_transport_ptr->send_query_async(std::make_shared<pg_param_detail>(query, param_array), [promise](std::expected<result::table, error::pg_exception> res) {
                if (res) {
                    promise->set_value(std::move(res.value()));
                } else {
                    promise->set_value(std::unexpected(std::move(res.error())));
                }
            });
            return fut;
        }

        template<typename... Args>
        void execute(const std::string_view query, std::function<void(result::table)>&& success_cb, std::function<void(error::pg_exception)>&& error_cb, Args&& ...params) const {
            std::array<supported_type, sizeof...(Args)> param_array{std::forward<Args>(params)...};
            m_transport_ptr->send_query_async(std::make_shared<pg_param_detail>(query, param_array), [success_cb = std::move(success_cb), error_cb = std::move(error_cb)](std::expected<result::table, error::pg_exception> res) {
                if (res) {
                    success_cb(std::move(res.value()));
                } else {
                    error_cb(std::move(res.error()));
                }
            });
        }

    public:
        COPY_SEMANTICS(client, delete);
        client(client&& other) noexcept = delete;
        client& operator=(client&& other) noexcept = delete;

    private:
        std::optional<error::pg_exception> m_init_err = std::nullopt;
        smart_ptr::intrusive_ptr<pg_transport> m_transport_ptr;
    };
}
