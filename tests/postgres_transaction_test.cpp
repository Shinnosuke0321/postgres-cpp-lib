//
// Created by Shinnosuke Kawai on 3/18/26.
//
#include "gtest/postgres_lib_test.h"

using PGClient = Core::Database::ConnectionManager<database::postgres_client>;

TEST_F(PostgresLibTest, Transaction_Commit) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    PGClient& client = acquired.value();

    // Count rows before
    auto before_future = client->execute("SELECT COUNT(*) FROM test_tables");
    auto before_result = before_future.get();
    ASSERT_TRUE(before_result) << before_result.error().to_str();
    database::result::table table_before = std::move(before_result.value());
    std::optional<int64_t> row_count_before = table_before.rows()[0]["count"].as<int64_t>();
    const size_t rows_before = row_count_before.value();
    {
        constexpr std::string_view insert_query = INSERT_QUERY;
        database::test_row test_row = database::make_test_values();
        auto txn = client->create_transaction();
        std::string copied_text = test_row.col_text.value();
        std::vector<std::byte> copied_bytes = test_row.col_byte.value();
        auto f1 = txn->execute(
            insert_query,
            test_row.col_bool,test_row.col_int16,test_row.col_int32,
            test_row.col_int64,test_row.col_uint16,test_row.col_uint32,
            test_row.col_uint64,test_row.col_float,test_row.col_double,
            copied_text,copied_bytes,test_row.col_ts);
        auto f2 = txn->execute(insert_query,COLUMN_DATA(test_row));
        auto r1 = f1.get();
        auto r2 = f2.get();
        ASSERT_TRUE(r1) << r1.error().to_str();
        ASSERT_TRUE(r2) << r2.error().to_str();
        txn->commit();
    }

    auto after_future = client->execute("SELECT COUNT(*) FROM test_tables");
    auto after_result = after_future.get();
    ASSERT_TRUE(after_result) << after_result.error().to_str();
    database::result::table table_after = std::move(after_result.value());
    std::optional<int64_t> row_count_after = table_after.rows()[0]["count"].as<int64_t>();
    ASSERT_TRUE(row_count_after);
    size_t rows_after = row_count_after.value();
    std::println("rows_before: {}, rows_after: {}", rows_before, rows_after);
    ASSERT_EQ(rows_before + 2, rows_after);
}

TEST_F(PostgresLibTest, TransactionAutoRollbackOnQueryFailure) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();

    PGClient& client = acquired.value();
    // Count rows before
    auto before_future = client->execute("SELECT COUNT(*) FROM test_tables");
    auto before_result = before_future.get();
    ASSERT_TRUE(before_result) << before_result.error().to_str();
    database::result::table table_before = std::move(before_result.value());
    std::optional<int64_t> row_count_before = table_before.rows()[0]["count"].as<int64_t>();
    ASSERT_TRUE(row_count_before);
    const size_t rows_before = row_count_before.value();

    {
        auto txn = client->create_transaction();
        constexpr std::string_view insert_query = INSERT_QUERY;
        database::test_row test_row = database::make_test_values();
        auto f1 = txn->execute(insert_query,COLUMN_DATA(test_row));
        auto r1 = f1.get();
        ASSERT_TRUE(r1) << r1.error().to_str();

        auto f2 = txn->execute(
            "INSERT INTO nonexistent_table_xyz (col) VALUES ($1)",
            test_row.col_text.value()); // will fail
        auto r2 = f2.get();
        ASSERT_FALSE(r2); // expect failure
        EXPECT_EQ(r2.error().get_type(), database::sql_error::type::QueryFailed);
    }

    // Verify no rows were inserted (ROLLBACK worked)
    auto after_future = client->execute("SELECT COUNT(*) FROM test_tables");
    auto after_result = after_future.get();
    ASSERT_TRUE(after_result) << after_result.error().to_str();
    database::result::table table_after = std::move(after_result.value());
    std::optional<int64_t> row_count_after = table_after.rows()[0]["count"].as<int64_t>();
    ASSERT_TRUE(row_count_after);
    size_t rows_after = row_count_after.value();
    EXPECT_EQ(rows_after, rows_before);
}

TEST_F(PostgresLibTest, TransactionManualRollback) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();

    PGClient& client = acquired.value();

    // Count rows before
    auto before_future = client->execute("SELECT COUNT(*) FROM test_tables");
    auto before_result = before_future.get();
    ASSERT_TRUE(before_result) << before_result.error().to_str();
    database::result::table table_before = std::move(before_result.value());
    std::optional<int64_t> row_count_before = table_before.rows()[0]["count"].as<int64_t>();
    ASSERT_TRUE(row_count_before);
    const size_t rows_before = row_count_before.value();
    {
        constexpr std::string_view insert_query = INSERT_QUERY;
        database::test_row test_row = database::make_test_values();
        auto txn = client->create_transaction();
        auto f1 = txn->execute(insert_query, COLUMN_DATA(test_row));
        auto r1 = f1.get();
        ASSERT_TRUE(r1) << r1.error().to_str();
        txn->rollback(); // sends ROLLBACK and waits
    }

    // Verify no rows were inserted
    auto after_future = client->execute("SELECT COUNT(*) FROM test_tables");
    auto after_result = after_future.get();
    ASSERT_TRUE(after_result) << after_result.error().to_str();
    database::result::table table = std::move(after_result.value());
    std::optional<int64_t> rows_count_after = table.rows()[0]["count"].as<int64_t>();
    ASSERT_TRUE(rows_count_after);
    size_t rows_after = rows_count_after.value();
    EXPECT_EQ(rows_after, rows_before);
}

TEST_F(PostgresLibTest, TransactionManualCommit) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();

    PGClient& client = acquired.value();

    // Count rows before
    auto before_future = client->execute("SELECT * FROM test_tables");
    auto before_result = before_future.get();
    ASSERT_TRUE(before_result) << before_result.error().to_str();
    const size_t rows_before = before_result.value().size();
    {
        constexpr std::string_view insert_query = INSERT_QUERY;
        auto TEST_COLUMN = database::make_test_values();
        auto txn = client->create_transaction();
        auto txn_future = txn->execute(insert_query, COlUMN_VALUES);
        auto txn_result = txn_future.get();
        ASSERT_TRUE(txn_result) << txn_result.error().to_str();
        txn->commit();
    }

    auto after_future = client->execute("SELECT * FROM test_tables");
    auto after_result = after_future.get();
    ASSERT_TRUE(after_result) << after_result.error().to_str();
    EXPECT_EQ(after_result.value().size(), rows_before+1);
}

TEST_F(PostgresLibTest, TransactionNestedAsyncQueries) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    {
        PGClient& client = acquired.value();
        constexpr std::string_view select_query = "select * from test_tables";
        auto txn = client->create_transaction();
        ASSERT_FALSE(!txn) << "Transaction should be created";
        auto shared_pms = std::make_shared<std::promise<std::expected<void, database::sql_error>>>();
        txn->execute_async(
            select_query,
            [shared_pms, txn](const database::result::table& table) {
                apply_changes_to_rows(txn, table);
                shared_pms->set_value({});
            },
            [shared_pms](const database::sql_error& error) {
                shared_pms->set_value(std::unexpected(error));
            });
        auto result = shared_pms->get_future().get();
        ASSERT_TRUE(result) << result.error().to_str();
        txn->commit();
    }
}

TEST_F(PostgresLibTest, TransactionFutureUpdateQuries) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    {
        PGClient& client = acquired.value();
        auto txn = client->create_transaction();
        ASSERT_FALSE(!txn) << "Transaction should be created";

        constexpr std::string_view select_query = "select * from test_tables";
        auto query_future = txn->execute(select_query);
        auto select_result = query_future.get();
        ASSERT_TRUE(select_result) << select_result.error().to_str();

        database::result::table& table_before = select_result.value();

        constexpr std::string_view update_query = UPDATE_QUERY_ID;
        auto TEST_COLUMN = database::make_test_values();

        for (database::result::table table = std::move(select_result.value()); auto& row : table.rows()) {
            std::optional<int32_t> id_opt = row["id"].as<int32_t>();
            ASSERT_TRUE(id_opt) << "id is not present in the table";
            int32_t id = id_opt.value();
            auto future_update = txn->execute(update_query,COlUMN_VALUES,id);
            auto update_result = future_update.get();
            ASSERT_TRUE(update_result) << update_result.error().to_str();
        }
        txn->commit();
    }
}

TEST_F(PostgresLibTest, FutureTransactionUsedAfterRolledback) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    {
        PGClient& client = acquired.value();
        database::shared_transaction transaction = client->create_transaction();
        queries_before_rolled_back(transaction, 1);
        transaction->rollback();
        queries_after_rolled_back(transaction, 1);
    }
}

TEST_F(PostgresLibTest, AsyncTransactionUsedAfterRolledback) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    {
        PGClient& client = acquired.value();
        database::shared_transaction transaction = client->create_transaction();
        queries_before_rolled_back(transaction, 0);
        transaction->rollback();
        queries_after_rolled_back(transaction, 0);
    }
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}