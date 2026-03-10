//
// Created by Shinnosuke Kawai on 3/7/26.
//

#pragma once
#include <core/ref.h>
#include <database/connection_pool.h>
#include "postgres.h"
#include <filesystem>
#include <fstream>

namespace Database {

    inline std::optional<std::variant<Core::Database::ConnectionError, PostgresErr>> Migrate(std_ex::intrusive_ptr<Core::Database::ConnectionPool<Postgres>> pool, const std::filesystem::path& path) noexcept {
        std::ifstream file(path);
        if (!file.is_open()) {
            return PostgresErr::SqlFileError("File not found");
        }
        std::string line;
        std::string query;
        while (std::getline(file, line)) {
            query += line;
        }
        using PGClient = Core::Database::ConnectionManager<Postgres>;
        auto acquire_result = pool->acquire();
        if (!acquire_result) {
            return std::move(acquire_result.error());
        }
        PGClient& client = acquire_result.value();
        auto future = client->execute(query);
        if (auto result = future.get(); !result) {
            return std::move(result.error());
        }
        return std::nullopt;
    }
}
