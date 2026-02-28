//
// Created by Shinnosuke Kawai on 2/25/26.
//
#include <thread>
#include <core/ref.h>
#include <barrier>
#include <gtest/gtest.h>
#include <print>

static inline std::atomic count{0};
using namespace Core;
class Resource : public RefCounted<Resource> {
public:
    Resource() {
        std::println("Resource created");
    }

    ~Resource() override {
        count.fetch_add(1, std::memory_order_relaxed);
        std::println("Resource destroyed");
    }
private:
    int data =  0;
};

static void PrintResource(const std_ex::intrusive_ptr<Resource>& resource) {
    ASSERT_EQ(resource->ref_count(), 2);
}

TEST(IntrusivePtrTest, MultiThreads) {
    {
        const unsigned hc = std::max(std::thread::hardware_concurrency(), 1u);
        const int threads = static_cast<int>(std::min(256u, hc * 4u));
        auto resource = std_ex::make_intrusive<Resource>();

        std::barrier ready(threads + 1);
        std::barrier release(threads + 1);

        std::vector<std::thread> workers;
        workers.reserve(threads);

        try {
            for (int i = 0; i < threads; ++i) {
                workers.emplace_back([resource, &ready, &release]() mutable {
                    ready.arrive_and_wait();   // everyone now holds exactly one ref
                    release.arrive_and_wait(); // keep holding r until main says go
                });
            }
        } catch (const std::system_error& e) {
            for (auto& t : workers) {
                if (t.joinable()) {
                    t.join();
                }
            }
            FAIL() << "Failed to create threads " << e.what();
        }

        ready.arrive_and_wait(); // all workers are definitely holding refs *now*
        ASSERT_EQ(resource->ref_count(), threads + 1);
        release.arrive_and_wait(); // let workers exit (and drop refs)

        for (auto& t : workers)
            t.join();
        ASSERT_EQ(resource->ref_count(), 1);
    }
    ASSERT_EQ(count.load(std::memory_order_relaxed), 1);
}

TEST(IntrusivePreTest, SingleThread_1) {
    auto origin = std_ex::make_intrusive<Resource>();
    origin = origin;
    ASSERT_EQ(origin->ref_count(), 1);
    auto copied_res = origin;
    ASSERT_EQ(copied_res->ref_count(), 2);
    auto moved_res = std::move(copied_res);
    ASSERT_EQ(moved_res->ref_count(), 2);
    ASSERT_EQ(copied_res.get(), nullptr);
    moved_res.reset();
    ASSERT_EQ(origin->ref_count(), 1);
    origin.reset();
    ASSERT_EQ(count.load(std::memory_order_relaxed), 1);
}

TEST(IntrusivePreTest, SingleThread_2) {
    {
        auto origin = std_ex::make_intrusive<Resource>();
        {
            ASSERT_EQ(origin->ref_count(), 1);
            auto printAction = [origin] {
                ASSERT_EQ(origin->ref_count(), 2);
                PrintResource(origin);
            };
            printAction();
            ASSERT_EQ(origin->ref_count(), 2);
        }
        ASSERT_EQ(origin->ref_count(), 1);
    }
    ASSERT_EQ(count.load(std::memory_order_relaxed), 1);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
