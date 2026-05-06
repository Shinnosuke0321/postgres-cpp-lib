// //
// // Created by Shinnosuke Kawai on 2/28/26.
// //
//
#include <gtest/gtest.h>
#include "gtest/postgres_lib_test.h"

TEST_F(DbConnectionTest, ConnectionMadeSuccessfully) {
    postgres_cxx::client client(valid_url);
    const auto connected = client.connect();
    ASSERT_TRUE(connected);
}

TEST_F(DbConnectionTest, ConnectingWithWrongUser) {
    postgres_cxx::client client(invalid_url_user);
    const auto connected = client.connect();
    ASSERT_FALSE(connected);
    std::println("{}", connected.error().to_what());
}

TEST_F(DbConnectionTest, ConnectingWithWrongPassword) {
    postgres_cxx::client client(invalid_url_password);
    const auto connected = client.connect();
    ASSERT_FALSE(connected);
    std::println("{}", connected.error().to_what());
}

TEST_F(DbConnectionTest, ConnectingWithWrongPort) {
    postgres_cxx::client client(invalid_url_port);
    const auto connected = client.connect();
    ASSERT_FALSE(connected);
    std::println("{}", connected.error().to_what());
}

TEST_F(DbConnectionTest, ConnectingWithWrongDb) {
    postgres_cxx::client client(invalid_url_db);
    const auto connected = client.connect();
    ASSERT_FALSE(connected);
    std::println("{}", connected.error().to_what());
}

TEST_F(DbConnectionTest, ConnectingWithWrongHost) {
    postgres_cxx::client client(invalid_url_host);
    const auto connected = client.connect();
    ASSERT_FALSE(connected);
    std::println("{}", connected.error().to_what());
}

TEST_F(DbConnectionTest, CallingConnectCausesNoCrash) {
    constexpr size_t num = 1000;
    std::vector<std::jthread> threads(num);
    postgres_cxx::client client(valid_url);
    for (auto& t : threads) {
        t = std::jthread([&client] {
            const auto connected = client.connect();
            ASSERT_TRUE(client.is_connected());
        });
    }
    for (auto& t : threads)
        if (t.joinable())
            t.join();
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
