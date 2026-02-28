//
// Created by Shinnosuke Kawai on 4/19/25.
//
#include "postgres.h"


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

    std::future<std::expected<UniquePGResult, PostgresErr>> Postgres::execute(const std::string_view query, std::vector<std::string>&& params) const {
        using Result = std::expected<UniquePGResult, PostgresErr>;
        auto prom = std::make_shared<std::promise<Result>>();
        auto future = prom->get_future();
        PGRequest request{};
        request.query = std::string(query);
        request.params = std::move(params);
        request.on_success = [prom](UniquePGResult reply) {
            try {
                prom->set_value(std::move(reply));
            } catch (...) {}
        };
        request.on_error = [prom](const PostgresErr& err) {
            try {
                prom->set_value(std::unexpected(err));
            } catch (...) {}
        };
        {
            std::lock_guard sl(m_mutex);
            m_requests.push_back(std::move(request));
        }
        m_cv.notify_one();
        return future;
    }
    void Postgres::execute_async(const std::string_view query, ResultCallback &&callback, ErrorCallback &&err_callback, std::vector<std::string>&& params) const noexcept
    {
        PGRequest request{};
        request.query = std::string(query);
        request.params = std::move(params);
        request.on_success = std::move(callback);
        request.on_error = std::move(err_callback);
        {
            std::lock_guard sl(m_mutex);
            m_requests.push_back(std::move(request));
        }
        m_cv.notify_one();
    }
}