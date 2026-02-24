//
// Created by Shinnosuke Kawai on 10/22/25.
//

#pragma once

namespace Core::Database {

    template<class T>
    requires std::derived_from<T, IConnection>
    ConnectionPool<T>::ConnectionPool(std::shared_ptr<ConnectionFactory> factory, const PoolConfig& opt) noexcept
    : m_config(opt),
      m_capacity(static_cast<std::ptrdiff_t>(m_config.init_size)),
      m_factory(std::move(factory))
    {
        if (m_config.is_eager && m_config.init_size > 0 && m_config.max_size > 0 && m_config.max_size >= m_config.init_size) {
            warmup_pool();
        }
        else {
            const auto init = m_config.init_size;
            const auto max  = m_config.max_size;
            if (m_config.max_size > m_config.init_size) {
                if (!m_capacity_expanded.exchange(true, std::memory_order_acq_rel)) {
                    m_capacity.release(static_cast<std::ptrdiff_t>(max - init));
                }
            }
            m_pool_ready.store(true, std::memory_order_release);
            m_pool_ready.notify_all();
        }
    }

    template<class T>
    requires std::derived_from<T, IConnection>
    void ConnectionPool<T>::warmup_pool() noexcept {
        using namespace std::literals;
        m_threads.reserve(m_config.init_size);
        for (size_t i = 0; i < m_config.init_size; ++i) {
            m_threads.emplace_back([this](const std::stop_token &st) { fill_pool(st);});
        }
    }

    template<class T> requires std::derived_from<T, IConnection>
    void ConnectionPool<T>::fill_pool(const std::stop_token& st) noexcept {
        using namespace std::chrono_literals;
        for (;;) {
            if (st.stop_requested())
                return;

            bool is_full = false;
            if (m_capacity.try_acquire()) {
                auto conn_res = m_factory->create_connection<T>();
                if (!conn_res) {
                    LOG_ERROR << conn_res.error().to_str();
                    m_capacity.release();
                    std::this_thread::sleep_for(1000ms);
                    continue;
                }
                {
                    std::unique_lock lk(m_mutex);
                    m_connections.emplace(std::move(*conn_res));
                    is_full = (m_connections.size() == m_config.init_size);
                }
                m_capacity.release();
            }

            if (is_full) {
                if (!m_pool_ready.exchange(true, std::memory_order_acq_rel)) {
                    m_pool_ready.notify_all();
                }
                const auto init = m_config.init_size;
                const auto max  = m_config.max_size;
                if (m_config.max_size > m_config.init_size) {
                    if (!m_capacity_expanded.exchange(true, std::memory_order_acq_rel)) {
                        m_capacity.release(static_cast<std::ptrdiff_t>(max - init));
                    }
                }
            }
            return;
        }
    }

    template<class T>
    requires std::derived_from<T, IConnection>
    void ConnectionPool<T>::wait_for_warmup() const noexcept {
        using namespace std::literals;
        while (!m_pool_ready.load(std::memory_order_acquire)) {
            m_pool_ready.wait(false, std::memory_order_acquire);
        }
        //LOG_INFO << "Connection pool is warmup";
    }

    template<class T> requires std::derived_from<T, IConnection>
    ConnectionPool<T>::~ConnectionPool() {
        for (auto& t : m_threads) {
            t.request_stop();
        }
    }

    template<class T>
    requires std::derived_from<T, IConnection>
    ConnectionPool<T>::AcquireResult ConnectionPool<T>::acquire(const std::chrono::seconds timeout) noexcept {
        using clock = std::chrono::steady_clock;
        const auto deadline = clock::now() + timeout;

        for (;;) {
            {
                std::unique_lock lk(m_mutex);
                if (!m_connections.empty()) {
                    auto conn = std::move(m_connections.front());
                    m_connections.pop();
                    lk.unlock();
                    return wrap_connection(std::move(conn));
                }
            }

            const auto now = clock::now();
            if (now >= deadline) {
                return std::unexpected(ConnectionError::Timeout("Timed out waiting for a connection"));
            }
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);

            if (m_capacity.try_acquire_for(remaining)) {
                // We got a permit; check again for a returned connection before creating a new one.
                {
                    std::unique_lock lk(m_mutex);
                    if (!m_connections.empty()) {
                        auto conn = std::move(m_connections.front());
                        m_connections.pop();
                        lk.unlock();
                        m_capacity.release(); // unused permit; return it
                        //LOG_DEBUG << "Acquired a connection from pool after waiting";
                        return wrap_connection(std::move(conn));
                    }
                }
                std::expected<std::unique_ptr<T>, ConnectionError> result = m_factory->create_connection<T>();
                if (!result) {
                    m_capacity.release();
                    return std::unexpected(result.error());
                }
                return wrap_connection(std::move(result.value()));
            }
            return std::unexpected(ConnectionError::Timeout("Timed out waiting for a connection"));
        }
    }

    template<class T>
    requires std::derived_from<T, IConnection>
    ConnectionManager<T> ConnectionPool<T>::wrap_connection(std::unique_ptr<T> c) noexcept {
        std::weak_ptr<ConnectionPool> weak_self = this->weak_from_this();

        auto releaser = [weak_self](std::unique_ptr<T> returned_conn) noexcept {
            if (!returned_conn)
                return;
            if (auto self = weak_self.lock()) {
                {
                    std::unique_lock<std::mutex> lk(self->m_mutex);
                    self->m_connections.push(std::move(returned_conn));
                }
                self->m_capacity.release();
            }
        };

        return ConnectionManager<T>(std::move(c), std::move(releaser));
    }
}