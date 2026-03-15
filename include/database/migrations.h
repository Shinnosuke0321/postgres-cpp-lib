//
// Created by Shinnosuke Kawai on 3/7/26.
//

#pragma once
#include <core/memory/intrusive_ptr.h>
#include <core/error/base_error.h>
#include <database/connection_pool.h>
#include "postgres.h"
#include <filesystem>
#include <fstream>
#include <optional>
#include <variant>

namespace database {

    struct PGMigrationError: Core::BaseError {
        PGMigrationError() = default;
        ~PGMigrationError() override = default;

        explicit PGMigrationError(PostgresErr&& err)
            : m_error(std::move(err)) {}
        explicit PGMigrationError(Core::Database::ConnectionError&& conn_err)
            : m_error(std::move(conn_err)) {}

        explicit operator bool() const noexcept { return m_error.has_value(); }

        std::string to_str() const noexcept override {
            if (!m_error) return {};
            return std::visit([](const auto& e) { return e.to_str(); }, *m_error);
        }
    private:
        using ErrorVariant = std::variant<PostgresErr, Core::Database::ConnectionError>;
        std::optional<ErrorVariant> m_error = std::nullopt;
    };

    inline PGMigrationError Migrate(smart_ptr::intrusive_ptr<Core::Database::ConnectionPool<Postgres>> pool, const std::filesystem::path& path) noexcept {
        std::ifstream file(path);
        if (!file.is_open()) {
            return PGMigrationError(PostgresErr::SqlFileError("Failed to open sql file"));
        }
        std::string line;
        std::string query;
        while (std::getline(file, line)) {
            query += line;
        }
        using PGClient = Core::Database::ConnectionManager<Postgres>;
        auto acquire_result = pool->acquire();
        if (!acquire_result) {
            return PGMigrationError(std::move(acquire_result.error()));
        }
        PGClient& client = acquire_result.value();
        auto future = client->execute(query);
        if (auto result = future.get(); !result) {
            return PGMigrationError(std::move(result.error()));
        }
        return {};
    }
}
