//
// Created by Shinnosuke Kawai on 4/19/25.
//
#include "database/postgres_client.h"
#include "database/transaction.h"

namespace database {
    std::shared_ptr<transaction> postgres_client::create_transaction() {
        auto txn = std::make_shared<transaction>(*this);
        std::future<std::expected<result::table, sql_error>> begin_future = txn->execute("BEGIN");
        if (const std::expected<result::table, sql_error> result = begin_future.get(); !result) {
            return nullptr;
        }
        return txn;
    }

    postgres_client::postgres_client(std::string&& uri, const std::size_t num_cb_threads)
    : m_uri(std::move(uri)),
      m_num_cb_threads(num_cb_threads)
    {}

    postgres_client::postgres_client(std::string&& uri, const bool heartbeat_enabled, const std::size_t num_cb_threads)
    : m_uri(std::move(uri)),
      m_heartbeat_enabled(heartbeat_enabled),
      m_num_cb_threads(num_cb_threads)
    {}

    postgres_client::~postgres_client() {
        // Stop the DB worker first — it may still post to the callback queue during drain.
        m_worker_thread.request_stop();
        m_cv.notify_all();
        if (m_worker_thread.joinable())
            m_worker_thread.join();

        // Stop all callback workers — all pending callbacks are now in the queue.
        {
            std::lock_guard lk(m_temp_cb_mutex);
            reap_overflow_threads(m_overflow_cb_threads);
            m_overflow_cb_threads.clear();
        }
        for (auto& w : m_cb_workers)
            w.request_stop();
        m_cb_cv.notify_all();
        for (auto& w : m_cb_workers)
            if (w.joinable()) w.join();
    }

    std::expected<void, Core::Database::ConnectionError> postgres_client::connect() noexcept {
        using Core::Database::ConnectionError;
        PGconn* raw_conn = PQconnectdb(m_uri.c_str());
        if (!raw_conn) {
            return std::unexpected(ConnectionError::ConnectionFailed("Postgres connection failed"));
        }
        unique_pg_conn unique_conn(raw_conn);
        if (PQstatus(unique_conn.get()) != CONNECTION_OK) {
            return std::unexpected(ConnectionError::ConnectionFailed(PQerrorMessage(unique_conn.get())));
        }
        if (PQsetnonblocking(unique_conn.get(), 1) != 0) {
            return std::unexpected(ConnectionError::SocketFailed(PQerrorMessage(unique_conn.get())));
        }
        m_connection = std::move(unique_conn);
        m_worker_thread = std::jthread([this](const std::stop_token& st) { QueryWorker(st); });
        m_cb_workers.reserve(m_num_cb_threads);
        for (std::size_t i = 0; i < m_num_cb_threads; ++i)
            m_cb_workers.emplace_back([this](const std::stop_token& st) { CallbackWorker(st); });
        return {};
    }

    bool postgres_client::is_connected() const noexcept {
        return m_connection.get() && PQstatus(m_connection.get()) == CONNECTION_OK;
    }
}
