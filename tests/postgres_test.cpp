//
// Created by Shinnosuke Kawai on 2/28/26.
//

#include <database/postgres.h>
#include <gtest/gtest.h>
#include <database/connection_pool.h>
#include <core/ref.h>

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
    Core::Database::ConnectionPool::PoolConfig config;

    auto postgres_pool = std_ex::make_intrusive<Core::Database::ConnectionPool<Database::Postgres>>(factory);
    postgres_pool->wait_for_warmup();

}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new PostgresTest());
    return RUN_ALL_TESTS();
}
