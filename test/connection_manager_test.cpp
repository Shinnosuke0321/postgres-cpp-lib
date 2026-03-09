//
// Unit tests for Core::Database::ConnectionManager
//
#include <gtest/gtest.h>
#include <database/connection_manager.h>

using namespace Core::Database;

namespace {
    struct SimpleConn : IConnection {
        bool* destroyed = nullptr;
        int tag = 0;
        ~SimpleConn() override { if (destroyed) *destroyed = true; }
    };
}

TEST(ConnectionManagerTest, Destructor_InvokesReleaser) {
    bool released = false;
    {
        auto conn = std::make_unique<SimpleConn>();
        ConnectionManager<SimpleConn> mgr(
            std::move(conn),
            [&released](std::unique_ptr<SimpleConn>) { released = true; });
    }
    ASSERT_TRUE(released);
}

TEST(ConnectionManagerTest, Destructor_PassesConnectionToReleaser) {
    bool destroyed = false;
    SimpleConn* captured_ptr = nullptr;
    {
        auto conn = std::make_unique<SimpleConn>();
        conn->destroyed = &destroyed;
        ConnectionManager<SimpleConn> mgr(
            std::move(conn),
            [&captured_ptr](std::unique_ptr<SimpleConn> c) {
                captured_ptr = c.get(); // take ownership; 'c' destructs at end of lambda
            });
    }
    // The releaser received the connection; 'c' in lambda was destroyed
    ASSERT_NE(captured_ptr, nullptr);
    ASSERT_TRUE(destroyed); // SimpleConn destroyed when lambda's 'c' went out of scope
}

TEST(ConnectionManagerTest, MoveConstructor_OriginalReleaserNulled) {
    int fire_count = 0;
    auto make_mgr = [&] {
        return ConnectionManager<SimpleConn>(
            std::make_unique<SimpleConn>(),
            [&fire_count](std::unique_ptr<SimpleConn>) { ++fire_count; });
    };

    auto a = make_mgr();
    {
        auto b = std::move(a);
        // a has null releaser and null connection now
        // Destroying a (via reset or scope exit) should be a no-op
    }
    // b was destroyed → releaser fired once
    ASSERT_EQ(fire_count, 1);
    // a goes out of scope at end of test — must not double-fire
}

TEST(ConnectionManagerTest, MoveAssignment_TransfersOwnership) {
    int count_a = 0, count_b = 0;

    auto a = ConnectionManager<SimpleConn>(
        std::make_unique<SimpleConn>(),
        [&count_a](std::unique_ptr<SimpleConn>) { ++count_a; });

    {
        auto b = ConnectionManager<SimpleConn>(
            std::make_unique<SimpleConn>(),
            [&count_b](std::unique_ptr<SimpleConn>) { ++count_b; });

        a = std::move(b);
        // b is now empty; a holds what was b's connection + releaser
        // b's original connection (transferred to a) will be released when a fires
    }
    // b went out of scope (empty after move) — its releaser (now null) should not fire
    ASSERT_EQ(count_b, 0);

    // a goes out of scope here → fires b's releaser (which was moved into a)
    // count_b increments
    // But wait: a was moved-assigned from b, so a now holds b's releaser
    // a's original releaser was already fired? No: move assignment just overwrites a's members
    // The old a's connection/releaser are dropped when operator= runs (no explicit release)
    // Actually: look at the implementation — move assignment just reassigns members without
    // calling the old releaser. So count_a stays 0.
    // We need to verify this by the end of the test scope.
    // a will be destroyed at end of test function, triggering count_b++.
    ASSERT_EQ(count_a, 0);
}

// Separate test to verify the move-assigned manager fires once on destruction
TEST(ConnectionManagerTest, MoveAssignment_FiredOnDestruction) {
    int count = 0;
    {
        ConnectionManager<SimpleConn> a(
            std::make_unique<SimpleConn>(),
            [](std::unique_ptr<SimpleConn>) {});

        ConnectionManager<SimpleConn> b(
            std::make_unique<SimpleConn>(),
            [&count](std::unique_ptr<SimpleConn>) { ++count; });

        a = std::move(b); // a now holds b's releaser; b is empty
        // end of scope: b destroyed (no-op), then a destroyed (count++)
    }
    ASSERT_EQ(count, 1);
}

TEST(ConnectionManagerTest, SelfMoveAssignment_IsNoOp) {
    int fire_count = 0;
    {
        auto mgr = ConnectionManager<SimpleConn>(
            std::make_unique<SimpleConn>(),
            [&fire_count](std::unique_ptr<SimpleConn>) { ++fire_count; });
        mgr = std::move(mgr); // self-move — guarded by this == &other check
        ASSERT_EQ(fire_count, 0); // should not have fired yet
    }
    ASSERT_EQ(fire_count, 1); // fires exactly once on destruction
}

TEST(ConnectionManagerTest, NullReleaser_ConnectionDeletedByManager) {
    bool destroyed = false;
    {
        auto conn = std::make_unique<SimpleConn>();
        conn->destroyed = &destroyed;
        // Pass empty Releaser — release() will call m_connection.reset() instead
        ConnectionManager mgr(std::move(conn), ConnectionManager<SimpleConn>::Releaser{});
    }
    ASSERT_TRUE(destroyed);
}

TEST(ConnectionManagerTest, ArrowAndDereferenceOperators) {
    auto conn = std::make_unique<SimpleConn>();
    conn->tag = 7;
    ConnectionManager<SimpleConn> mgr(
        std::move(conn),
        [](std::unique_ptr<SimpleConn>) {});

    ASSERT_EQ(mgr->tag, 7);
    ASSERT_EQ((*mgr).tag, 7);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
