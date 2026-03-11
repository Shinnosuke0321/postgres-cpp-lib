//
// Created by Shinnosuke Kawai on 2/28/26.
//

#include <gtest/gtest.h>
#include "database/migrations.h"

class PostgresTest : public testing::Environment {
    void SetUp() override {
        setenv("POSTGRES_DB_URL", "postgresql://test_user:test_password@localhost:5432/test_db?sslmode=disable", 1);
    }
    void TearDown() override {
        unsetenv("POSTGRES_DB_URL");
    }
};

TEST(PostgresSQL_Lib, QueryFutureTest) {
    auto factory = std::make_shared<Core::Database::ConnectionFactory>();
    factory->register_factory<Database::Postgres>([]() -> Core::Database::ConnectionResult {
        std::optional<std::string> url = Database::GetDatabaseUrl();
        if (!url) {
            return std::unexpected(Core::Database::ConnectionError::MissingConfig("Postgres URI not provided"));
        }
        auto pg_conn = std::make_unique<Database::Postgres>(std::move(*url));
        if (std::expected<void, Core::Database::ConnectionError> result = pg_conn->connect(); !result) {
            return std::unexpected(result.error());
        }
        return std::move(pg_conn);
    });
    Core::Database::PoolConfig config;
    config.is_eager = true;
    auto postgres_pool = smart_ptr::make_intrusive<Core::Database::ConnectionPool<Database::Postgres>>(factory);
    postgres_pool->wait_for_warmup();
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    using PGClient = Core::Database::ConnectionManager<Database::Postgres>;
    PGClient& client = acquired.value();

    std::string query = "INSERT INTO users (name) VALUES ($1)";
    auto future = client->execute(query, "user1");
    auto result = future.get();
    ASSERT_TRUE(result) << result.error().to_str();
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new PostgresTest());
    return RUN_ALL_TESTS();
}
