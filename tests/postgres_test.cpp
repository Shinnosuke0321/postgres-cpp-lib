//
// Created by Shinnosuke Kawai on 2/28/26.
//

#include <barrier>
#include <thread>
#include <gtest/gtest.h>
#include "database/migrations.h"
#include <database/result/table.h>
#include "test_queries.h"
#include "test_values.h"

using PGClient = Core::Database::ConnectionManager<database::postgres_client>;

class PostgresTest : public testing::Test {
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

    static void apply_changes_to_rows(PGClient& client, const database::result::table& table) {
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

    void TearDown() override {
        unsetenv("POSTGRES_DB_URL");
    }

    smart_ptr::intrusive_ptr<Core::Database::ConnectionPool<database::postgres_client>> postgres_pool;
};

TEST_F(PostgresTest, QueryFutureTest_Insert) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    PGClient& client = acquired.value();
    constexpr std::string_view insert_query =
        "INSERT INTO test_tables "
        "(col_bool, col_int16, col_int32, col_int64, "
        "col_uint16, col_uint32, col_uint64, "
        "col_float, col_double, col_text, col_byte, col_ts) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)";
    auto [col_bool,col_int16,col_int32,
        col_int64,col_uint16,col_uint32,
        col_uint64,col_float,col_double,
        col_text, col_byte,col_ts] = database::make_test_values();
    auto future = client->execute(insert_query,
        col_bool,col_int16,col_int32,
        col_int64,col_uint16,col_uint32,
        col_uint64,col_float,col_double,
        col_text.value(),col_byte.value(),col_ts);
    auto result = future.get();
    ASSERT_TRUE(result) << result.error().to_str();
}

TEST_F(PostgresTest, AsyncQueryTest_Insert) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    PGClient& client = acquired.value();

    std::barrier ready(2);
    std::barrier release(2);
    std::optional<database::sql_error> error = std::nullopt;

    constexpr std::string_view insert_query =
        "INSERT INTO test_tables "
        "(col_bool, col_int16, col_int32, col_int64, "
        "col_uint16, col_uint32, col_uint64, "
        "col_float, col_double, col_text, col_byte, col_ts) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)";
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
        col_bool,col_int16,col_int32,
        col_int64,col_uint16,col_uint32,
        col_uint64,col_float,col_double,
        col_text.value(),col_byte.value(),col_ts);
    ready.arrive_and_wait();
    if (error)
        FAIL() << error->to_str();
    release.arrive_and_wait();
    SUCCEED();
}

TEST_F(PostgresTest, QueryFuture_Select) {
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

TEST_F(PostgresTest, AsyncQuery_Select) {
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

TEST_F(PostgresTest, NestedAsyncQuery_Update) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();

    PGClient& client = acquired.value();
    constexpr std::string_view select_all_query =
        "SELECT id, col_bool, col_int16, col_int32, col_int64, "
        "col_uint16, col_uint32, col_uint64, "
        "col_float, col_double, col_text, col_byte, col_ts "
        "FROM test_tables";

    auto shared_pms = std::make_shared<std::promise<std::expected<void, database::sql_error>>>();
    client->execute_async(
        select_all_query,
        [&client, shared_pms](const database::result::table& result) {
            apply_changes_to_rows(client, result);
            shared_pms->set_value({});
        },
        [shared_pms](const database::sql_error& error) {
            shared_pms->set_value(std::unexpected(error));
        });
    auto result = shared_pms->get_future().get();
    ASSERT_TRUE(result) << result.error().to_str();
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
