//
// Created by Shinnosuke Kawai on 3/7/26.
//

#pragma once
#include <core/memory/intrusive_ptr.h>
#include <database/connection_pool.h>
#include "postgres.h"
#include <filesystem>
#include <fstream>
#include <variant>

namespace Database {

    struct PGMigrationError {
        using ErrorVariant = std::variant<std::monostate, PostgresErr, Core::Database::ConnectionError>;

        PGMigrationError() = default;
        explicit PGMigrationError(PostgresErr&& err) : error_(std::move(err)) {}
        explicit PGMigrationError(Core::Database::ConnectionError&& conn_err) : error_(std::move(conn_err)) {}

        explicit operator bool() const noexcept { return !std::holds_alternative<std::monostate>(error_); }

        std::string to_str() noexcept {
            return std::visit([]<typename T0>(T0& e) -> std::string {
                if constexpr (std::is_same_v<std::decay_t<T0>, std::monostate>) {
                    return {};
                } else {
                    return e.to_str();
                }
            }, error_);
        }
    private:
        ErrorVariant error_;
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
