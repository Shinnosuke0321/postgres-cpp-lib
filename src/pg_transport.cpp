//
// Created by Shinnosuke Kawai on 4/21/26.
//
#include "postgrescxx/pg_transport.h"

namespace postgres_cxx {
    using namespace error;
    using types::ConnectionFailed, types::QueryFailed;

    pg_transport::pg_transport(smart_ptr::intrusive_ptr<event_loop_executor> ev_loop, pg_connector connector, query_executor executor)
    : m_exe(std::move(ev_loop)),
      m_connector(std::move(connector)),
      m_query_executor(std::move(executor)) {};

    void pg_transport::connect_async(conn_callback_t cb) noexcept {
        auto self = this->intrusive_from_this();
        m_exe->post([self, cb = std::move(cb)] mutable {
            self->m_connector.start([self, cb = std::move(cb)](pg_connector::connection_result res) mutable  {
                if (res) {
                    self->m_query_executor.set_ctx(std::move(*res));
                    cb({});
                } else {
                    cb(std::unexpected(std::move(res.error())));
                }
            });
        });
    }

    bool pg_transport::check_connection() noexcept {
        return m_connector.is_connected();
    }

    void pg_transport::send_query_async(pg_param_detail detail, query_callback_t &&query_cb) noexcept {
        auto self = this->intrusive_from_this();
        m_exe->post([self, detail = std::move(detail), query_cb = std::move(query_cb)] mutable {
            if (!self->m_connector.is_connected()) {
                query_cb(MAKE_UNEXPECTED_ERROR(pg_exception, QueryFailed, "not connected"));
                return;
            }
            self->m_query_executor.run_query(std::move(detail),std::move(query_cb));
        });
    }
}
