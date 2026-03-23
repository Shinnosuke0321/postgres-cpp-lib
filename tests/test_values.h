//
// Created by Shinnosuke Kawai on 3/20/26.
//

#pragma once
#include <database/internal/type_detail.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace database {

    struct test_row {
        bool col_bool;
        int16_t col_int16;
        int32_t col_int32;
        int64_t col_int64;
        uint16_t col_uint16;
        uint32_t col_uint32;
        uint64_t col_uint64;
        float col_float;
        double col_double;
        std::optional<std::string> col_text;
        std::optional<std::vector<std::byte>> col_byte;
        timestamp col_ts;
    };

    inline test_row make_test_values() {
        return {
            .col_bool   = true,
            .col_int16  = int16_t{42},
            .col_int32  = int32_t{123456},
            .col_int64  = int64_t{9876543210LL},
            .col_uint16 = uint16_t{65000},
            .col_uint32 = uint32_t{4000000000U},
            .col_uint64 = uint64_t{18000000000000000000ULL},
            .col_float  = 3.14f,
            .col_double = 2.718281828,
            .col_text   = "hello postgres",
            .col_byte   = std::vector{
                std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}
            },
            .col_ts     = std::chrono::system_clock::now(),
        };
    }

    // Same as make_test_values() but col_text and col_byte are NULL,
    // exercising std::nullptr_t support.
    inline test_row make_null_test_values() {
        return {
            .col_bool   = true,
            .col_int16  = int16_t{42},
            .col_int32  = int32_t{123456},
            .col_int64  = int64_t{9876543210LL},
            .col_uint16 = uint16_t{65000},
            .col_uint32 = uint32_t{4000000000U},
            .col_uint64 = uint64_t{18000000000000000000ULL},
            .col_float  = 3.14f,
            .col_double = 2.718281828,
            .col_text   = std::nullopt,
            .col_byte   = std::nullopt,
            .col_ts     = std::chrono::system_clock::now(),
        };
    }

} // namespace database::testing::params
