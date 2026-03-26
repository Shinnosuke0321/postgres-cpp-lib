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
        constexpr std::string_view insert_query =
            "INSERT INTO test_tables "
            "(col_bool, col_int16, col_int32, col_int64, "
            "col_uint16, col_uint32, col_uint64, "
            "col_float, col_double, col_text, col_byte, col_ts) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)";
        auto [col_bool,col_int16,col_int32,
              col_int64,col_uint16,col_uint32,
              col_uint64,col_float,col_double,
              col_text, col_byte,col_ts] = database::make_test_values();
        auto txn = client->create_transaction();
        std::string text = col_text.value();
        std::vector<std::byte> bytes = col_byte.value();
        auto f1 = txn->execute(
            insert_query,
            col_bool,col_int16,col_int32,
            col_int64,col_uint16,col_uint32,
            col_uint64,col_float,col_double,
            text,bytes,col_ts);
        auto f2 = txn->execute(
            insert_query,
            col_bool,col_int16,col_int32,
            col_int64,col_uint16,col_uint32,
            col_uint64,col_float,col_double,
            col_text.value(), col_byte.value(),col_ts);
        auto r1 = f1.get();
        auto r2 = f2.get();
        ASSERT_TRUE(r1) << r1.error().to_str();
        ASSERT_TRUE(r2) << r2.error().to_str();
        // destructor sends COMMIT (fire-and-forget)
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

TEST_F(PostgresLibTest, Transaction_RollbackOnQueryFailure) {
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
        constexpr std::string_view insert_query =
            "INSERT INTO test_tables "
            "(col_bool, col_int16, col_int32, col_int64, "
            "col_uint16, col_uint32, col_uint64, "
            "col_float, col_double, col_text, col_byte, col_ts) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)";
        auto [col_bool,col_int16,col_int32,
              col_int64,col_uint16,col_uint32,
              col_uint64,col_float,col_double,
              col_text, col_byte,col_ts] = database::make_test_values();
        auto f1 = txn->execute(
            insert_query,
            col_bool,col_int16,col_int32,
            col_int64,col_uint16,col_uint32,
            col_uint64,col_float,col_double,
            col_text.value(), col_byte.value(),col_ts);
        auto r1 = f1.get();
        ASSERT_TRUE(r1) << r1.error().to_str();

        auto f2 = txn->execute(
            "INSERT INTO nonexistent_table_xyz (col) VALUES ($1)",
            col_text.value()); // will fail
        auto r2 = f2.get();
        ASSERT_FALSE(r2); // expect failure
        EXPECT_EQ(r2.error().get_type(), database::sql_error::type::QueryFailed);

        txn->rollback(); // sends ROLLBACK and waits
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

TEST_F(PostgresLibTest, Transaction_ManualRollback) {
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
        constexpr std::string_view insert_query =
                "INSERT INTO test_tables "
                "(col_bool, col_int16, col_int32, col_int64, "
                "col_uint16, col_uint32, col_uint64, "
                "col_float, col_double, col_text, col_byte, col_ts) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)";
        auto [col_bool,col_int16,col_int32,
              col_int64,col_uint16,col_uint32,
              col_uint64,col_float,col_double,
              col_text, col_byte,col_ts] = database::make_test_values();
        auto txn = client->create_transaction();
        auto f1 = txn->execute(
            insert_query,
        col_bool,col_int16,col_int32,
        col_int64,col_uint16,col_uint32,
        col_uint64,col_float,col_double,
        col_text.value(), col_byte.value(),col_ts);
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

TEST_F(PostgresLibTest, Transaction_DestructorAutoCommit) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();

    PGClient& client = acquired.value();

    // Count rows before
    auto before_future = client->execute("SELECT COUNT(*) FROM test_tables");
    auto before_result = before_future.get();
    ASSERT_TRUE(before_result) << before_result.error().to_str();
    const size_t rows_before = before_result.value().size();
    {
        constexpr std::string_view insert_query =
            "INSERT INTO test_tables "
            "(col_bool, col_int16, col_int32, col_int64, "
            "col_uint16, col_uint32, col_uint64, "
            "col_float, col_double, col_text, col_byte, col_ts) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)";
        auto [col_bool,col_int16,col_int32,
              col_int64,col_uint16,col_uint32,
              col_uint64,col_float,col_double,
              col_text, col_byte,col_ts] = database::make_test_values();
        auto txn = client->create_transaction();
        auto txn_future = txn->execute(
            insert_query,
            col_bool,col_int16,col_int32,
            col_int64,col_uint16,col_uint32,
            col_uint64,col_float,col_double,
            col_text.value(), col_byte.value(),col_ts);
        auto txn_result = txn_future.get();
        ASSERT_TRUE(txn_result) << txn_result.error().to_str();
        // destructor fires here — fire-and-forget COMMIT
    }

    auto after_future = client->execute("SELECT * FROM test_tables");
    auto after_result = after_future.get();
    ASSERT_TRUE(after_result) << after_result.error().to_str();
    EXPECT_GT(after_result.value().size(), rows_before+1);
}

TEST_F(PostgresLibTest, NestedAsyncQueries_AutoCommitTransaction) {
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
    }
}

TEST_F(PostgresLibTest, FutureQueries_AutoCommitTransaction) {
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

        constexpr std::string_view update_query =
            "UPDATE test_tables SET "
            "col_bool=$1, col_int16=$2, col_int32=$3, col_int64=$4, "
            "col_uint16=$5, col_uint32=$6, col_uint64=$7, "
            "col_float=$8, col_double=$9, col_text=$10, col_byte=$11, col_ts=$12 "
            "WHERE id=$13";
        auto [col_bool,col_int16,col_int32,
              col_int64,col_uint16,col_uint32,
              col_uint64,col_float,col_double,
              col_text, col_byte,col_ts] = database::make_test_values();

        for (database::result::table table = std::move(select_result.value()); auto& row : table.rows()) {
            std::optional<int32_t> id_opt = row["id"].as<int32_t>();
            ASSERT_TRUE(id_opt) << "id is not present in the table";
            int32_t id = id_opt.value();
            std::println("Updating row with id: {}", id);
            auto future_update = txn->execute(
                update_query,
                col_bool,col_int16,col_int32,
                col_int64,col_uint16,col_uint32,
                col_uint64,col_float,col_double,
                col_text.value(), col_byte.value(),col_ts,
                id);
            auto update_result = future_update.get();
            ASSERT_TRUE(update_result) << update_result.error().to_str();
        }
    }
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}