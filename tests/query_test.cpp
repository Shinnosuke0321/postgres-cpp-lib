//
// Created by Shinnosuke Kawai on 4/25/26.
//
#include <gtest/gtest.h>
#include "postgrescxx/client.h"

class QueryTest: public testing::Test {
protected:
    static void SetUpTestCase() {
        auto connected = client.connect();
        ASSERT_TRUE(connected) << connected.error().to_what();
    }
    void SetUp() override {}
    void TearDown() override {}

    inline static postgres_cxx::client client{"postgresql://test_user:test_password@localhost:5432/lego?sslmode=disable"};
};

TEST_F(QueryTest, RunLongQueryNoParams) {
    auto res = client.execute(
        "SELECT lt.name AS theme_name, COUNT(ls.set_num) AS number_of_sets "
        "FROM lego_themes lt "
        "JOIN lego_sets ls ON lt.id = ls.theme_id "
        "GROUP BY lt.name "
        "ORDER BY number_of_sets DESC "
        "LIMIT 5;")
    .get();
    ASSERT_TRUE(res) << res.error().to_string();
    postgres_cxx::result::table& table = res.value();
    ASSERT_EQ(table.rows().size(), 5u);
}

TEST_F(QueryTest, TableReturnsRightTypes) {
    auto res = client.execute(
        "SELECT lt.name AS theme_name, COUNT(ls.set_num) AS number_of_sets "
        "FROM lego_themes lt "
        "JOIN lego_sets ls ON lt.id = ls.theme_id "
        "GROUP BY lt.name "
        "ORDER BY number_of_sets DESC "
        "LIMIT 5;")
    .get();
    ASSERT_TRUE(res) << res.error().to_string();
    postgres_cxx::result::table& table =  res.value();
    for (auto& row : table.rows()) {
        auto theme_name = row.get<std::string>("theme_name");
        auto number_of_sets = row.get<int64_t>("number_of_sets");
        ASSERT_TRUE(theme_name.has_value());
        ASSERT_TRUE(number_of_sets.has_value());
    }
}

TEST_F(QueryTest, ReturnedSpecifiedNumberOfRows) {
    auto res = client.execute(
        "SELECT lt.name AS theme_name, COUNT(ls.set_num) AS number_of_sets "
        "FROM lego_themes lt "
        "JOIN lego_sets ls ON lt.id = ls.theme_id "
        "GROUP BY lt.name "
        "ORDER BY number_of_sets DESC "
        "LIMIT $1;", 10LL)
    .get();
    ASSERT_TRUE(res) << res.error().to_string();
    postgres_cxx::result::table& table =  res.value();
    ASSERT_EQ(table.rows().size(), 10u);
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
