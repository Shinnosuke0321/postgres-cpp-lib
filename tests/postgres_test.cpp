//
// Created by Shinnosuke Kawai on 2/28/26.
//

#include <database/postgres.h>
#include <gtest/gtest.h>
#include <database/connection_pool.h>
#include <core/ref.h>

TEST(PostgresSQL_lib, GetDatabaseUrlTest) {
    std::string pass_url = "postgres://postgres:postgres@localhost:5432/postgres"
                                 "?keepalives=1"
                                 "&keepalives_idle=30"
                                 "&keepalives_interval=10"
                                 "&keepalives_count=5";
    const std::optional<std::string> null_url = Database::GetDatabaseUrl();
    ASSERT_EQ(null_url, std::nullopt);
    setenv("POSTGRES_DB_URL", "postgres://postgres:postgres@localhost:5432/postgres", 1);
    const std::optional<std::string> valid_url = Database::GetDatabaseUrl();
    ASSERT_TRUE(valid_url);
    ASSERT_EQ(valid_url.value(), pass_url);
}

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
        return pg_conn;
    });
    auto postgres_pool = std_ex::make_intrusive<Core::Database::ConnectionPool<Database::Postgres>>(factory);
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
