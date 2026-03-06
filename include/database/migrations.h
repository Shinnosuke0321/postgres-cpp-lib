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

    inline std::optional<std::string> Migrate(std_ex::intrusive_ptr<Core::Database::ConnectionPool<Postgres>> pool, std::filesystem::path path) noexcept {
        std::ifstream file(path);
        if (!file.is_open()) {
            return "Failed to open file:";
        }
        std::string line;
        std::string query;
        while (std::getline(file, line)) {
            query += line;
        }
        using PGClient = Core::Database::ConnectionManager<Postgres>;
        auto acquire_result = pool->acquire();
        if (!acquire_result) {
            return acquire_result.error().to_str();
        }
        PGClient& client = acquire_result.value();
        auto future = client->execute(query);
        auto result = future.get();
        if (!result) {
            return result.error().to_str();
        }
        return std::nullopt;
    }
}
