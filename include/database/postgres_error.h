//
// Created by Shinnosuke Kawai on 2/19/26.
//

#pragma once
#include <string>
#include <format>
#include <core/error/base_error.h>

namespace database {
    struct sql_error: Core::BaseError {
        enum class Type {
            ConnectionFailed, ReconnectFailed, QueryFailed, FlushFailed, PollFailed,
            ConsumeFailed, SocketFailed, Busy, TimeOut, ShuttingDown,
            BadConnection, SqlFileError, TransactionRolledBack,
        };

        static sql_error SqlFileError(const char* str) noexcept {return sql_error{Type::SqlFileError, str};}
        static sql_error FailedToConnect() noexcept {return sql_error{Type::ConnectionFailed, "Failed to connect to postgres"};}
        static sql_error FailedToReconnect(const char* msg) noexcept {return sql_error{Type::ReconnectFailed, msg};}
        static sql_error BadConnection(const char* str) noexcept {return sql_error{Type::BadConnection, str};}
        static sql_error SocketFailed(const char* str) noexcept { return sql_error{Type::SocketFailed, str};}
        static sql_error QueryFailed(const char* str) noexcept { return sql_error{Type::QueryFailed, str};}
        static sql_error ShuttingDown(const char* str) noexcept { return sql_error{Type::ShuttingDown, str};}
        static sql_error TransactionRolledBack() noexcept { return sql_error{Type::TransactionRolledBack, "transaction already rolled back"};}

        Type get_type() const noexcept {return err;}

        std::string to_str() const noexcept override {
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
                case Type::SqlFileError:
                    code_str = "SqlFileError";
                    break;
                case Type::TransactionRolledBack:
                    code_str = "TransactionRolledBack";
                    break;
            }
            return std::format("Postgres: {} {}", code_str, message);
        }
        ~sql_error() noexcept override = default;
    private:
        explicit sql_error(const Type err, std::string&& message): err(err), message(std::move(message)){}
        explicit sql_error(const Type err, const char* message): err(err), message(message){}
    private:
        Type err{};
        std::string message;
    };
}