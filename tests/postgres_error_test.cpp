//
// Created by Shinnosuke Kawai on 3/9/26.
//
#include <gtest/gtest.h>
#include "database/postgres_error.h"

TEST(PostgresErrTest, GetTypeReturnsCorrectEnum) {
    using E = database::sql_error;
    EXPECT_EQ(E::SqlFileError("x").get_type(),       E::type::SqlFileError);
    EXPECT_EQ(E::FailedToConnect().get_type(),        E::type::ConnectionFailed);
    EXPECT_EQ(E::FailedToReconnect("x").get_type(),  E::type::ReconnectFailed);
    EXPECT_EQ(E::BadConnection("x").get_type(),       E::type::BadConnection);
    EXPECT_EQ(E::SocketFailed("x").get_type(),        E::type::SocketFailed);
    EXPECT_EQ(E::QueryFailed("x").get_type(),         E::type::QueryFailed);
    EXPECT_EQ(E::ShuttingDown("x").get_type(),        E::type::ShuttingDown);
}

TEST(PostgresErrTest, ToStrContainsTypeName) {
    using E = database::sql_error;
    EXPECT_TRUE(E::SqlFileError("x").to_str().contains("SqlFileError"));
    EXPECT_TRUE(E::FailedToConnect().to_str().contains("ConnectionFailed"));
    EXPECT_TRUE(E::FailedToReconnect("x").to_str().contains("ReconnectFailed"));
    EXPECT_TRUE(E::BadConnection("x").to_str().contains("BadConnection"));
    EXPECT_TRUE(E::SocketFailed("x").to_str().contains("SocketFailed"));
    EXPECT_TRUE(E::QueryFailed("x").to_str().contains("QueryFailed"));
    EXPECT_TRUE(E::ShuttingDown("x").to_str().contains("ShuttingDown"));
}

TEST(PostgresErrTest, ToStrContainsMessage) {
    using E = database::sql_error;
    const char* msg = "my_error_detail";
    EXPECT_TRUE(E::QueryFailed(msg).to_str().contains(msg));
    EXPECT_TRUE(E::SqlFileError(msg).to_str().contains(msg));
    EXPECT_TRUE(E::FailedToReconnect(msg).to_str().contains(msg));
    EXPECT_TRUE(E::BadConnection(msg).to_str().contains(msg));
    EXPECT_TRUE(E::SocketFailed(msg).to_str().contains(msg));
    EXPECT_TRUE(E::ShuttingDown(msg).to_str().contains(msg));
}

TEST(PostgresErrTest, ToStrStartsWithPrefix) {
    using E = database::sql_error;
    EXPECT_TRUE(E::QueryFailed("x").to_str().starts_with("Postgres: "));
    EXPECT_TRUE(E::FailedToConnect().to_str().starts_with("Postgres: "));
    EXPECT_TRUE(E::SqlFileError("x").to_str().starts_with("Postgres: "));
    EXPECT_TRUE(E::BadConnection("x").to_str().starts_with("Postgres: "));
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
