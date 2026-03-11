//
// Created by Shinnosuke Kawai on 3/11/26.
//
#pragma once
#include <string>

namespace Core {
    struct BaseError {
        virtual ~BaseError() = default;
        virtual std::string to_str() const noexcept = 0;
    };
}