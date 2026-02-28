//
// Created by Shinnosuke Kawai on 12/4/25.
//

#pragma once
#include <memory>
#include <shared_mutex>
#include <typeindex>
#include <utility>
#include "connection.h"
#include <functional>

namespace Core::Database {
    class ConnectionFactory {
    public:
        explicit ConnectionFactory() = default;
        ~ConnectionFactory() = default;
        ConnectionFactory(const ConnectionFactory&) = delete;
        ConnectionFactory& operator=(const ConnectionFactory&) = delete;
    public:
        using CreateConnectionFn = std::function<std::expected<std::unique_ptr<IConnection>, ConnectionError>()>;
    public:
        template<class T>
        requires std::derived_from<T, IConnection>
        void register_factory(CreateConnectionFn fn) {
            const auto type_id = std::type_index(typeid(T));
            CreateConnectionFn factory = [fn = std::move(fn)]() mutable -> std::expected<std::unique_ptr<IConnection>, ConnectionError> {
                std::expected<std::unique_ptr<IConnection>, ConnectionError> res = fn();
                if (!res) {
                    return std::unexpected(res.error());
                }

                // Upcast T* -> IConnection*
                auto typed_ptr = std::move(res.value());
                std::unique_ptr<IConnection> base_ptr(typed_ptr.release());
                return base_ptr;
            };
            std::unique_lock lock(m_shared_mutex);
            m_factories[type_id] = std::move(factory);
        }

        template<class T>
        requires std::derived_from<T, IConnection>
        std::expected<std::unique_ptr<T>, ConnectionError> create_connection() {
            const auto type_id = std::type_index(typeid(T));

            std::shared_lock lock(m_shared_mutex);
            const auto it = m_factories.find(type_id);
            if (it == m_factories.end()) {
                char buffer[35] = "No factory registered for type ";
                strcat(buffer, type_id.name());
                return std::unexpected(ConnectionError::FactoryNotRegistered(buffer));
            };
            const CreateConnectionFn& factory = it->second;
            lock.unlock();
            std::expected<std::unique_ptr<IConnection>, ConnectionError> base_res = factory();
            if (!base_res) {
                return std::unexpected(base_res.error());
            }
            std::unique_ptr<IConnection> base_ptr = std::move(base_res.value());
            IConnection* typed_ptr = base_ptr.release();
            return std::unique_ptr<T>(static_cast<T*>(typed_ptr));
        }

    private:
        std::shared_mutex m_shared_mutex;
        std::unordered_map<std::type_index, CreateConnectionFn> m_factories{};
    };
}
