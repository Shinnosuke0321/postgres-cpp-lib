//
// Created by Shinnosuke Kawai on 4/25/26.
//
#include "../include/postgrescxx/actor/query_executor.h"

#include <ostream>

#include "database/connection_error.h"

namespace postgres_cxx {
    using namespace error;
    using types::QueryFailed;

    query_executor::query_executor(query_executor &&other) noexcept
    : m_write_ev(other.m_write_ev),
      m_read_ev(other.m_read_ev),
      m_res_cbs(std::move(other.m_res_cbs)),
      m_ev_ctx(std::move(other.m_ev_ctx)),
      m_postgres_ctx(std::move(other.m_postgres_ctx)) {
        other.m_write_ev = nullptr;
        other.m_read_ev = nullptr;
        other.m_res_cbs = nullptr;
    }

    query_executor& query_executor::operator=(query_executor &&other) noexcept {
        if (this != &other) {
            m_write_ev = other.m_write_ev;
            m_read_ev = other.m_read_ev;
            m_res_cbs = std::move(other.m_res_cbs);
            m_ev_ctx = std::move(other.m_ev_ctx);
            m_postgres_ctx = std::move(other.m_postgres_ctx);
            other.m_write_ev = nullptr;
            other.m_read_ev = nullptr;
            other.m_res_cbs = nullptr;
        }
        return *this;
    }

    query_executor::~query_executor() {
        cleanup();
    }

    void query_executor::cleanup() noexcept {
        reset_events();
    }

    void query_executor::reset_events() noexcept {
        if (m_read_ev) {
            event_free(m_read_ev);
            m_read_ev = nullptr;
        }
        if (m_write_ev) {
            event_free(m_write_ev);
            m_write_ev = nullptr;
        }
    }

    void query_executor::run_query(const std::shared_ptr<pg_param_detail>& detail, on_result_t on_result) noexcept {
        m_res_cbs = std::move(on_result);
        const std::string_view query = detail->command();
        const int n_params = detail->count();
        const char** param_vals = detail->param_values();
        const int* param_lens = detail->param_lengths();
        const int* param_formats = detail->param_formats();

        if (!PQsendQueryParams(m_postgres_ctx->raw_conn(),query.data(),n_params,nullptr,param_vals,param_lens,param_formats,1)) {
            notify_error(CREATE_ERROR(pg_exception, QueryFailed, PQerrorMessage(m_postgres_ctx->raw_conn())));
            return;
        }
        if (const int input = PQflush(m_postgres_ctx->raw_conn()); input == 0) {
            on_read_event();
        } else if (input == 1) {
            on_write_event();
        } else {
            notify_error(CREATE_ERROR(pg_exception, QueryFailed, "PQflush failed"));
        }
    }

    void query_executor::on_read_event() noexcept {
        std::printf("on_read_event\n");
        const int fd = PQsocket(m_postgres_ctx->raw_conn());
        if (fd < 0) {
            notify_error(CREATE_ERROR(pg_exception, QueryFailed, "PQsocket failed: no backend connection is currently open"));
            return;
        }
        if (m_read_ev) {
            event_free(m_read_ev);
            m_read_ev = nullptr;
        }
        m_read_ev = event_new(m_ev_ctx->ev_base(), fd, EV_READ | EV_PERSIST, read_cb, this);
        event_add(m_read_ev, nullptr);
    }

    void query_executor::on_write_event() noexcept {
        const int fd = PQsocket(m_postgres_ctx->raw_conn());
        if (fd < 0) {
            notify_error(CREATE_ERROR(pg_exception, QueryFailed, "PQsocket failed: no backend connection is currently open"));
            return;
        }
        if (m_write_ev) {
            event_free(m_write_ev);
            m_write_ev = nullptr;
        }
        m_write_ev = event_new(m_ev_ctx->ev_base(), fd, EV_WRITE | EV_PERSIST, write_cb, this);
        event_add(m_write_ev, nullptr);
    }

    void query_executor::write_cb(evutil_socket_t, short, void* priv) noexcept {
        static_cast<query_executor*>(priv)->handle_write();
    }

    void query_executor::handle_write() noexcept {
        if (const int flushed = PQflush(m_postgres_ctx->raw_conn()); flushed == 1) {
            on_write_event();
        } else if (flushed == 0) {
            on_read_event();
        } else {
            notify_error(CREATE_ERROR(pg_exception, QueryFailed, PQerrorMessage(m_postgres_ctx->raw_conn())));
        }
    }

    void query_executor::read_cb(evutil_socket_t, short, void* priv) noexcept {
        static_cast<query_executor*>(priv)->handle_read();
    }

    void query_executor::handle_read() noexcept {
        if (!PQconsumeInput(m_postgres_ctx->raw_conn())) {
            notify_error(CREATE_ERROR(pg_exception, QueryFailed, PQerrorMessage(m_postgres_ctx->raw_conn())));
            return;
        }
        if (PQisBusy(m_postgres_ctx->raw_conn())) {
            on_read_event();
            return;
        }
        process_results();
    }

    void query_executor::process_results() noexcept {
        while(result::unique_pg_result res{PQgetResult(m_postgres_ctx->raw_conn())}) {
            const ExecStatusType status = PQresultStatus(res.get());
            if (status == PGRES_TUPLES_OK || status == PGRES_COMMAND_OK) {
                m_res_cbs(result::table(std::move(res)));
            } else {
                std::string err_msg = PQresultErrorMessage(res.get());
                std::erase(err_msg, '\n');
                notify_error(CREATE_ERROR(pg_exception, QueryFailed, std::move(err_msg)));
            }
        }
        prepare_next_query();
    }

    void query_executor::notify_error(pg_exception error) const noexcept {
       m_res_cbs(std::unexpected(std::move(error)));
    }

    void query_executor::prepare_next_query() noexcept {
        reset_events();
        m_res_cbs = nullptr;
    }
}
