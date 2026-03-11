//
// Advanced unit and integration tests for Core::Database::ConnectionPool
//
#include <gtest/gtest.h>
#include <database/connection_pool.h>
#include <barrier>
#include <chrono>
#include <optional>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {
    struct FakeConn : Core::Database::IConnection {
        int id = 0;
        explicit FakeConn(int i = 0) : id(i) {}
    };
}

// Fixture: provides a ConnectionFactory pre-registered for FakeConn
class PoolFeeder : public ::testing::Test {
protected:
    std::shared_ptr<Core::Database::ConnectionFactory> factory;
    std::atomic<int> conn_id_counter{0};

    void SetUp() override {
        factory = std::make_shared<Core::Database::ConnectionFactory>();
        factory->register_factory<FakeConn>([this]() -> Core::Database::ConnectionResult {
            return std::unique_ptr<Core::Database::IConnection>(
                new FakeConn{conn_id_counter.fetch_add(1)});
        });
    }

    auto make_pool(Core::Database::PoolConfig cfg) {
        return smart_ptr::make_intrusive<Core::Database::ConnectionPool<FakeConn>>(factory, cfg);
    }
};

TEST_F(PoolFeeder, LazyMode_PoolReadyImmediately) {
    Core::Database::PoolConfig cfg;
    cfg.is_eager = false;
    cfg.init_size = 2;
    cfg.max_size = 4;

    auto pool = make_pool(cfg);
    // No wait_for_warmup() needed — lazy mode sets pool_ready in constructor
    auto res = pool->acquire();
    ASSERT_TRUE(res.has_value());
}

TEST_F(PoolFeeder, LazyMode_CanAcquireUpToMaxSize) {
    Core::Database::PoolConfig cfg;
    cfg.is_eager = false;
    cfg.init_size = 0;
    cfg.max_size = 6;

    auto pool = make_pool(cfg);

    // Acquire all 6 slots and hold them simultaneously
    std::vector<Core::Database::ConnectionManager<FakeConn>> managers;
    managers.reserve(6);
    for (int i = 0; i < 6; ++i) {
        auto res = pool->acquire();
        ASSERT_TRUE(res.has_value()) << "acquire #" << i << " should succeed";
        managers.emplace_back(std::move(res.value()));
    }
    ASSERT_EQ(static_cast<int>(managers.size()), 6);
}

TEST_F(PoolFeeder, Acquire_Timeout_WhenPoolExhausted) {
    Core::Database::PoolConfig cfg;
    cfg.is_eager = false;
    cfg.init_size = 0;
    cfg.max_size = 2;

    auto pool = make_pool(cfg);

    auto r1 = pool->acquire();
    auto r2 = pool->acquire();
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    auto m1 = std::move(r1.value());
    auto m2 = std::move(r2.value());

    // Pool exhausted; acquire with zero timeout should return Timeout immediately
    auto r3 = pool->acquire(std::chrono::seconds{0});
    ASSERT_FALSE(r3.has_value());
    ASSERT_EQ(r3.error().get_code(), Core::Database::ConnectionError::Type::Timeout);
}

TEST_F(PoolFeeder, Acquire_ConnectionReturned_IsReused) {
    Core::Database::PoolConfig cfg;
    cfg.is_eager = false;
    cfg.init_size = 0;
    cfg.max_size = 1;

    auto pool = make_pool(cfg);

    FakeConn* first_ptr = nullptr;
    {
        auto res = pool->acquire();
        ASSERT_TRUE(res.has_value());
        auto mgr = std::move(res.value());
        first_ptr = mgr.operator->();
    } // manager destroyed → connection returned to queue

    auto res2 = pool->acquire();
    ASSERT_TRUE(res2.has_value());
    FakeConn* second_ptr = res2.value().operator->();

    ASSERT_EQ(first_ptr, second_ptr); // same object reused from queue
}

TEST_F(PoolFeeder, Acquire_FactoryFailure_ReturnsError) {
    // Override factory with one that always fails
    auto fail_factory = std::make_shared<Core::Database::ConnectionFactory>();
    fail_factory->register_factory<FakeConn>([]() -> Core::Database::ConnectionResult {
        return std::unexpected(Core::Database::ConnectionError::AuthFailed("injected failure"));
    });

    Core::Database::PoolConfig cfg;
    cfg.is_eager = false;
    cfg.init_size = 0;
    cfg.max_size = 2;

    auto pool = smart_ptr::make_intrusive<Core::Database::ConnectionPool<FakeConn>>(fail_factory, cfg);
    auto res = pool->acquire();
    ASSERT_FALSE(res.has_value());
    // Must be the factory's error, not a timeout
    ASSERT_EQ(res.error().get_code(), Core::Database::ConnectionError::Type::AuthFailed);
}

TEST_F(PoolFeeder, InvalidConfig_EagerWithInitGtMax_FallsToLazy) {
    // init_size > max_size with is_eager=true: condition fails, constructor falls to lazy path
    Core::Database::PoolConfig cfg;
    cfg.is_eager = true;
    cfg.init_size = 10;
    cfg.max_size = 5;

    auto pool = make_pool(cfg);
    // Pool should be immediately ready (lazy path set m_pool_ready = true)
    auto res = pool->acquire();
    ASSERT_TRUE(res.has_value());
}

// ---------------------------------------------------------------------------
// Suite: ConnectionPoolLifetimeTest
// ---------------------------------------------------------------------------

TEST_F(PoolFeeder, RefCountTracking_LazyMode) {
    Core::Database::PoolConfig cfg;
    cfg.is_eager = false;
    cfg.init_size = 1;
    cfg.max_size = 2;

    auto pool = make_pool(cfg);
    ASSERT_EQ(pool->ref_count(), 1u);
    {
        auto res = pool->acquire();
        ASSERT_TRUE(res.has_value());
        auto mgr = std::move(res.value());
        // wrap_connection captured an intrusive_ptr to pool in the releaser lambda
        ASSERT_EQ(pool->ref_count(), 2u);
    } // manager destroyed → releaser fires → intrusive_ptr in lambda destroyed
    ASSERT_EQ(pool->ref_count(), 1u);
}

TEST_F(PoolFeeder, PoolOutlivedByManager_NoUseAfterFree) {
    Core::Database::PoolConfig cfg;
    cfg.is_eager = false;
    cfg.init_size = 0;
    cfg.max_size = 1;

    std::optional<Core::Database::ConnectionManager<FakeConn>> mgr_holder;
    {
        auto pool = make_pool(cfg);
        auto res = pool->acquire();
        ASSERT_TRUE(res.has_value());
        mgr_holder.emplace(std::move(res.value()));
        ASSERT_EQ(pool->ref_count(), 2u);
        // pool intrusive_ptr goes out of scope here → ref_count: 2→1
        // Pool is kept alive by the lambda inside mgr_holder
    }
    // mgr_holder destroyed here → releaser fires → pool ref_count: 1→0 → pool deleted
    // If ASan reports no use-after-free, the lifetime management is correct
    mgr_holder.reset();
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Suite: ConnectionPoolConcurrencyTest
// ---------------------------------------------------------------------------

TEST_F(PoolFeeder, ConcurrentAcquireRelease_NoRefLeak) {
    Core::Database::PoolConfig cfg;
    cfg.is_eager = false;
    cfg.init_size = 0;
    cfg.max_size = 8;

    auto pool = make_pool(cfg);
    ASSERT_EQ(pool->ref_count(), 1u);

    constexpr int kThreads = 32;
    std::barrier start_barrier(kThreads + 1);
    std::vector<std::jthread> threads;
    threads.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&](std::stop_token) {
            start_barrier.arrive_and_wait();
            auto res = pool->acquire(2s);
            // Connection is released when res/manager goes out of scope here
            (void)res;
        });
    }

    start_barrier.arrive_and_wait();
    threads.clear(); // joins all jthreads (their destructors request stop and join)

    // All managers destroyed; every lambda's intrusive_ptr dropped → ref_count back to 1
    ASSERT_EQ(pool->ref_count(), 1u);
}

TEST_F(PoolFeeder, HighContention_AllAcquiresSucceedOrTimeout) {
    Core::Database::PoolConfig cfg;
    cfg.is_eager = false;
    cfg.init_size = 0;
    cfg.max_size = 4;

    auto pool = make_pool(cfg);

    constexpr int kThreads = 64;
    std::atomic success_count{0};
    std::atomic failure_count{0};
    std::barrier start_barrier(kThreads + 1);
    std::vector<std::jthread> threads;
    threads.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&](std::stop_token) {
            start_barrier.arrive_and_wait();
            auto res = pool->acquire(2s);
            if (res.has_value()) {
                success_count.fetch_add(1, std::memory_order_relaxed);
                // Connection released immediately when res goes out of scope
            } else {
                failure_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    start_barrier.arrive_and_wait();
    threads.clear();

    ASSERT_EQ(success_count.load() + failure_count.load(), kThreads);
    ASSERT_GE(success_count.load(), 4); // at least max_size connections were dispensed
    ASSERT_EQ(pool->ref_count(), 1u);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
