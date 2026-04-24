//
// Created by Shinnosuke Kawai on 4/21/26.
//
#include "postgrescxx/pg_transport.h"

#include "database/connection_error.h"
#include "postgrescxx/result/colunm.h"

namespace postgres_cxx {
    using namespace error;
    using types::ConnectionFailed;
    void pg_transport::handle_connect(std::string &&url, conn_callback_t &&cb) noexcept {
        switch (m_state) {
            case state::busy:
            case state::connected:
                cb({});
                return;
            case state::connecting:
                m_conn_waters.push(std::move(cb));
                return;
            case state::disconnected:
                m_state = state::connecting;
                m_conn_waters.push(std::move(cb));
                m_conn.reset(PQconnectStart(url.c_str()));
                if (!m_conn) {
                    notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, "PQconnectStart failed"));
                    return;
                }
                if (PQstatus(m_conn.get()) == CONNECTION_BAD) {
                    notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, "PQstatus returned CONNECTION_BAD"));
                    return;
                }
                PQsetnonblocking(m_conn.get(), 1);
                poll_connection();
                return;
            case state::closing:
                cb(std::unexpected(CREATE_ERROR(pg_exception, ConnectionFailed, "Connection is closing")));
                break;
        }
    }

    void pg_transport::notify_error_all(pg_exception error) noexcept {
        while (!m_conn_waters.empty()) {
            auto cb = std::move(m_conn_waters.front());
            m_conn_waters.pop();
            cb(std::unexpected(std::move(error)));
        }
        cleanup();
    }

    void pg_transport::cleanup() noexcept {
        std::printf("cleaning up events\n");
        if (m_read_ev) {
            std::printf("deleting read event\n");
            event_del(m_read_ev);
            m_read_ev = nullptr;
        }
        if (m_write_ev) {
            std::printf("deleting write event\n");
            event_del(m_write_ev);
            m_write_ev = nullptr;
        }
        m_state = state::disconnected;
        std::printf("cleaned up\n");
    }

    void pg_transport::poll_connection() noexcept {
        std::printf("poll_connection\n");
        switch (PQconnectPoll(m_conn.get())) {
            case PGRES_POLLING_FAILED: {
                std::println("Polling failed:");
                std::string err_msg = PQerrorMessage(m_conn.get());
                notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, std::move(err_msg)));
                return;
            }
            case PGRES_POLLING_READING: {
                std::printf("Polling reading\n");
                on_read_event();
                return;
            }
            case PGRES_POLLING_WRITING: {
                std::printf("Polling writing\n");
                on_write_event();
                return;
            }
            case PGRES_POLLING_OK: {
                std::printf("Polling ok\n");
                reset_events();
                notify_success_all();
            }
            case PGRES_POLLING_ACTIVE:
                break;
        }
    }

    void pg_transport::reset_events() noexcept {
        std::println("resetting events");
        if (m_read_ev) event_del(m_read_ev);
        if (m_write_ev) event_del(m_write_ev);
        m_read_ev = nullptr;
        m_write_ev = nullptr;
    }

    void pg_transport::notify_success_all() noexcept {
        std::println("successfully conneccted. notifying callbacks");
        m_state = state::connected;
        while (!m_conn_waters.empty()) {
            auto cb = std::move(m_conn_waters.front());
            m_conn_waters.pop();
            cb({});
        }
    }

    void pg_transport::on_read_cb(evutil_socket_t, short, void* priv) noexcept {
        std::printf("on_read_cb\n");
        static_cast<pg_transport*>(priv)->handle_read();
    }
    void pg_transport::on_write_cb(evutil_socket_t, short, void* priv) noexcept {
        std::printf("on_write_cb\n");
        static_cast<pg_transport*>(priv)->handle_write();
    }
    void pg_transport::handle_read() noexcept {
        std::printf("handle_read\n");
        if (m_state == state::connecting) {
            std::printf("state is connecting\n");
            poll_connection();
            return;
        }

        if (!PQconsumeInput(m_conn.get())) {
            std::printf("PQconsumeInput failed\n");
            std::string err_msg = PQerrorMessage(m_conn.get());
            notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, std::move(err_msg)));
            return;
        }
        std::println("checking the server is busy.");
        if (PQisBusy(m_conn.get())) {
            std::println("server is busy. moving to wait for read-readiness");
            m_state = state::busy;
            on_read_event();
            return;
        }
        std::println("server is not busy. calling PQgetResult");
        // result is ready -> call PQgetResult
        result::unique_pg_result result(PQgetResult(m_conn.get()));
        if (!result) {
            notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, "PQgetResult failed"));
            return;
        }
    }
    void pg_transport::handle_write() noexcept {
        std::printf("handle_write\n");

        if (m_state == state::connecting) {
            std::printf("state is connecting. call poll connection\n");
            poll_connection();
            return;
        }

        if (m_state == state::busy) {
            if (const int r = PQflush(m_conn.get()); r == 0) {
                event_del(m_write_ev);
                on_read_event();
            } else if (r < 0) {
                std::string err_msg = PQerrorMessage(m_conn.get());
                notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, std::move(err_msg)));
            } else {
                on_write_event();
            }
        }
    }
    void pg_transport::on_write_event() noexcept {
        std::printf("on_write_event. getting file descriptor\n");
        const int fd = PQsocket(m_conn.get());
        std::printf("got file descriptor: %d\n", fd);
        if (fd < 0) {
            notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, "PQsocket failed: no backend connection is currently open"));
            return;
        }
        m_write_ev = event_new(m_exe->base(), fd, EV_WRITE, on_write_cb, this);
        event_add(m_write_ev, nullptr);
    }

    void pg_transport::on_read_event() noexcept {
        std::printf("on_read_event getting file descripter\n");
        int fd = PQsocket(m_conn.get());
        std::printf("got file descriptor: %d\n", fd);
        if (fd < 0) {
            notify_error_all(CREATE_ERROR(pg_exception, ConnectionFailed, "PQsocket failed: no backend connection is currently open"));
            return;
        }
        m_read_ev = event_new(m_exe->base(), fd, EV_READ, on_read_cb, this);
        event_add(m_read_ev, nullptr);
    }
}
