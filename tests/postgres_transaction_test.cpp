//
// Created by Shinnosuke Kawai on 3/18/26.
//
#include <database/connection_pool.h>
#include <database/connection_factory.h>
#include <gtest/gtest.h>
#include "test_queries.h"
#include <database/postgres_client.h>
#include <database/transaction.h>

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

    static std::tuple<std::string, const char *, float, double, bool, int16_t, int32_t, int64_t> MakeTestValues() {
        std::string test_string = "test string";
        auto test_char = "test char";
        return std::make_tuple(test_string, test_char, 2.5f, 3.4, false, 112, 12143523, 3435434215);
    }

    void TearDown() override {
        unsetenv("POSTGRES_DB_URL");
    }

    smart_ptr::intrusive_ptr<Core::Database::ConnectionPool<database::postgres_client>> postgres_pool;
};

TEST_F(PostgresTest, Transaction_Commit) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    PGClient& client = acquired.value();

    auto [text, varchar, float4, float8, boolean, integer16, integer32, integer64] = MakeTestValues();

    // Count rows before
    auto before_future = client->execute("SELECT * FROM test_tables");
    auto before_result = before_future.get();
    ASSERT_TRUE(before_result) << before_result.error().to_str();
    const size_t rows_before = before_result.value().size();

    std::string query{database::testing::insert_query};
    {
        auto txn = client->create_transaction();
        auto f1 = txn->execute(query, text, varchar, float4, float8, boolean, integer16, integer32, integer64);
        auto f2 = txn->execute(query, text, varchar, float4, float8, boolean, integer16, integer32, integer64);
        auto r1 = f1.get();
        auto r2 = f2.get();
        ASSERT_TRUE(r1) << r1.error().to_str();
        ASSERT_TRUE(r2) << r2.error().to_str();
        // destructor sends COMMIT (fire-and-forget)
    }

    auto after_future = client->execute("SELECT * FROM test_tables");
    auto after_result = after_future.get();
    ASSERT_TRUE(after_result) << after_result.error().to_str();
    EXPECT_EQ(after_result.value().size(), rows_before + 2);
}

TEST_F(PostgresTest, Transaction_RollbackOnQueryFailure) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();

    PGClient& client = acquired.value();

    auto [text, varchar, float4, float8, boolean, integer16, integer32, integer64] = MakeTestValues();

    const std::string insert_query = "INSERT INTO test_tables "
        "(unnullable_text, unnullable_varchar, unnullable_float4, unnullable_float8, unnullable_bool, unnullable_int16, unnullable_int32, unnullable_int64) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)";

    // Count rows before
    auto before_future = client->execute("SELECT COUNT(*) FROM test_tables");
    auto before_result = before_future.get();
    ASSERT_TRUE(before_result) << before_result.error().to_str();
    const size_t rows_before = before_result.value().size();

    auto txn = client->create_transaction();
    auto f1 = txn->execute(insert_query, text, varchar, float4, float8, boolean, integer16, integer32, integer64);
    auto r1 = f1.get();
    ASSERT_TRUE(r1) << r1.error().to_str();

    auto f2 = txn->execute("INSERT INTO nonexistent_table_xyz (col) VALUES ($1)", text); // will fail
    auto r2 = f2.get();
    ASSERT_FALSE(r2); // expect failure
    EXPECT_EQ(r2.error().get_type(), database::sql_error::Type::QueryFailed);

    txn->rollback(); // sends ROLLBACK and waits

    // Verify no rows were inserted (ROLLBACK worked)
    auto after_future = client->execute("SELECT COUNT(*) FROM test_tables");
    auto after_result = after_future.get();
    ASSERT_TRUE(after_result) << after_result.error().to_str();
    EXPECT_EQ(after_result.value().size(), rows_before);
}

TEST_F(PostgresTest, Transaction_ManualRollback) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();

    PGClient& client = acquired.value();

    auto [text, varchar, float4, float8, boolean, integer16, integer32, integer64] = MakeTestValues();

    // Count rows before
    auto before_future = client->execute("SELECT COUNT(*) FROM test_tables");
    auto before_result = before_future.get();
    ASSERT_TRUE(before_result) << before_result.error().to_str();
    const size_t rows_before = before_result.value().size();

    const std::string insert_query = "INSERT INTO test_tables "
        "(unnullable_text, unnullable_varchar, unnullable_float4, unnullable_float8, unnullable_bool, unnullable_int16, unnullable_int32, unnullable_int64) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)";

    {
        auto txn = client->create_transaction();
        auto f1 = txn->execute(insert_query, text, varchar, float4, float8, boolean, integer16, integer32, integer64);
        auto r1 = f1.get();
        ASSERT_TRUE(r1) << r1.error().to_str();
        txn->rollback(); // sends ROLLBACK and waits
    }

    // Verify no rows were inserted
    auto after_future = client->execute("SELECT COUNT(*) FROM test_tables");
    auto after_result = after_future.get();
    ASSERT_TRUE(after_result) << after_result.error().to_str();
    EXPECT_EQ(after_result.value().size(), rows_before);
}

TEST_F(PostgresTest, Transaction_DestructorAutoCommit) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();

    PGClient& client = acquired.value();

    auto [text, varchar, float4, float8, boolean, integer16, integer32, integer64] = MakeTestValues();

    // Count rows before
    auto before_future = client->execute("SELECT * FROM test_tables");
    auto before_result = before_future.get();
    ASSERT_TRUE(before_result) << before_result.error().to_str();
    const size_t rows_before = before_result.value().size();

    const std::string insert_query = "INSERT INTO test_tables "
        "(unnullable_text, unnullable_varchar, unnullable_float4, unnullable_float8, unnullable_bool, unnullable_int16, unnullable_int32, unnullable_int64) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)";

    {
        auto txn = client->create_transaction();
        txn->execute(insert_query, text, varchar, float4, float8, boolean, integer16, integer32, integer64);
        // destructor fires here — fire-and-forget COMMIT
    }

    auto after_future = client->execute("SELECT * FROM test_tables");
    auto after_result = after_future.get();
    ASSERT_TRUE(after_result) << after_result.error().to_str();
    EXPECT_GT(after_result.value().size(), rows_before);
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}