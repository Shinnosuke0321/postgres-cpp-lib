//
// Created by Shinnosuke Kawai on 2/24/26.
//
#include <cassert>
#include <chrono>
#include <memory>
#include <gtest/gtest.h>
#include <database/connection_pool.h>

using namespace std::chrono_literals;

namespace {
    struct FakeConn : Core::Database::IConnection {
        int value = 42;
        ~FakeConn() override {
            assert(value == 42);
        };
    };
}

TEST(ConnectionPoolTest, PoolTest) {
    auto factory = std::make_shared<Core::Database::ConnectionFactory>();

    factory->register_factory<FakeConn>([]() -> Core::Database::ConnectionResult {
        return std::unique_ptr<Core::Database::IConnection>(new FakeConn{});
    });

    Core::Database::PoolConfig cfg;
    cfg.is_eager = true;

    auto pool = std_ex::make_intrusive<Core::Database::ConnectionPool<FakeConn>>(factory, cfg);
    pool->wait_for_warmup();
    ASSERT_EQ(pool->ref_count(), 1);
    {
        auto res = pool->acquire();
        ASSERT_EQ(pool->ref_count(), 2);
        auto mgr = std::move(res.value());
        ASSERT_EQ(pool->ref_count(), 2);
        ASSERT_EQ(mgr->value,  42);
    } // manager destructor should return connection to pool
    ASSERT_EQ(pool->ref_count(), 1);

    {
        auto res2 = pool->acquire(1s);
        ASSERT_EQ(pool->ref_count(), 2);
        auto& mgr2 = res2.value();
        ASSERT_EQ(pool->ref_count(), 2);
        ASSERT_EQ(mgr2->value, 42);
    }
    ASSERT_EQ(pool->ref_count(), 1);
}
TEST(ConnectionPoolTest, LazyPool_BasicAcquire) {
    auto factory = std::make_shared<Core::Database::ConnectionFactory>();
    factory->register_factory<FakeConn>([]() -> Core::Database::ConnectionResult {
        return std::unique_ptr<Core::Database::IConnection>(new FakeConn{});
    });

    Core::Database::PoolConfig cfg;
    cfg.is_eager = false;
    cfg.init_size = 2;
    cfg.max_size = 4;

    auto pool = std_ex::make_intrusive<Core::Database::ConnectionPool<FakeConn>>(factory, cfg);
    ASSERT_EQ(pool->ref_count(), 1u);
    {
        auto res = pool->acquire();
        ASSERT_TRUE(res.has_value());
        auto mgr = std::move(res.value());
        ASSERT_EQ(pool->ref_count(), 2u);
        ASSERT_EQ(mgr->value, 42);
    }
    ASSERT_EQ(pool->ref_count(), 1u);
}

TEST(ConnectionPoolTest, MultipleAcquires_UpToMaxSize) {
    auto factory = std::make_shared<Core::Database::ConnectionFactory>();
    factory->register_factory<FakeConn>([]() -> Core::Database::ConnectionResult {
        return std::unique_ptr<Core::Database::IConnection>(new FakeConn{});
    });

    Core::Database::PoolConfig cfg;
    cfg.is_eager = false;
    cfg.init_size = 0;
    cfg.max_size = 3;

    auto pool = std_ex::make_intrusive<Core::Database::ConnectionPool<FakeConn>>(factory, cfg);
    {
        auto r1 = pool->acquire();
        auto r2 = pool->acquire();
        auto r3 = pool->acquire();
        ASSERT_TRUE(r1.has_value());
        ASSERT_TRUE(r2.has_value());
        ASSERT_TRUE(r3.has_value());
        auto m1 = std::move(r1.value());
        auto m2 = std::move(r2.value());
        auto m3 = std::move(r3.value());
        // pool + 3 manager lambdas each holding an intrusive_ptr to pool
        ASSERT_EQ(pool->ref_count(), 4u);
    }
    ASSERT_EQ(pool->ref_count(), 1u);
}

TEST(ConnectionPoolTest, TimeoutOnExhaustedPool) {
    auto factory = std::make_shared<Core::Database::ConnectionFactory>();
    factory->register_factory<FakeConn>([]() -> Core::Database::ConnectionResult {
        return std::unique_ptr<Core::Database::IConnection>(new FakeConn{});
    });

    Core::Database::PoolConfig cfg;
    cfg.is_eager = false;
    cfg.init_size = 0;
    cfg.max_size = 1;

    auto pool = std_ex::make_intrusive<Core::Database::ConnectionPool<FakeConn>>(factory, cfg);
    auto r1 = pool->acquire();
    ASSERT_TRUE(r1.has_value());
    auto m1 = std::move(r1.value());

    // Pool exhausted; second acquire with zero timeout should return Timeout
    auto r2 = pool->acquire(std::chrono::seconds{0});
    ASSERT_FALSE(r2.has_value());
    ASSERT_EQ(r2.error().get_code(), Core::Database::ConnectionError::Type::Timeout);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}