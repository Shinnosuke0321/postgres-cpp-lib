//
// Created by Shinnosuke Kawai on 4/10/26.
//
#pragma once
#include <string_view>
#include "core/error/base_error.h"

namespace database {
    enum class conn_err_types {
        ConnectionFailed,
        MissingConfig,
        FactoryNotRegistered,
        Timeout,
        SocketFailed,
        AuthFailed
    };
    class connection_error: public core::error::typed_error<connection_error, conn_err_types> {
    public:
        ERROR_CLASS_CATEGORY(connection);
        connection_error(const connection_error&) = default;
        connection_error& operator=(const connection_error&) = default;
        connection_error(connection_error&&) noexcept = default;
        connection_error& operator=(connection_error&&) noexcept = default;
        ~connection_error() override = default;
    };
}