//
// Created by Shinnosuke Kawai on 10/22/25.
//

#pragma once
#include "connection.h"
#include <functional>

namespace Core::Database {
    template<class T>
    requires std::derived_from<T, IConnection>
    class ConnectionManager {
    public:
        using Releaser = std::function<void(std::unique_ptr<T>)>;

        explicit ConnectionManager(std::unique_ptr<T> connection, Releaser&& releaser)
            : m_connection(std::move(connection)), m_releaser(std::move(releaser)) {}

        ConnectionManager(ConnectionManager&& other) noexcept
        : m_connection(std::move(other.m_connection)),
          m_releaser(std::move(other.m_releaser)) {
            other.m_connection.reset();
            other.m_releaser = nullptr;
        }
        ConnectionManager& operator=(ConnectionManager&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            m_connection = std::move(other.m_connection);
            m_releaser = std::move(other.m_releaser);
            other.m_connection.reset();
            other.m_releaser = nullptr;
            return *this;
        };

        ConnectionManager(ConnectionManager const&) = delete;
        ConnectionManager& operator=(ConnectionManager const&) = delete;

        ~ConnectionManager() {
            release();
        }

        T& operator*() { return *m_connection; }
        const T& operator*() const { return *m_connection; }
        T* operator->() { return m_connection.get();}
        const T* operator->() const { return m_connection.get();}
    private:
        void release() noexcept {
            if (m_connection && m_releaser)
                m_releaser(std::move(m_connection));
            else
                m_connection.reset();
        }
    private:
        std::unique_ptr<T> m_connection;
        Releaser m_releaser;
    };
}
