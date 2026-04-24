// //
// // Created by Shinnosuke Kawai on 2/28/26.
// //
//
#include <gtest/gtest.h>
#include "gtest/postgres_lib_test.h"

TEST_F(DbConnectionTest, ConnectionMadeSuccessfully) {
    const postgres_cxx::client client(valid_url);
    const auto connected = client.connect();
    ASSERT_TRUE(connected);
}

TEST_F(DbConnectionTest, ConnectingWithWrongUser) {
    const postgres_cxx::client client(invalid_url_user);
    const auto connected = client.connect();
    ASSERT_FALSE(connected);
    std::println("{}", connected.error().to_what());
}

TEST_F(DbConnectionTest, ConnectingWithWrongPassword) {
    const postgres_cxx::client client(invalid_url_password);
    const auto connected = client.connect();
    ASSERT_FALSE(connected);
    std::println("{}", connected.error().to_what());
}

TEST_F(DbConnectionTest, ConnectingWithWrongPort) {
    const postgres_cxx::client client(invalid_url_port);
    const auto connected = client.connect();
    ASSERT_FALSE(connected);
    std::println("{}", connected.error().to_what());
}

TEST_F(DbConnectionTest, ConnectingWithWrongDb) {
    const postgres_cxx::client client(invalid_url_db);
    const auto connected = client.connect();
    ASSERT_FALSE(connected);
    std::println("{}", connected.error().to_what());
}

TEST_F(DbConnectionTest, ConnectingWithWrongHost) {
    const postgres_cxx::client client(invalid_url_host);
    const auto connected = client.connect();
    ASSERT_FALSE(connected);
    std::println("{}", connected.error().to_what());
}


int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
