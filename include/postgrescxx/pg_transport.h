//
// Created by Shinnosuke Kawai on 3/18/26.
//

#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <core/memory/intrusive_ptr.h>
#include "transaction.h"
#include "error/pg_exception.h"
#include "event/event_loop_executor.h"
#include "internal/query_detail.h"
#include "actor/query_executor.h"
#include "actor/pg_connector.h"

namespace postgres_cxx {
    class pg_transport: public core::ref_counted<pg_transport> {
        using conn_callback_t = std::function<void(std::expected<void, error::pg_exception>)>;
        using query_callback_t = std::function<void(std::expected<result::table, error::pg_exception>)>;
    public:
        static std::expected<smart_ptr::intrusive_ptr<pg_transport>, error::pg_exception> make_transport(std::string url) noexcept {
            auto event_loop = event_loop_executor::run_loop();
            if (!event_loop)
                return std::unexpected(std::move(event_loop.error()));
            auto& [ev_loop, ev_ctx] = event_loop.value();
            pg_connector connector{std::move(url), ev_ctx->intrusive_from_this()};
            query_executor executor{ev_ctx->intrusive_from_this()};
            return smart_ptr::intrusive_ptr(new pg_transport(std::move(ev_loop), std::move(connector), std::move(executor)));
        }
    public:
        void connect_async(conn_callback_t cb) noexcept;
        bool check_connection() noexcept;
        void send_query_async(pg_param_detail detail, query_callback_t&& query_cb) noexcept;
        void make_transaction(std::function<void(std::expected<transaction, error::pg_exception>)>&& on_transaction_created) noexcept;
        ~pg_transport() noexcept override = default;
    private:
        explicit pg_transport(smart_ptr::intrusive_ptr<event_loop_executor> ev_loop, pg_connector connector, query_executor executor);

    private:
        smart_ptr::intrusive_ptr<event_loop_executor> m_exe;
        pg_connector m_connector;
        query_executor m_query_executor;
    };
}
