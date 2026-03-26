//
// Created by Shinnosuke Kawai on 3/26/26.
//

#pragma once
#include "test_values.h"
#include <gtest/gtest.h>
#include <database/connection_pool.h>
#include <database/postgres_client.h>
#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
inline int setenv(const char* name, const char* value, int overwrite)
{
    if (!overwrite) {
        size_t envsize = 0;
        errno_t err = getenv_s(&envsize, NULL, 0, name);
        if (err == 0 && envsize != 0) return 0; // Variable already exists, do not overwrite
    }
    std::string env_var = std::string(name) + "=" + std::string(value);
    return _putenv(env_var.c_str());
}
#else
#include <cstdlib> // for setenv on non-Windows systems
#endif

inline void apply_changes_to_rows(const std::shared_ptr<database::transaction>& transaction_ptr, const database::result::table& table) {
    for (const auto& row : table.rows()) {
        std::optional<int32_t> id_opt = row["id"].as<int32_t>();
        EXPECT_TRUE(id_opt) << "id is not present in the table";
        int32_t id = id_opt.value();
        auto [col_bool,col_int16,col_int32,
              col_int64,col_uint16,col_uint32,
              col_uint64,col_float,col_double,
              col_text, col_byte,col_ts] = database::make_updated_values();
        auto shared_pms = std::make_shared<std::promise<std::expected<void, database::sql_error>>>();
        std::future<std::expected<void, database::sql_error>> future = shared_pms->get_future();
        std::println("Updating row with id: {}", id);
        transaction_ptr->execute_async(
            "UPDATE test_tables SET "
            "col_bool=$1, col_int16=$2, col_int32=$3, col_int64=$4, "
            "col_uint16=$5, col_uint32=$6, col_uint64=$7, col_float=$8, "
            "col_double=$9, col_text=$10, col_byte=$11, col_ts=$12 "
            "WHERE id=$13",
            [shared_pms](const database::result::table&) { shared_pms->set_value({}); },
            [transaction_ptr, shared_pms](const database::sql_error& error) {
                transaction_ptr->rollback();
                shared_pms->set_value(std::unexpected(error));
            },
            col_bool,col_int16,col_int32,col_int64,
            col_uint16,col_uint32,col_uint64,col_float,
            col_double,col_text.value(),col_byte.value(),col_ts,
            id);
        auto result = future.get();
        ASSERT_TRUE(result) << result.error().to_str();
    }
}

inline void apply_changes_to_rows(Core::Database::ConnectionManager<database::postgres_client>& client, const database::result::table& table) {
    for (const auto& row : table.rows()) {
        std::optional<int32_t> id_opt = row["id"].as<int32_t>();
        EXPECT_TRUE(id_opt) << "id is not present in the table";
        int32_t id = id_opt.value();
        auto [col_bool,col_int16,col_int32,
              col_int64,col_uint16,col_uint32,
              col_uint64,col_float,col_double,
              col_text, col_byte,col_ts] = database::make_updated_values();
        auto shared_pms = std::make_shared<std::promise<std::expected<void, database::sql_error>>>();
        client->execute_async(
            "UPDATE test_tables SET "
            "col_bool=$1, col_int16=$2, col_int32=$3, col_int64=$4, "
            "col_uint16=$5, col_uint32=$6, col_uint64=$7, col_float=$8, "
            "col_double=$9, col_text=$10, col_byte=$11, col_ts=$12 "
            "WHERE id=$13",
            [shared_pms](const database::result::table&) { shared_pms->set_value({}); },
            [shared_pms](const database::sql_error& error) { shared_pms->set_value(std::unexpected(error)); },
            col_bool,col_int16,col_int32,col_int64,
            col_uint16,col_uint32,col_uint64,col_float,
            col_double,col_text.value(),col_byte.value(),col_ts,
            id);
        auto result = shared_pms->get_future().get();
        ASSERT_TRUE(result) << result.error().to_str();
    }
}

class PostgresLibTest : public testing::Test {
protected:
    void SetUp() override {
        setenv("POSTGRES_DB_URL", "postgresql://test_user:test_password@localhost:5432/test_db?sslmode=disable", 1);
        auto factory = std::make_shared<Core::Database::ConnectionFactory>();
        factory->register_factory<database::postgres_client>([]() -> Core::Database::ConnectionResult {
            std::optional<std::string> url = database::GetDatabaseUrl();
            if (!url) {
                return std::unexpected(Core::Database::ConnectionError::MissingConfig("Postgres URI not provided"));
            }
            auto pg_conn = std::make_unique<database::postgres_client>(std::move(*url));
            if (std::expected<void, Core::Database::ConnectionError> result = pg_conn->connect(); !result) {
                return std::unexpected(result.error());
            }
            return std::move(pg_conn);
        });
        Core::Database::PoolConfig config;
        config.is_eager = true;
        postgres_pool = smart_ptr::make_intrusive<Core::Database::ConnectionPool<database::postgres_client>>(factory);
        postgres_pool->wait_for_warmup();
    }

    void TearDown() override {
    }

    smart_ptr::intrusive_ptr<Core::Database::ConnectionPool<database::postgres_client>> postgres_pool;
};
