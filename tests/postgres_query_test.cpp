//
// Created by Shinnosuke Kawai on 2/28/26.
//

#include <barrier>
#include <thread>
#include <gtest/gtest.h>
#include "database/migrations.h"
#include <database/result/table.h>
#include "gtest/postgres_lib_test.h"
#include "gtest/test_queries.h"
#include "gtest/test_values.h"

using PGClient = Core::Database::ConnectionManager<database::postgres_client>;

TEST_F(PostgresLibTest, QueryFutureTest_Insert) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    PGClient& client = acquired.value();
    constexpr std::string_view insert_query = INSERT_QUERY;
    auto [col_bool,col_int16,col_int32,
          col_int64,col_uint16,col_uint32,
          col_uint64,col_float,col_double,
          col_text, col_byte,col_ts] = database::make_test_values();
    auto future = client->execute(insert_query, COlUMN_VALUES);
    auto result = future.get();
    ASSERT_TRUE(result) << result.error().to_str();
}

TEST_F(PostgresLibTest, AsyncQueryTest_Insert) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    PGClient& client = acquired.value();

    std::barrier ready(2);
    std::barrier release(2);
    std::optional<database::sql_error> error = std::nullopt;

    constexpr std::string_view insert_query = INSERT_QUERY;
    auto [col_bool,col_int16,col_int32,
          col_int64,col_uint16,col_uint32,
          col_uint64,col_float,col_double,
          col_text, col_byte,col_ts] = database::make_test_values();
    client->execute_async(insert_query,
        [&ready, &release](const database::result::table &result) {
            ready.arrive_and_wait();
            std::println("Successfully inserted");
            release.arrive_and_wait();
        },
        [&error, &ready, &release](const database::sql_error& err) {
            ready.arrive_and_wait();
            error = err;
            release.arrive_and_wait();
        },
        COlUMN_VALUES);
    ready.arrive_and_wait();
    if (error)
        FAIL() << error->to_str();
    release.arrive_and_wait();
    SUCCEED();
}

TEST_F(PostgresLibTest, QueryFuture_Select) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    PGClient& client = acquired.value();
    std::string query = "SELECT * FROM test_tables";
    auto future = client->execute(query);
    auto result = future.get();
    ASSERT_TRUE(result) << result.error().to_str();

    database::result::table table = std::move(result.value());
    std::println("{} rows selected", table.size());
}

TEST_F(PostgresLibTest, AsyncQuery_Select) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    PGClient& client = acquired.value();

    auto shared_pms = std::make_shared<std::promise<std::expected<void, database::sql_error>>>();
    const std::string query = "SELECT * FROM test_tables";
    client->execute_async(
        query,
        [shared_pms](const database::result::table& result) {
            shared_pms->set_value({});
        },
        [shared_pms](const database::sql_error& error) {
            shared_pms->set_value(std::unexpected(error));
        });
    auto result = shared_pms->get_future().get();
    ASSERT_TRUE(result) << result.error().to_str();
}

TEST_F(PostgresLibTest, NestedAsyncQuery_Update) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();

    PGClient& client = acquired.value();
    constexpr std::string_view select_all_query =
        "SELECT id, col_bool, col_int16, col_int32, col_int64, "
        "col_uint16, col_uint32, col_uint64, "
        "col_float, col_double, col_text, col_byte, col_ts "
        "FROM test_tables";

    auto shared_pms = std::make_shared<std::promise<std::expected<void, database::sql_error>>>();
    auto future = shared_pms->get_future();
    client->execute_async(
        select_all_query,
        [&client, shared_pms](const database::result::table& result) {
            apply_changes_to_rows(client, result, shared_pms);
        },
        [shared_pms](const database::sql_error& error) {
            shared_pms->set_value(std::unexpected(error));
        });
    auto result = future.get();
    ASSERT_TRUE(result) << result.error().to_str();
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
