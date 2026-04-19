//
// Created by Shinnosuke Kawai on 3/27/26.
//
// Verifies that the callback thread pool overflow mechanism works correctly.
// When all callback workers are busy, postgres_client must spawn temporary
// threads (overflow threads) to dispatch remaining callbacks.
//

#include <latch>
#include <mutex>
#include <print>
#include <set>
#include <thread>
#include <gtest/gtest.h>
#include "gtest/postgres_lib_test.h"

using PGClient = Core::Database::ConnectionManager<database::postgres_client>;

// Sends enough async queries to exhaust the single idle callback worker while
// the first callback is deliberately held busy, then verifies that overflow
// (temporary) threads are spawned for the remaining callbacks.
//
// Mechanism under test (postgres_priv.cpp — PostCallback):
//   if (m_idle_cb_workers == 0)  →  spawn jthread overflow thread
//   else                         →  push to callback queue
TEST_F(PostgresLibTest, CallbackPool_SaturationSpawnsOverflowThreads) {
    auto acquired = postgres_pool->acquire();
    ASSERT_TRUE(acquired) << acquired.error().to_str();
    PGClient& client = acquired.value();

    // cb0_started  — counted down by the first callback once it begins
    //                executing, confirming the callback worker is now occupied.
    // release_cb0  — counted down by the test thread to unblock that callback.
    std::latch cb0_started{1};
    std::latch release_cb0{1};

    constexpr int total_queries = 8;
    std::latch all_done{total_queries};

    std::mutex tid_mutex;
    std::set<std::thread::id> callback_thread_ids;
    std::atomic_bool had_error{false};

    // ── Query 0 ──────────────────────────────────────────────────────────────
    // This callback intentionally pins the sole callback worker thread until
    // the test explicitly releases it, guaranteeing m_idle_cb_workers == 0
    // while all subsequent queries are being processed.
    client->execute_async(
        "SELECT 1",
        [&](const database::result::table&) {
            {
                std::lock_guard lk(tid_mutex);
                callback_thread_ids.insert(std::this_thread::get_id());
            }
            cb0_started.count_down();  // callback worker is now occupied
            release_cb0.wait();        // block until test releases us
            all_done.count_down();
        },
        [&](const database::sql_error& err) {
            std::println("Query 0 error: {}", err.to_str());
            had_error.store(true, std::memory_order_relaxed);
            cb0_started.count_down();  // avoid deadlock on the test thread
            all_done.count_down();
        }
    );

    // Wait until the first callback is actually running inside the callback
    // worker (m_idle_cb_workers has been decremented to 0).
    cb0_started.wait();
    if (had_error) {
        release_cb0.count_down();
        FAIL() << "Initial query failed; cannot test overflow behaviour";
    }

    // ── Queries 1 .. total_queries-1 ─────────────────────────────────────────
    // Each of these is dispatched while the callback worker is blocked on
    // query 0.  PostCallback will observe m_idle_cb_workers == 0 for every
    // result that arrives and must therefore spawn a temporary overflow thread.
    for (int i = 1; i < total_queries; ++i) {
        client->execute_async(
            "SELECT 1",
            [&](const database::result::table&) {
                {
                    std::lock_guard lk(tid_mutex);
                    callback_thread_ids.insert(std::this_thread::get_id());
                }
                all_done.count_down();
            },
            [&](const database::sql_error& err) {
                std::println("Query error: {}", err.to_str());
                had_error.store(true, std::memory_order_relaxed);
                all_done.count_down();
            }
        );
    }

    // Give the query worker enough time to process (and call PostCallback for)
    // all remaining queries while the callback worker is still occupied.
    // SELECT 1 on a local PostgreSQL completes in <5 ms; 300 ms is ample.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Unblock query 0's callback, letting the callback worker finish.
    release_cb0.count_down();

    all_done.wait();
    ASSERT_FALSE(had_error) << "One or more async queries returned an error";

    // Overflow threads run on freshly spawned jthreads — each has a distinct
    // thread::id from the single callback worker.  Observing > 1 unique ID
    // across all N callbacks confirms that overflow threads were actually used.
    std::println("Unique callback thread IDs observed: {}", callback_thread_ids.size());
    EXPECT_GT(callback_thread_ids.size(), 1u)
        << "Expected overflow threads: callback pool should have been saturated";
}

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
