//
// Created by Shinnosuke Kawai on 3/7/26.
//

#pragma once
#include <core/memory/intrusive_ptr.h>
#include <core/error/base_error.h>
#include <database/connection_pool.h>
#include "postgres_client.h"
#include <filesystem>
#include <fstream>
#include <optional>
#include <variant>
#include "internal/sql_parser.h"

namespace database {

    struct migration_error: Core::BaseError {
        migration_error() = default;
        ~migration_error() override = default;

        explicit migration_error(sql_error&& err)
            : m_error(std::move(err)) {}
        explicit migration_error(Core::Database::ConnectionError&& conn_err)
            : m_error(std::move(conn_err)) {}

        explicit operator bool() const noexcept { return m_error.has_value(); }

        [[nodiscard]] std::string to_str() const noexcept override {
            if (!m_error) return {};
            return std::visit([](const auto& e) { return e.to_str(); }, *m_error);
        }
    private:
        using ErrorVariant = std::variant<sql_error, Core::Database::ConnectionError>;
        std::optional<ErrorVariant> m_error = std::nullopt;
    };

    inline migration_error Migrate(smart_ptr::intrusive_ptr<Core::Database::ConnectionPool<postgres_client>> pool, const std::filesystem::path& path) noexcept {
        std::ifstream file(path);
        if (!file.is_open()) {
            return migration_error(sql_error::SqlFileError("Failed to open sql file"));
        }
        const std::string sql(std::istreambuf_iterator<char>(file), {});
        const auto statements = internal::ParseStatements(sql);

        using PGClient = Core::Database::ConnectionManager<postgres_client>;
        auto acquire_result = pool->acquire();
        if (!acquire_result) {
            return migration_error(std::move(acquire_result.error()));
        }
        PGClient& client = acquire_result.value();

        for (const auto& stmt : statements) {
            auto future = client->execute(stmt);
            if (auto result = future.get(); !result) {
                return migration_error(std::move(result.error()));
            }
        }
        return {};
    }
}
