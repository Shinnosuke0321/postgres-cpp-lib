//
// Created by Shinnosuke Kawai on 3/15/26.
//

#pragma once
#include <cstring>
#include <optional>
#include <vector>
#include <postgres_ext.h>

namespace database::result {
    namespace pg_oid {
        constexpr Oid Bool        = 16;
        constexpr Oid Int2        = 21;
        constexpr Oid Int4        = 23;
        constexpr Oid Int8        = 20;
        constexpr Oid Float4      = 700;
        constexpr Oid Float8      = 701;
        constexpr Oid Text        = 25;
        constexpr Oid Varchar     = 1043;
        constexpr Oid Bpchar      = 1042;
        constexpr Oid Bytea       = 17;
        constexpr Oid Timestamp   = 1114;
        constexpr Oid Timestamptz = 1184;
    }

    namespace pg_detail {
        template<typename T>
        T ReadBigEndian(const std::byte* src) noexcept {
            T value{};
            const auto* s = reinterpret_cast<const unsigned char*>(src);
            auto* d = reinterpret_cast<unsigned char*>(&value);
            for (std::size_t i = 0; i < sizeof(T); ++i)
                d[i] = s[sizeof(T) - 1 - i];
            return value;
        }
    }
}


namespace database::result {
    struct colum {
        colum() = default;
        colum(const Oid oid, const char* val, const int val_len, const bool is_null) : is_null(is_null), oid(oid) {
            if (!is_null && val && val_len > 0) {
                data.resize(val_len);
                std::memcpy(data.data(), val, val_len);
            }
        }
        colum(colum&& other) noexcept = default;
        colum& operator=(colum&& other) noexcept = default;
        colum(const colum&) = delete;
        colum& operator=(const colum&) = delete;

        template<typename T>
        std::optional<T> as() const { return std::nullopt; }
    private:
        bool is_null = true;
        Oid  oid = 0;
        std::vector<std::byte> data;
    };
}

namespace database::result {
    template<>
    inline std::optional<bool> colum::as<bool>() const {
        if (is_null || data.empty())
            return std::nullopt;
        if (oid == pg_oid::Bool)
            return data[0] != std::byte{0};
        return std::nullopt;
    }

    template<>
    inline std::optional<int16_t> colum::as<int16_t>() const {
        if (is_null)
            return std::nullopt;
        if (oid == pg_oid::Int2 && data.size() == 2)
            return pg_detail::ReadBigEndian<int16_t>(data.data());
        return std::nullopt;
    }

    template<>
    inline std::optional<int32_t> colum::as<int32_t>() const {
        if (is_null)
            return std::nullopt;
        if (oid == pg_oid::Int4 && data.size() == 4)
            return pg_detail::ReadBigEndian<int32_t>(data.data());
        if (oid == pg_oid::Int2 && data.size() == 2)
            return pg_detail::ReadBigEndian<int16_t>(data.data());
        return std::nullopt;
    }

    template<>
    inline std::optional<int64_t> colum::as<int64_t>() const {
        if (is_null)
            return std::nullopt;
        if (oid == pg_oid::Int8 && data.size() == 8)
            return pg_detail::ReadBigEndian<int64_t>(data.data());
        if (oid == pg_oid::Int4 && data.size() == 4)
            return pg_detail::ReadBigEndian<int32_t>(data.data());
        if (oid == pg_oid::Int2 && data.size() == 2)
            return pg_detail::ReadBigEndian<int16_t>(data.data());
        return std::nullopt;
    }

    template<>
    inline std::optional<float> colum::as<float>() const {
        if (is_null)
            return std::nullopt;
        if (oid == pg_oid::Float4 && data.size() == 4) {
            const auto bits = pg_detail::ReadBigEndian<uint32_t>(data.data());
            float f;
            std::memcpy(&f, &bits, sizeof(float));
            return f;
        }
        return std::nullopt;
    }

    template<>
    inline std::optional<double> colum::as<double>() const {
        if (is_null)
            return std::nullopt;
        if (oid == pg_oid::Float8 && data.size() == 8) {
            const auto bits = pg_detail::ReadBigEndian<uint64_t>(data.data());
            double d;
            std::memcpy(&d, &bits, sizeof(double));
            return d;
        }
        if (oid == pg_oid::Float4 && data.size() == 4) {
            if (const auto f = as<float>())
                return *f;
        }
        return std::nullopt;
    }

    template<>
    inline std::optional<std::string> colum::as<std::string>() const {
        if (is_null)
            return std::nullopt;
        if (oid == pg_oid::Text || oid == pg_oid::Varchar || oid == pg_oid::Bpchar)
            return std::string(reinterpret_cast<const char*>(data.data()), data.size());
        return std::nullopt;
    }

    using timestamp = std::chrono::system_clock::time_point;

    template<>
    inline std::optional<timestamp> colum::as<timestamp>() const {
        if (is_null)
            return std::nullopt;
        if ((oid == pg_oid::Timestamp || oid == pg_oid::Timestamptz) && data.size() == 8) {
            const auto us = pg_detail::ReadBigEndian<int64_t>(data.data());
            // PostgreSQL epoch starts at 2000-01-01 UTC (Unix epoch + 946684800s)
            constexpr int64_t kPgEpochUs = 946684800LL * 1'000'000LL;
            return timestamp{std::chrono::microseconds(us + kPgEpochUs)};
        }
        return std::nullopt;
    }
}