//
// Created by Shinnosuke Kawai on 4/25/26.
//

#pragma once
#include "io_tracker.h"
#include <event2/event.h>
#include "postgrescxx/error/pg_exception.h"
#include <core/memory/intrusive_ptr.h>
#include <expected>
#include <functional>
#include <mutex>
#include <queue>
#include "postgrescxx/context/event_context.h"
#include "postgrescxx/context/postgres_ctx.h"

namespace postgres_cxx {
    class pg_connector: public io_tracker {
    public:
        using connection_result = std::expected<smart_ptr::intrusive_ptr<postgres_ctx>, error::pg_exception>;
        using conn_callback_t = std::function<void(connection_result)>;
    public:
        void start(conn_callback_t &&cb) noexcept;
        void set_reconnect_handler(conn_callback_t cb) noexcept;
        bool is_connected() noexcept;
    public:
        explicit pg_connector(std::string url, smart_ptr::intrusive_ptr<event_context> ev_ctx)
        : m_url(std::move(url)),
          m_event_ctx(std::move(ev_ctx)) {}
        COPY_SEMANTICS(pg_connector, delete);
        pg_connector& operator=(pg_connector&& other) noexcept;
        pg_connector(pg_connector&& other) noexcept;
        ~pg_connector() override = default;
    private:
        void on_read_event() noexcept override;
        void on_write_event() noexcept override;
        void handle_read() noexcept override;
        void handle_write() noexcept override;
        void reset_events() noexcept override;
    private:
        static void on_write_cb(evutil_socket_t fd, short events, void* priv) noexcept;
        static void on_read_cb(evutil_socket_t fd, short events, void* priv) noexcept;
        void notify_error_all(const error::pg_exception& error) noexcept;
        void notify_success_all() noexcept;
        void poll_connection() noexcept;
        void reconnect() noexcept;
        void cleanup() noexcept;
    private:
        enum class state {
            disconnected, connecting, connected, closing
        };

    private:
        std::mutex m_mutex;
        std::string m_url;
        event* m_write_ev = nullptr;
        event* m_read_ev = nullptr;
        state m_state = state::disconnected;
        conn_callback_t m_reconnect_cb;
        std::queue<conn_callback_t> m_conn_waters;
        smart_ptr::intrusive_ptr<event_context> m_event_ctx;
        smart_ptr::intrusive_ptr<postgres_ctx> m_postgres_ctx;
    };
}
