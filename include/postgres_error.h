//
// Created by Shinnosuke Kawai on 2/19/26.
//

#pragma once
#include <string>
#include <format>

namespace Database {
    struct PostgresErr {
        enum class Type {
            ConnectionFailed, ReconnectFailed, QueryFailed, FlushFailed, PollFailed,
            ConsumeFailed, SocketFailed, Busy, TimeOut, ShuttingDown,
            BadConnection
        };

        static PostgresErr FailedToConnect() noexcept {return PostgresErr{Type::ConnectionFailed, "Failed to connect to postgres"};}
        static PostgresErr FailedToReconnect(const char* msg) noexcept {return PostgresErr{Type::ReconnectFailed, msg};}
        static PostgresErr BadConnection(const char* str) noexcept {return PostgresErr{Type::BadConnection, str};}
        static PostgresErr SocketFailed(const char* str) noexcept { return PostgresErr{Type::SocketFailed, str};}
        static PostgresErr QueryFailed(const char* str) noexcept { return PostgresErr{Type::QueryFailed, str};}
        static PostgresErr ShuttingDown(const char* str) noexcept { return PostgresErr{Type::ShuttingDown, str};}

        Type get_type() const noexcept {return err;}

        std::string to_str() const noexcept {
            std::string_view code_str;
            switch (err) {
                case Type::ConnectionFailed:
                    code_str = "ConnectionFailed";
                    break;
                case Type::QueryFailed:
                    code_str = "QueryFailed";
                    break;
                case Type::FlushFailed:
                    code_str = "FlushFailed";
                    break;
                case Type::PollFailed:
                    code_str = "PollFailed";
                    break;
                case Type::ConsumeFailed:
                    code_str = "ConsumeFailed";
                    break;
                case Type::SocketFailed:
                    code_str = "SocketFailed";
                    break;
                case Type::Busy:
                    code_str = "Busy";
                    break;
                case Type::TimeOut:
                    code_str = "TimeOut";
                    break;
                case Type::ShuttingDown:
                    code_str = "ShuttingDown";
                    break;
                case Type::BadConnection:
                    code_str = "BadConnection";
                    break;
                case Type::ReconnectFailed:
                    code_str = "ReconnectFailed";
                    break;
            }
            return std::format("Postgres: {} {}", code_str, message);
        }
    private:
        explicit PostgresErr(const Type err, std::string&& message): err(err), message(std::move(message)){}
        explicit PostgresErr(const Type err, const char* message): err(err), message(message){}
    private:
        Type err{};
        std::string message;
    };
}