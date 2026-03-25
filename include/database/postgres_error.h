//
// Created by Shinnosuke Kawai on 2/19/26.
//

#pragma once
#include <string>
#include <format>
#include <core/error/base_error.h>

namespace database {
    struct sql_error: Core::BaseError {
        enum class type {
            ConnectionFailed, ReconnectFailed, QueryFailed, FlushFailed, PollFailed,
            ConsumeFailed, SocketFailed, Busy, TimeOut, ShuttingDown,
            BadConnection, SqlFileError, TransactionRolledBack,
        };

        static sql_error SqlFileError(const char* str) noexcept {return sql_error{type::SqlFileError, str};}
        static sql_error FailedToConnect() noexcept {return sql_error{type::ConnectionFailed, "Failed to connect to postgres"};}
        static sql_error FailedToReconnect(const char* msg) noexcept {return sql_error{type::ReconnectFailed, msg};}
        static sql_error BadConnection(const char* str) noexcept {return sql_error{type::BadConnection, str};}
        static sql_error SocketFailed(const char* str) noexcept { return sql_error{type::SocketFailed, str};}
        static sql_error QueryFailed(const char* str) noexcept { return sql_error{type::QueryFailed, str};}
        static sql_error ShuttingDown(const char* str) noexcept { return sql_error{type::ShuttingDown, str};}
        static sql_error TransactionRolledBack() noexcept { return sql_error{type::TransactionRolledBack, "transaction already rolled back"};}

        type get_type() const noexcept {return err;}

        std::string to_str() const noexcept override {
            std::string_view code_str;
            switch (err) {
                case type::ConnectionFailed:
                    code_str = "ConnectionFailed";
                    break;
                case type::QueryFailed:
                    code_str = "QueryFailed";
                    break;
                case type::FlushFailed:
                    code_str = "FlushFailed";
                    break;
                case type::PollFailed:
                    code_str = "PollFailed";
                    break;
                case type::ConsumeFailed:
                    code_str = "ConsumeFailed";
                    break;
                case type::SocketFailed:
                    code_str = "SocketFailed";
                    break;
                case type::Busy:
                    code_str = "Busy";
                    break;
                case type::TimeOut:
                    code_str = "TimeOut";
                    break;
                case type::ShuttingDown:
                    code_str = "ShuttingDown";
                    break;
                case type::BadConnection:
                    code_str = "BadConnection";
                    break;
                case type::ReconnectFailed:
                    code_str = "ReconnectFailed";
                    break;
                case type::SqlFileError:
                    code_str = "SqlFileError";
                    break;
                case type::TransactionRolledBack:
                    code_str = "TransactionRolledBack";
                    break;
            }
            std::erase(message, '\n');
            return std::format("Postgres: {} {}", code_str, message);
        }
        ~sql_error() noexcept override = default;
    private:
        explicit sql_error(const type err, std::string&& message): err(err), message(std::move(message)){}
        explicit sql_error(const type err, const char* message): err(err), message(message){}
    private:
        type err{};
        mutable std::string message;
    };
}