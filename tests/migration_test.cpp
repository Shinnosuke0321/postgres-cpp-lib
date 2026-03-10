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
        factory->register_factory<Database::Postgres>([]() -> Core::Database::ConnectionResult {
            std::optional<std::string> url = Database::GetDatabaseUrl();
            if (!url) {
                return std::unexpected(Core::Database::ConnectionError::MissingConfig("Postgres URI not provided"));
            }
            auto pg_conn = std::make_unique<Database::Postgres>(std::move(*url));
            if (auto result = pg_conn->connect(); !result) {
                return std::unexpected(result.error());
            }
            return std::move(pg_conn);
        });
        pg_pool = std_ex::make_intrusive<Core::Database::ConnectionPool<Database::Postgres>>(factory);
        pg_pool->wait_for_warmup();
    }

    void TearDown() override {
        unsetenv("POSTGRES_DB_URL");
    }

    std_ex::intrusive_ptr<Core::Database::ConnectionPool<Database::Postgres>> pg_pool;
};

TEST_F(PGMigrationTest, WrongPathToSqlFilePassed) {
    auto result = Database::Migrate(pg_pool, "/tmp/nonexistent_migration_file_12345.sql");
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<Database::PostgresErr>(result.value()));
    const auto& err = std::get<Database::PostgresErr>(result.value());
    EXPECT_EQ(err.get_type(), Database::PostgresErr::Type::SqlFileError);
}

TEST_F(PGMigrationTest, ValidMigrationSucceeds) {
    auto result = Database::Migrate(pg_pool, "tests/docker/init_test_users.sql");
    EXPECT_FALSE(result.has_value()) << "Expected migration to succeed but got an error";
}

TEST_F(PGMigrationTest, IdempotentMigrationSucceeds) {
    auto first = Database::Migrate(pg_pool, "tests/docker/init_test_users.sql");
    ASSERT_FALSE(first.has_value()) << "First migration failed unexpectedly";
    auto second = Database::Migrate(pg_pool, "tests/docker/init_test_users.sql");
    EXPECT_FALSE(second.has_value()) << "Second migration failed — migration is not idempotent";
}

TEST_F(PGMigrationTest, InvalidSqlFileReturnsQueryFailed) {
    const std::filesystem::path temp_path = "/tmp/invalid_migration_test.sql";
    {
        std::ofstream tmp(temp_path);
        tmp << "THIS IS NOT VALID SQL;";
    }
    auto result = Database::Migrate(pg_pool, temp_path);
    std::filesystem::remove(temp_path);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<Database::PostgresErr>(result.value()));
    const auto& err = std::get<Database::PostgresErr>(result.value());
    EXPECT_EQ(err.get_type(), Database::PostgresErr::Type::QueryFailed);
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
