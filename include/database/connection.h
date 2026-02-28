//
// Created by Shinnosuke Kawai on 10/22/25.
//
#pragma once
#include <expected>
#include <string>
#include <memory>

namespace Core::Database {
    struct IConnection {
        virtual ~IConnection() = default;
    };

    struct ConnectionError {
        enum class Type {
            ConnectionFailed, MissingConfig, FactoryNotRegistered, Timeout, SocketFailed, AuthFailed
        };
        static ConnectionError ConnectionFailed(const char* str) noexcept {
            return ConnectionError{Type::ConnectionFailed, str};
        }
        static ConnectionError MissingConfig(const char* str) noexcept {
            return ConnectionError{Type::MissingConfig, str};
        }
        static ConnectionError FactoryNotRegistered(const char* str) noexcept {
            return ConnectionError{Type::FactoryNotRegistered, str};
        }
        static ConnectionError Timeout(const char* str) noexcept {
            return ConnectionError{Type::Timeout, str};
        }
        static ConnectionError SocketFailed(const char* str) noexcept {
            return ConnectionError{Type::SocketFailed, str};
        }
        static ConnectionError AuthFailed(const char* str) noexcept {
            return ConnectionError{Type::AuthFailed, str};
        }

        std::string& to_str() & noexcept { return m_message; }

        Type get_code() const noexcept { return type; }
    private:
        ConnectionError(const Type type, std::string message) : type(type), m_message(std::move(message)) {}
        ConnectionError(const Type type, const char* message) : type(type), m_message(message) {}
        ConnectionError(const Type type, std::string&& message) : type(type), m_message(std::move(message)) {}

    private:
        Type type;
        std::string m_message{};
    };

    using ConnectionResult = std::expected<std::unique_ptr<IConnection>, ConnectionError>;
}
