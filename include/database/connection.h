//
// Created by Shinnosuke Kawai on 10/22/25.
//
#pragma once
#include <expected>
#include <memory>
#include "connection_error.h"

namespace database {
    struct IConnection {
        virtual ~IConnection() = default;
    };

    using ConnectionResult = std::expected<std::unique_ptr<IConnection>, connection_error>;
}
