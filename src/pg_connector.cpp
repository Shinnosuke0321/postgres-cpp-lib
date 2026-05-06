//
// Created by Shinnosuke Kawai on 4/25/26.
//

#include "postgrescxx/actor/pg_connector.h"

namespace postgres_cxx {
    using namespace error;
    using types::ConnectionFailed, types::QueryFailed;

    pg_connector& pg_connector::operator=(pg_connector &&other) noexcept {
        std::lock_guard lock(m_mutex);
        if (this != &other) {
            m_url = std::move(other.m_url);
            m_event_ctx = std::move(other.m_event_ctx);
            m_postgres_ctx = std::move(other.m_postgres_ctx);
            m_state = other.m_state;
            m_reconnect_cb = std::move(other.m_reconnect_cb);
            m_conn_waters = std::move(other.m_conn_waters);
            m_read_ev = other.m_read_ev;
            m_write_ev = other.m_write_ev;
            other.m_read_ev = nullptr;
            other.m_write_ev = nullptr;
        }
        return *this;
    }

    pg_connector::pg_connector(pg_connector &&other) noexcept
    : m_url(std::move(other.m_url)),
      m_write_ev(other.m_write_ev),
      m_read_ev(other.m_read_ev),
      m_state(other.m_state),
      m_reconnect_cb(std::move(other.m_reconnect_cb)),
      m_conn_waters(std::move(other.m_conn_waters)),
      m_event_ctx(std::move(other.m_event_ctx)),
      m_postgres_ctx(std::move(other.m_postgres_ctx))
    {
        other.m_read_ev = nullptr;
        other.m_write_ev = nullptr;
    }

    bool pg_connector::is_connected() noexcept {
        std::unique_lock lock(m_mutex);
        return m_state == state::connected && m_postgres_ctx->raw_conn() && PQstatus(m_postgres_ctx->raw_conn()) == CONNECTION_OK;
    }
    void pg_connector::set_reconnect_handler(conn_callback_t cb) noexcept {
        std::unique_lock lock(m_mutex);
        if (!m_reconnect_cb)
            m_reconnect_cb = std::move(cb);
    }

    void pg_connector::start(conn_callback_t &&cb) noexcept {
        switch (m_state) {
            case state::connected:
                cb(m_postgres_ctx);
                return;
            case state::connecting:
                m_conn_waters.push(std::move(cb));
                return;
            case state::disconnected:
                m_state = state::connecting;
                m_conn_waters.push(std::move(cb));
                m_postgres_ctx = smart_ptr::make_intrusive<postgres_ctx>(PQconnectStart(m_url.c_str()));
                if (!m_postgres_ctx->raw_conn()) {
                    notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, "PQconnectStart failed"));
                    return;
                }
                if (PQstatus(m_postgres_ctx->raw_conn()) == CONNECTION_BAD) {
                    notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, "PQstatus returned CONNECTION_BAD"));
                    return;
                }
                PQsetnonblocking(m_postgres_ctx->raw_conn(), 1);
                poll_connection();
                return;
            case state::closing:
                cb(std::unexpected(CREATE_ERROR(pg_exception, ConnectionFailed, "Connection is closing")));
                break;
        }
    }

    void pg_connector::poll_connection() noexcept {
        switch (PQconnectPoll(m_postgres_ctx->raw_conn())) {
            case PGRES_POLLING_FAILED: {
                cleanup();
                std::string err_msg = PQerrorMessage(m_postgres_ctx->raw_conn());
                notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, std::move(err_msg)));
                return;
            }
            case PGRES_POLLING_READING: {
                on_read_event();
                return;
            }
            case PGRES_POLLING_WRITING: {
                on_write_event();
                return;
            }
            case PGRES_POLLING_OK: {
                reset_events();
                notify_success_all();
                return;
            }
            case PGRES_POLLING_ACTIVE:
                break;
        }
    }

    void pg_connector::on_write_event() noexcept {
        const int fd = PQsocket(m_postgres_ctx->raw_conn());
        if (fd < 0) {
            notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, "PQsocket failed: no backend connection is currently open"));
            return;
        }
        if (m_write_ev) {
            event_free(m_write_ev);
            m_write_ev = nullptr;
        }
        m_write_ev = event_new(m_event_ctx->ev_base(), fd, EV_WRITE, on_write_cb, this);
        event_add(m_write_ev, nullptr);
    }

    void pg_connector::on_read_event() noexcept {
        const int fd = PQsocket(m_postgres_ctx->raw_conn());
        if (fd < 0) {
            notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, "PQsocket failed: no backend connection is currently open"));
            return;
        }
        if (m_read_ev) {
            event_free(m_read_ev);
            m_read_ev = nullptr;
        }
        m_read_ev = event_new(m_event_ctx->ev_base(), fd, EV_READ | EV_PERSIST, on_read_cb, this);
        event_add(m_read_ev, nullptr);
    }
    void pg_connector::on_read_cb(evutil_socket_t, short, void* priv) noexcept {
        static_cast<pg_connector*>(priv)->handle_read();
    }
    void pg_connector::on_write_cb(evutil_socket_t, short, void* priv) noexcept {
        static_cast<pg_connector*>(priv)->handle_write();
    }
    void pg_connector::handle_read() noexcept {
        if (m_state == state::connecting) {
            poll_connection();
            return;
        }
        if (m_state == state::connected) {
            PQconsumeInput(m_postgres_ctx->raw_conn());
            if (PQstatus(m_postgres_ctx->raw_conn()) == CONNECTION_BAD) {
                reconnect();
            }
        }
    }
    void pg_connector::handle_write() noexcept {
        poll_connection();
    }

    void pg_connector::reconnect() noexcept {
        cleanup();
        m_postgres_ctx.reset(new postgres_ctx(PQconnectStart(m_url.c_str())));
        if (!m_postgres_ctx->raw_conn()) {
            if (m_reconnect_cb)
                m_reconnect_cb(std::unexpected(CREATE_ERROR(pg_exception, ConnectionFailed, "PQconnectStart failed on reconnect")));
            return;
        }
        if (PQstatus(m_postgres_ctx->raw_conn()) == CONNECTION_BAD) {
            if (m_reconnect_cb)
                m_reconnect_cb(std::unexpected(CREATE_ERROR(pg_exception, ConnectionFailed, "PQstatus returned CONNECTION_BAD on reconnect")));
            return;
        }
        m_state = state::connecting;
        if (m_reconnect_cb)
            m_conn_waters.push(m_reconnect_cb);
        PQsetnonblocking(m_postgres_ctx->raw_conn(), 1);
        poll_connection();
    }

    void pg_connector::notify_success_all() noexcept {
        m_state = state::connected;
        reset_events();
        on_read_event();
        while (!m_conn_waters.empty()) {
            auto cb = std::move(m_conn_waters.front());
            m_conn_waters.pop();
            cb(m_postgres_ctx);
        }
    }
    void pg_connector::notify_error_all(const pg_exception& error) noexcept {
        cleanup();
        while (!m_conn_waters.empty()) {
            auto cb = std::move(m_conn_waters.front());
            m_conn_waters.pop();
            cb(std::unexpected(error));
        }
    }

    void pg_connector::cleanup() noexcept {
        reset_events();
        m_state = state::disconnected;
    }

    void pg_connector::reset_events() noexcept {
        if (m_read_ev) event_free(m_read_ev);
        if (m_write_ev) event_free(m_write_ev);
        m_read_ev = nullptr;
        m_write_ev = nullptr;
    }
}
