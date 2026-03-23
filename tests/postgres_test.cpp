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

    void TearDown() override {
        unsetenv("POSTGRES_DB_URL");
    }

    smart_ptr::intrusive_ptr<Core::Database::ConnectionPool<database::postgres_client>> postgres_pool;
};

TEST_F(PostgresTest, QueryFutureTest_Insert) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    using PGClient = Core::Database::ConnectionManager<database::postgres_client>;
    PGClient& client = acquired.value();



    std::string query(database::testing::insert_query);
    auto [col_bool,
        col_int16,
        col_int32,
        col_int64,
        col_uint16,
        col_uint32,
        col_uint64,
        col_float,
        col_double,
        col_text, col_byte
        ,col_ts] = database::make_test_values();
    auto future = client->execute(query,
    col_bool,col_int16,col_int32,
        col_int64,
        col_uint16,
        col_uint32,
        col_uint64,
        col_float,
        col_double,
        col_text.value(), col_byte.value()
        ,col_ts);
    auto result = future.get();
    ASSERT_TRUE(result) << result.error().to_str();
}

TEST_F(PostgresTest, AsyncQueryTest_Insert) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    using PGClient = Core::Database::ConnectionManager<database::postgres_client>;
    PGClient& client = acquired.value();

    std::barrier ready(2);
    std::barrier release(2);
    std::optional<database::sql_error> error = std::nullopt;

    const std::string query = "INSERT INTO test_tables "
                        "(unnullable_text, unnullable_varchar, unnullable_float4, unnullable_float8, unnullable_bool, unnullable_int16, unnullable_int32, unnullable_int64) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)";
    client->execute_async(query,
        [&ready, &release](const database::result::table &result) {
            ready.arrive_and_wait();
            std::println("{} rows acquired!!", result.size());
            std::println("Successfully inserted");
            release.arrive_and_wait();
        },
        [&error, &ready, &release](const database::sql_error& err) {
            ready.arrive_and_wait();
            error = err;
            release.arrive_and_wait();
        }
        ,text, varchar, float4, float8, boolean, integer16, integer32, integer64);
    ready.arrive_and_wait();
    if (error)
        FAIL() << error->to_str();
    release.arrive_and_wait();
    SUCCEED();
}

TEST_F(PostgresTest, QueryFuture_Select) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    using PGClient = Core::Database::ConnectionManager<database::postgres_client>;
    PGClient& client = acquired.value();
    std::string query = "SELECT * FROM test_tables";
    auto future = client->execute(query);
    auto result = future.get();
    ASSERT_TRUE(result) << result.error().to_str();

    database::result::table table = std::move(result.value());
    std::println("{} rows selected", table.size());
    for (auto& row : table.rows()) {

    }
}

TEST_F(PostgresTest, AsyncQuery_Select) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    using PGClient = Core::Database::ConnectionManager<database::postgres_client>;
    PGClient& client = acquired.value();
    std::string query = "SELECT * FROM test_tables";
    auto future = client->execute(query);
    auto result = future.get();
    ASSERT_TRUE(result) << result.error().to_str();
    database::result::table table = std::move(result.value());
    std::println("{} rows selected", table.size());
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
