//
// Created by Shinnosuke Kawai on 3/9/26.
//
#include <gtest/gtest.h>
#include "database/migrations.h"
#include <fstream>
#include <filesystem>

class PGMigrationTest : public testing::Test {
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
            if (auto result = pg_conn->connect(); !result) {
                return std::unexpected(result.error());
            }
            return std::move(pg_conn);
        });
        pg_pool = smart_ptr::make_intrusive<Core::Database::ConnectionPool<database::Postgres>>(factory);
        pg_pool->wait_for_warmup();
    }

    void TearDown() override {
        unsetenv("POSTGRES_DB_URL");
    }

    smart_ptr::intrusive_ptr<Core::Database::ConnectionPool<database::Postgres>> pg_pool;
};

TEST_F(PGMigrationTest, WrongPathToSqlFilePassed) {
    auto result = database::Migrate(pg_pool, "/tmp/nonexistent_migration_file_12345.sql");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result.to_str().contains("Postgres:"));
    ASSERT_TRUE(result.to_str().contains("SqlFileError"));
}

TEST_F(PGMigrationTest, ValidMigrationSucceeds) {
    auto result = database::Migrate(pg_pool, TEST_MIGRATIONS_SQL_PATH);
    EXPECT_FALSE(result) << result.to_str();
}

TEST_F(PGMigrationTest, IdempotentMigrationSucceeds) {
    auto first = database::Migrate(pg_pool, TEST_MIGRATIONS_SQL_PATH);
    ASSERT_FALSE(first) << first.to_str();
    auto second = database::Migrate(pg_pool, TEST_MIGRATIONS_SQL_PATH);
    EXPECT_FALSE(second) << second.to_str();
}

TEST_F(PGMigrationTest, InvalidSqlFileReturnsQueryFailed) {
    const std::filesystem::path temp_path = "/tmp/invalid_migration_test.sql";
    {
        std::ofstream tmp(temp_path);
        tmp << "THIS IS NOT VALID SQL;";
    }
    auto result = database::Migrate(pg_pool, temp_path);
    std::filesystem::remove(temp_path);

    ASSERT_TRUE(result);
    ASSERT_TRUE(result.to_str().contains("Postgres: QueryFailed")) << result.to_str();
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
