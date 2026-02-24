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
#include "connection.h"
#include "connection_factory.h"
#include "connection_manager.h"
#include <trantor/utils/Logger.h>

namespace Core::Database {
    template<class T>
    requires std::derived_from<T, IConnection>
    class ConnectionPool: public std::enable_shared_from_this<ConnectionPool<T>> {
    public:
        struct PoolConfig {
            std::size_t max_size = 30;
            std::size_t init_size = 10;
            bool is_eager = false;
        };
    public:
        using Factory = std::shared_ptr<ConnectionFactory>;
        using AcquireResult = std::expected<ConnectionManager<T>, ConnectionError>;

        explicit ConnectionPool(Factory factory, const PoolConfig& opt = PoolConfig()) noexcept;
        ~ConnectionPool();

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