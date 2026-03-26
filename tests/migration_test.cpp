//
// Created by Shinnosuke Kawai on 3/9/26.
//
#include "database/migrations.h"
#include <fstream>
#include <filesystem>
#include "gtest/postgres_lib_test.h"


TEST_F(PostgresLibTest, WrongPathToSqlFilePassed) {
    auto result = database::Migrate(postgres_pool, "/tmp/nonexistent_migration_file_12345.sql");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result.to_str().contains("Postgres:"));
    ASSERT_TRUE(result.to_str().contains("SqlFileError"));
}

TEST_F(PostgresLibTest, ValidMigrationSucceeds) {
    auto result = database::Migrate(postgres_pool, TEST_MIGRATIONS_SQL_PATH);
    EXPECT_FALSE(result) << result.to_str();
}

TEST_F(PostgresLibTest, IdempotentMigrationSucceeds) {
    auto first = database::Migrate(postgres_pool, TEST_MIGRATIONS_SQL_PATH);
    ASSERT_FALSE(first) << first.to_str();
    auto second = database::Migrate(postgres_pool, TEST_MIGRATIONS_SQL_PATH);
    EXPECT_FALSE(second) << second.to_str();
}

TEST_F(PostgresLibTest, InvalidSqlFileReturnsQueryFailed) {
    const std::filesystem::path temp_path = "/tmp/invalid_migration_test.sql";
    {
        std::ofstream tmp(temp_path);
        tmp << "THIS IS NOT VALID SQL;";
    }
    auto result = database::Migrate(postgres_pool, temp_path);
    std::filesystem::remove(temp_path);

    ASSERT_TRUE(result);
    ASSERT_TRUE(result.to_str().contains("Postgres: QueryFailed")) << result.to_str();
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
