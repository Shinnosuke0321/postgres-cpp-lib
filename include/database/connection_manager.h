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

        explicit ConnectionManager(std::unique_ptr<T> connection, Releaser releaser)
            : m_connection(std::move(connection)), m_releaser(std::move(releaser)) {}

        ConnectionManager(ConnectionManager&& other) noexcept = default;
        ConnectionManager& operator=(ConnectionManager&& other) noexcept = default;

        ConnectionManager(ConnectionManager const&) = delete;
        ConnectionManager& operator=(ConnectionManager const&) = delete;

        ~ConnectionManager() {
            if (m_connection && m_releaser)
                m_releaser(std::move(m_connection));
        }

        T& operator*() { return *m_connection; }
        const T& operator*() const { return *m_connection; }
        T* operator->() { return m_connection.get();}
        const T* operator->() const { return m_connection.get();}
    private:
        std::unique_ptr<T> m_connection;
        Releaser m_releaser;
    };
}
