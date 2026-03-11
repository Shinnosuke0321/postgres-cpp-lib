#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <mutex>
#include <queue>
#include <expected>
#include <semaphore>
#include <vector>
#include <thread>
#include <stop_token>
#include "connection.h"
#include "connection_factory.h"
#include "connection_manager.h"
#include <core/memory/intrusive_ptr.h>

namespace Core::Database {
    struct PoolConfig {
        std::size_t max_size = std::thread::hardware_concurrency();
        std::size_t init_size = 10;
        bool is_eager = false;
    };

    template<class T>
    requires std::derived_from<T, IConnection>
    class ConnectionPool: public core::ref_counted<ConnectionPool<T>> {
    public:
        using SharedFactory = std::shared_ptr<ConnectionFactory>;
        using AcquireResult = std::expected<ConnectionManager<T>, ConnectionError>;

        explicit ConnectionPool(SharedFactory factory, const PoolConfig& opt = PoolConfig()) noexcept;
        ~ConnectionPool() override;

        AcquireResult acquire(std::chrono::seconds timeout = std::chrono::seconds{3}) noexcept;
        void wait_for_warmup() const noexcept;
    private:
        void warmup_pool() noexcept;
        void fill_pool(const std::stop_token& st) noexcept;

        ConnectionManager<T> wrap_connection(std::unique_ptr<T> c) noexcept;

    private:
        mutable std::mutex m_mutex;
        std::atomic_bool m_pool_ready = false;
        std::atomic_bool m_capacity_expanded = false;
        PoolConfig m_config;
        std::queue<std::unique_ptr<T>> m_connections;
        std::counting_semaphore<> m_capacity{0};
        std::shared_ptr<ConnectionFactory> m_factory;
        std::vector<std::jthread> m_threads;
    };
} // namespace Core::Database

#include "connection_pool_impl.h"