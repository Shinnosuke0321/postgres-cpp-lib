//
// Created by Shinnosuke Kawai on 2/28/26.
//

#include <barrier>
#include <gtest/gtest.h>
#include "database/migrations.h"
#include <database/result/table.h>

class PostgresTest : public testing::Test {
protected:
    void SetUp() override {
        setenv("POSTGRES_DB_URL", "postgresql://test_user:test_password@localhost:5432/test_db?sslmode=disable", 1);
        auto factory = std::make_shared<Core::Database::ConnectionFactory>();
        factory->register_factory<database::Postgres>([]() -> Core::Database::ConnectionResult {
            std::optional<std::string> url = database::GetDatabaseUrl();
            if (!url) {
                return std::unexpected(Core::Database::ConnectionError::MissingConfig("Postgres URI not provided"));
            }
            auto pg_conn = std::make_unique<database::Postgres>(std::move(*url));
            if (std::expected<void, Core::Database::ConnectionError> result = pg_conn->connect(); !result) {
                return std::unexpected(result.error());
            }
            return std::move(pg_conn);
        });
        Core::Database::PoolConfig config;
        config.is_eager = true;
        postgres_pool = smart_ptr::make_intrusive<Core::Database::ConnectionPool<database::Postgres>>(factory);
        postgres_pool->wait_for_warmup();
    }

    static std::tuple<std::string, const char *, float, double, bool, int16_t, int32_t, int64_t> MakeTestValues() {
        std::string test_string = "test string";
        auto test_char = "test char";
        return std::make_tuple(test_string, test_char, 2.5f, 3.4, false, 112, 12143523, 3435434215);
    }

    void TearDown() override {
        unsetenv("POSTGRES_DB_URL");
    }

    smart_ptr::intrusive_ptr<Core::Database::ConnectionPool<database::Postgres> > postgres_pool;
};

TEST_F(PostgresTest, QueryFutureTest_Insert) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    using PGClient = Core::Database::ConnectionManager<database::Postgres>;
    PGClient& client = acquired.value();

    auto [text, varchar, float4, float8, boolean, integer16, integer32, integer64] = MakeTestValues();

    std::string query = "INSERT INTO test_tables "
                        "(unnullable_text, unnullable_varchar, unnullable_float4, unnullable_float8, unnullable_bool, unnullable_int16, unnullable_int32, unnullable_int64) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)";
    auto future = client->execute(query,
        text, varchar, float4, float8, boolean, integer16, integer32, integer64);
    auto result = future.get();
    ASSERT_TRUE(result) << result.error().to_str();
}

TEST_F(PostgresTest, AsyncQueryTest_Insert) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    using PGClient = Core::Database::ConnectionManager<database::Postgres>;
    PGClient& client = acquired.value();

    std::barrier ready(2);
    std::barrier release(2);

    auto [text, varchar, float4, float8, boolean, integer16, integer32, integer64] = MakeTestValues();

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
        [&ready, &release](const database::PostgresErr& err) {
            ready.arrive_and_wait();
            std::printf("error: %s\n", err.to_str().c_str());
            release.arrive_and_wait();
        }
        ,text, varchar, float4, float8, boolean, integer16, integer32, integer64);
    ready.arrive_and_wait();
    release.arrive_and_wait();
    SUCCEED();
}

TEST_F(PostgresTest, QueryFuture_Select) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    using PGClient = Core::Database::ConnectionManager<database::Postgres>;
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
    using PGClient = Core::Database::ConnectionManager<database::Postgres>;
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
