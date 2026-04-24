//
// Created by Shinnosuke Kawai on 3/18/26.
//

#pragma once
#include <functional>
#include <libpq-fe.h>
#include <core/memory/intrusive_ptr.h>
#include "error/pg_exception.h"
#include "event/event_loop_executor.h"
#include "result/table.h"

namespace postgres_cxx {
    struct pg_conn_deleter {
        void operator()(PGconn* conn) const noexcept {
            if (conn) {
                PQfinish(conn);
            }
        }
    };
    class pg_transport: public core::ref_counted<pg_transport> {
        using conn_callback_t = std::function<void(std::expected<void, error::pg_exception>)>;
    public:
        pg_transport(): m_exe(smart_ptr::make_intrusive<event_loop_executor>()) {}

        void connect_async(std::string url, conn_callback_t cb) noexcept {

            if (auto init_res = m_exe->init(); !init_res) {
                cb(std::unexpected(std::move(init_res.error())));
                return;
            }

            auto self = this->intrusive_from_this();
            m_exe->post([self, url = std::move(url), cb = std::move(cb)] mutable {
                self->handle_connect(std::move(url), std::move(cb));
            });
        }

        ~pg_transport() noexcept override {
            cleanup();
        }

    private:
        void handle_connect(std::string&& url, conn_callback_t&& cb) noexcept;
        void notify_error_all(error::pg_exception error) noexcept;
        void notify_success_all() noexcept;
        static void on_write_cb(evutil_socket_t fd, short events, void* priv) noexcept;
        static void on_read_cb(evutil_socket_t fd, short events, void* priv) noexcept;
        void poll_connection() noexcept;
        void reset_events() noexcept;
        void on_write_event() noexcept;
        void on_read_event() noexcept;
        void handle_read() noexcept;
        void handle_write() noexcept;
        void cleanup() noexcept;
    private:
        enum class state {
            disconnected,
            connecting,
            connected,
            busy,
            closing
        };

    private:
        state m_state = state::disconnected;
        std::queue<conn_callback_t> m_conn_waters;
        event* m_write_ev = nullptr;
        event* m_read_ev = nullptr;
        std::unique_ptr<PGconn, pg_conn_deleter> m_conn;
        smart_ptr::intrusive_ptr<event_loop_executor> m_exe;
    };
}
