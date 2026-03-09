//
// Unit tests for Core::Database::ConnectionFactory
//
#include <gtest/gtest.h>
#include <database/connection_factory.h>
#include <atomic>
#include <memory>
#include <thread>
#include <chrono>

using namespace Core::Database;

namespace {
    struct ConnA : IConnection { int tag = 1; };
    struct ConnB : IConnection { int tag = 2; };
}

class ConnectionFactoryTest : public ::testing::Test {
protected:
    ConnectionFactory factory;
};

TEST_F(ConnectionFactoryTest, CreateAfterRegistration_Succeeds) {
    factory.register_factory<ConnA>([]() -> ConnectionResult {
        return std::unique_ptr<IConnection>(new ConnA{});
    });

    auto result = factory.create_connection<ConnA>();
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result.value().get(), nullptr);
    ASSERT_EQ(result.value()->tag, 1);
}

TEST_F(ConnectionFactoryTest, FactoryReturningError_Propagates) {
    factory.register_factory<ConnA>([]() -> ConnectionResult {
        return std::unexpected(ConnectionError::AuthFailed("bad creds"));
    });

    auto result = factory.create_connection<ConnA>();
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().get_code(), ConnectionError::Type::AuthFailed);
}

TEST_F(ConnectionFactoryTest, TwoDistinctTypes_BothWork) {
    factory.register_factory<ConnA>([]() -> ConnectionResult {
        return std::unique_ptr<IConnection>(new ConnA{});
    });
    factory.register_factory<ConnB>([]() -> ConnectionResult {
        return std::unique_ptr<IConnection>(new ConnB{});
    });

    auto ra = factory.create_connection<ConnA>();
    auto rb = factory.create_connection<ConnB>();
    ASSERT_TRUE(ra.has_value());
    ASSERT_TRUE(rb.has_value());
    ASSERT_EQ(ra.value()->tag, 1);
    ASSERT_EQ(rb.value()->tag, 2);
}

TEST_F(ConnectionFactoryTest, ReRegistration_OverwritesPrevious) {
    factory.register_factory<ConnA>([]() -> ConnectionResult {
        auto c = std::make_unique<ConnA>();
        c->tag = 1;
        return std::unique_ptr<IConnection>(std::move(c));
    });
    factory.register_factory<ConnA>([]() -> ConnectionResult {
        auto c = std::make_unique<ConnA>();
        c->tag = 99;
        return std::unique_ptr<IConnection>(std::move(c));
    });

    auto result = factory.create_connection<ConnA>();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value()->tag, 99);
}

TEST_F(ConnectionFactoryTest, FactoryCalledEachTime_NotCached) {
    std::atomic call_count{0};
    factory.register_factory<ConnA>([&call_count]() -> ConnectionResult {
        ++call_count;
        return std::unique_ptr<IConnection>(new ConnA{});
    });

    factory.create_connection<ConnA>();
    factory.create_connection<ConnA>();
    factory.create_connection<ConnA>();
    ASSERT_EQ(call_count.load(), 3);
}

// create_connection now copies the factory function while holding the shared lock,
// so concurrent writers replacing the entry cannot cause a dangling-reference crash.
TEST_F(ConnectionFactoryTest, ConcurrentReaderWriter_NoDataRace) {
    factory.register_factory<ConnA>([]() -> ConnectionResult {
        return std::unique_ptr<IConnection>(new ConnA{});
    });

    std::atomic running{true};
    std::vector<std::thread> threads;
    threads.reserve(8);

    // Writers: repeatedly re-register the factory
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &running]() {
            while (running.load(std::memory_order_relaxed)) {
                factory.register_factory<ConnA>([]() -> ConnectionResult {
                    return std::unique_ptr<IConnection>(new ConnA{});
                });
            }
        });
    }

    // Readers: repeatedly create connections concurrently with writers
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &running] {
            while (running.load(std::memory_order_relaxed)) {
                const auto res = factory.create_connection<ConnA>();
                (void)res;
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds{200});
    running.store(false);
    for (auto& thread: threads) thread.join();
    SUCCEED();
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
