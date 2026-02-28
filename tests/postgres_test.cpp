//
// Created by Shinnosuke Kawai on 2/28/26.
//

#include "postgres.h"
#include <gtest/gtest.h>

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

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
