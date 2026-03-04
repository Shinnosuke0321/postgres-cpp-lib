//
// Created by Shinnosuke Kawai on 4/19/25.
//
#include "database/postgres.h"


namespace Database {
    std::expected<std::unique_ptr<Postgres>, Core::Database::ConnectionError> Postgres::ConnectionFactory() noexcept {
        using Core::Database::ConnectionError;
        std::optional<std::string> uri = GetDatabaseUrl();
        if (!uri)
            return std::unexpected(ConnectionError::MissingConfig("Postgres URI not provided"));
        auto conn = std::make_unique<Postgres>(std::move(*uri), true);
        if (std::expected<void, ConnectionError> result = conn->connect(); !result)
            return std::unexpected(result.error());
        return conn;
    }

    Postgres::Postgres(std::string&& uri)
    : m_uri(std::move(uri))
    {}

    Postgres::Postgres(std::string&& uri, const bool heartbeat_enabled)
    : m_uri(std::move(uri)),
      m_heartbeat_enabled(heartbeat_enabled)
    {}

    Postgres::~Postgres() {
        m_worker_thread.request_stop();
        m_cv.notify_all();
        if (m_worker_thread.joinable())
            m_worker_thread.join();
    }
    std::expected<void, Core::Database::ConnectionError> Postgres::connect() noexcept {
        using Core::Database::ConnectionError;
        PGconn* raw_conn = PQconnectdb(m_uri.c_str());
        if (!raw_conn) {
            return std::unexpected(ConnectionError::ConnectionFailed("Postgres connection failed"));
        }
        UniquePGConn unique_conn(raw_conn);
        if (PQstatus(unique_conn.get()) != CONNECTION_OK) {
            return std::unexpected(ConnectionError::ConnectionFailed(PQerrorMessage(unique_conn.get())));
        }
        if (PQsetnonblocking(unique_conn.get(), 1) != 0) {
            return std::unexpected(ConnectionError::SocketFailed(PQerrorMessage(unique_conn.get())));
        }
        m_connection = std::move(unique_conn);
        m_worker_thread = std::jthread([this](const std::stop_token &stop_token) mutable {QueryWorker(stop_token);});
        return {};
    }

    bool Postgres::is_connected() const noexcept {
        return m_connection.get() && PQstatus(m_connection.get()) == CONNECTION_OK;
    }
}