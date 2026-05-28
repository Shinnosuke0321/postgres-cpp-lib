//
// Created by Shinnosuke Kawai on 4/25/26.
//
#pragma once
#include <functional>
#include <expected>
#include <memory>
#include "io_tracker.h"
#include "core/memory/intrusive_ptr.h"
#include "postgrescxx/result/table.h"
#include "postgrescxx/error/pg_exception.h"
#include "postgrescxx/internal/query_detail.h"
#include "postgrescxx/context/event_context.h"
#include "postgrescxx/context/postgres_ctx.h"

namespace postgres_cxx {
    class query_executor: public io_tracker {
        using on_result_t = std::function<void(std::expected<result::table, error::pg_exception>)>;
    public:
        void run_query(pg_param_detail detail, on_result_t on_result) noexcept;
        void set_ctx(smart_ptr::intrusive_ptr<postgres_ctx> ctx) noexcept {
            m_postgres_ctx = std::move(ctx);
        }

    public:
        explicit query_executor(smart_ptr::intrusive_ptr<event_context> ev_ctx): m_ev_ctx(std::move(ev_ctx)) {};
        ~query_executor() override;
    public:
        COPY_SEMANTICS(query_executor, delete);
        query_executor(query_executor&& other) noexcept;
        query_executor& operator=(query_executor&& other) noexcept;

    private:
        static void write_cb(evutil_socket_t, short, void* priv) noexcept;
        static void read_cb(evutil_socket_t, short, void* priv) noexcept;
        void on_write_event() noexcept override;
        void on_read_event() noexcept override;
        void handle_read() noexcept override;
        void handle_write() noexcept override;
        void reset_events() noexcept override;
        void cleanup() noexcept;
        void process_results() noexcept;
        void prepare_next_query() noexcept;
        void notify_error(error::pg_exception error) const noexcept;
    private:
        event* m_write_ev = nullptr;
        event* m_read_ev = nullptr;
        on_result_t m_res_cbs = nullptr;
        smart_ptr::intrusive_ptr<event_context> m_ev_ctx;
        smart_ptr::intrusive_ptr<postgres_ctx> m_postgres_ctx;
    };
}
