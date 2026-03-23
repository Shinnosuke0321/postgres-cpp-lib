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
#include <string>
#include <variant>
#include <vector>

namespace database {

    struct PGMigrationError: Core::BaseError {
        PGMigrationError() = default;
        ~PGMigrationError() override = default;

        explicit PGMigrationError(sql_error&& err)
            : m_error(std::move(err)) {}
        explicit PGMigrationError(Core::Database::ConnectionError&& conn_err)
            : m_error(std::move(conn_err)) {}

        explicit operator bool() const noexcept { return m_error.has_value(); }

        std::string to_str() const noexcept override {
            if (!m_error) return {};
            return std::visit([](const auto& e) { return e.to_str(); }, *m_error);
        }
    private:
        using ErrorVariant = std::variant<sql_error, Core::Database::ConnectionError>;
        std::optional<ErrorVariant> m_error = std::nullopt;
    };

    namespace internal {
        // Parses SQL text into individual statements, stripping -- and /* */ comments.
        // Respects single-quoted string literals so comment markers inside strings are ignored.
        inline std::vector<std::string> ParseStatements(const std::string& sql) {
            std::vector<std::string> statements;
            std::string current;
            std::size_t i = 0;
            const std::size_t n = sql.size();

            while (i < n) {
                // Single-line comment: skip to end of line
                if (i + 1 < n && sql[i] == '-' && sql[i + 1] == '-') {
                    while (i < n && sql[i] != '\n') ++i;
                    continue;
                }
                // Block comment: skip to */
                if (i + 1 < n && sql[i] == '/' && sql[i + 1] == '*') {
                    i += 2;
                    while (i + 1 < n && !(sql[i] == '*' && sql[i + 1] == '/')) ++i;
                    if (i + 1 < n) i += 2;
                    continue;
                }
                // String literal: pass through verbatim, handling '' escape
                if (sql[i] == '\'') {
                    current += sql[i++];
                    while (i < n) {
                        if (sql[i] == '\'' && i + 1 < n && sql[i + 1] == '\'') {
                            current += sql[i++];
                            current += sql[i++];
                        } else if (sql[i] == '\'') {
                            current += sql[i++];
                            break;
                        } else {
                            current += sql[i++];
                        }
                    }
                    continue;
                }
                // Statement separator: flush current statement
                if (sql[i] == ';') {
                    ++i;
                    const auto start = current.find_first_not_of(" \t\r\n");
                    if (start != std::string::npos) {
                        const auto end = current.find_last_not_of(" \t\r\n");
                        statements.push_back(current.substr(start, end - start + 1));
                    }
                    current.clear();
                    continue;
                }
                current += sql[i++];
            }
            // Handle trailing statement without a semicolon
            const auto start = current.find_first_not_of(" \t\r\n");
            if (start != std::string::npos) {
                const auto end = current.find_last_not_of(" \t\r\n");
                statements.push_back(current.substr(start, end - start + 1));
            }
            return statements;
        }
    } // namespace internal

    inline PGMigrationError Migrate(smart_ptr::intrusive_ptr<Core::Database::ConnectionPool<postgres_client>> pool, const std::filesystem::path& path) noexcept {
        std::ifstream file(path);
        if (!file.is_open()) {
            return PGMigrationError(sql_error::SqlFileError("Failed to open sql file"));
        }
        const std::string sql(std::istreambuf_iterator<char>(file), {});
        const auto statements = internal::ParseStatements(sql);

        using PGClient = Core::Database::ConnectionManager<postgres_client>;
        auto acquire_result = pool->acquire();
        if (!acquire_result) {
            return PGMigrationError(std::move(acquire_result.error()));
        }
        PGClient& client = acquire_result.value();

        for (const auto& stmt : statements) {
            auto future = client->execute(stmt);
            if (auto result = future.get(); !result) {
                return PGMigrationError(std::move(result.error()));
            }
        }
        return {};
    }
}
