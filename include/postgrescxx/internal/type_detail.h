//
// Created by Shinnosuke Kawai on 3/2/26.
//

#pragma once
#include <bit>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <print>
#include <string>
#include <type_traits>
#include <span>
#include <variant>
#include <vector>

namespace postgres_cxx {
    using timestamp = std::chrono::system_clock::time_point;
    using supported_type = std::variant<std::nullptr_t,
                                        bool,
                                        int16_t, int32_t, int64_t, uint16_t, uint32_t, uint64_t,
                                        double, float,
                                        const char*, char*, std::string,
                                        std::vector<std::byte>,
                                        timestamp>;

    struct pg_param_detail {
        std::string query;
        std::vector<std::string> text;      // size == n; empty string for NULL
        std::vector<const char*> buffers;    // size == n; nullptr for NULL
        std::vector<int> lengths;           // size == n
        std::vector<int> formats;           // size == n (all 0)

        pg_param_detail() = default;
        explicit pg_param_detail(const std::string_view query, const std::size_t n)
        : query(std::string{query}), text(n), buffers(n, nullptr), lengths(n, 0), formats(n, 0) {}

        [[nodiscard]] int count() const noexcept { return static_cast<int>(buffers.size()); }

        pg_param_detail (const pg_param_detail&) = delete;
        pg_param_detail& operator=(const pg_param_detail&) = delete;

        pg_param_detail(pg_param_detail&& other) noexcept
        : query(std::move(other.query)),
          text(std::move(other.text)),
          buffers(std::move(other.buffers)),
          lengths(std::move(other.lengths)),
          formats(std::move(other.formats)) {}

        pg_param_detail& operator=(pg_param_detail&& other) noexcept {
            if (this != &other) {
                query = std::move(other.query);
                text = std::move(other.text);
                buffers = std::move(other.buffers);
                lengths = std::move(other.lengths);
                formats = std::move(other.formats);
            }
            return *this;
        }
    };
}

namespace postgres_cxx::internal {
    template<class Param>
    constexpr bool IsSupported() noexcept {
        using D = std::decay_t<Param>;
        constexpr bool is_valid = std::is_same_v<D, std::nullptr_t> ||
                                  std::is_same_v<D, std::vector<std::byte>> ||
                                  std::is_same_v<D, std::span<const std::byte>> ||
                                  (std::is_integral_v<D> && !std::is_same_v<D, bool>) ||
                                  std::is_same_v<D, bool> || std::is_same_v<D, float> || std::is_same_v<D, double> ||
                                  std::is_same_v<D, std::string> || std::is_same_v<D, char*> || std::is_same_v<D, const char*> ||
                                  std::is_same_v<D, std::chrono::system_clock::time_point>;
        return is_valid;
    };

    template<typename Integral>
    constexpr supported_type NormalizeIntegral(Integral data)
    {
        using D = std::decay_t<Integral>;
        static_assert(std::is_integral_v<D> && !std::is_same_v<D, bool>, "Integral type must be signed or unsigned");
        if constexpr (std::is_signed_v<D>) {
            if constexpr (sizeof(D) <= 2)
                return supported_type{ static_cast<std::int16_t>(data) };
            else if constexpr (sizeof(D) <= 4)
                return supported_type{ static_cast<std::int32_t>(data) };
            else
                return supported_type{ static_cast<std::int64_t>(data) };
        }
        else {
            // Unsigned types are widened to the next signed type because PostgreSQL
            // has no native unsigned integers:
            //   uint8_t → int16_t (SMALLINT, 2 bytes)
            //   uint16_t → int32_t (INTEGER, 4 bytes)
            //   uint32_t → int64_t (BIGINT, 8 bytes)
            //   uint64_t → uint64_t (kept; ToBinary encodes it as NUMERIC binary)
            if constexpr (sizeof(D) == 1)
                return supported_type{ static_cast<std::int16_t>(data) };
            else if constexpr (sizeof(D) == 2)
                return supported_type{ static_cast<std::int32_t>(data) };
            else if constexpr (sizeof(D) == 4)
                return supported_type{ static_cast<std::int64_t>(data) };
            else
                return supported_type{ static_cast<std::uint64_t>(data) };
        }
    }

    template<class Type>
    constexpr supported_type CreateSingleData(Type&& param)
    {
        static_assert(internal::IsSupported<Type>(),
            "Allowed types: integral (except bool), bool, float, double, std::string, string literal, Timestamp, vector of std::byte");
        using D = std::decay_t<Type>;
        if constexpr (std::is_integral_v<D> && !std::is_same_v<D, bool>) {
            return NormalizeIntegral(param);
        }
        else if constexpr (std::is_same_v<D, std::string> || std::is_same_v<D, std::vector<std::byte>>) {
            return supported_type {std::forward<Type>(param)};
        }
        else if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*>) {
            return supported_type {std::string(param)};
        }
        else if constexpr (std::is_same_v<D, std::span<const std::byte>>) {
            return supported_type {std::vector<std::byte>(param.begin(), param.end())};
        }
        else {
            return supported_type {param};
        }
    }
}

namespace postgres_cxx::internal {

    // A small helper for std::visit
    template <class... Ts>
    struct Overloaded : Ts... { using Ts::operator()...; };
    template <class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;

    // Convert Timestamp to a simple ISO-ish UTC string: YYYY-MM-DD HH:MM:SSZ
    inline std::string ToTimestampString(const timestamp& tp)
    {
        using namespace std::chrono;
        const auto secs = time_point_cast<seconds>(tp);
        const std::time_t tt = system_clock::to_time_t(secs);

        std::tm tm{};
    #if defined(_WIN32)
        gmtime_s(&tm, &tt);
    #else
        gmtime_r(&tt, &tm);
    #endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%SZ");
        return oss.str();
    }

    // Returns value in big-endian (network) byte order.
    template<typename T>
    T ToNetworkOrder(T value) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            static_assert(std::is_trivially_copyable_v<T>);
            T result = std::byteswap(value);
            return result;
        }
    }

    // Encodes a fixed-size integer/float type as big-endian bytes into a std::string.
    template<typename T>
    std::string EncodeFixed(T value) noexcept {
        const T net = ToNetworkOrder(value);
        std::string out(sizeof(T), '\0');
        std::memcpy(out.data(), &net, sizeof(T));
        return out;
    }

    // Encodes a uint64_t as a PostgreSQL NUMERIC binary value (big-endian).
    // Layout: ndigits(2) | weight(2) | sign(2) | dscale(2) | digits[ndigits](2 each)
    // Each digit is a base-10000 group, most significant first.
    inline std::string EncodeNumericUint64(const std::uint64_t value) noexcept {
        if (value == 0) {
            // ndigits=0, weight=0, sign=NUMERIC_POS=0, dscale=0
            return {8, '\0'};
        }

        std::vector<std::uint16_t> raw;
        std::uint64_t v = value;
        while (v > 0) {
            raw.push_back(static_cast<std::uint16_t>(v % 10000));
            v /= 10000;
        }
        std::reverse(raw.begin(), raw.end()); // most-significant first

        const auto total_groups = static_cast<std::int16_t>(raw.size());
        const auto weight = static_cast<int16_t>(total_groups - 1);

        // Strip trailing zero digits (valid for integer NUMERIC, dscale=0)
        while (raw.size() > 1 && raw.back() == 0)
            raw.pop_back();

        const auto digitCount = static_cast<std::int16_t>(raw.size());

        std::string out;
        out.reserve(8 + digitCount * 2);
        out += EncodeFixed(digitCount);
        out += EncodeFixed(weight);
        out += EncodeFixed(std::int16_t{0}); // sign = NUMERIC_POS
        out += EncodeFixed(std::int16_t{0}); // dscale = 0 (integer)
        for (const auto d : raw)
            out += EncodeFixed(d);
        return out;
    }

    // Encodes a SupportedType value into its PostgreSQL binary wire format.
    // Integers/floats: big-endian; text: raw UTF-8 bytes (no null terminator);
    // bool: 1 byte; byte: 1 byte; timestamp: int64 µs since PostgreSQL epoch (2000-01-01 UTC).
    inline std::string ToBinary(const supported_type& v)
    {
        return std::visit(Overloaded{
            [](std::nullptr_t)          -> std::string { return {}; },
            [](const bool b)     { return std::string(1, b ? '\x01' : '\x00'); },
            [](const std::int16_t x)    -> std::string { return EncodeFixed(x); },
            [](const std::int32_t x)    -> std::string { return EncodeFixed(x); },
            [](const std::int64_t x)    -> std::string { return EncodeFixed(x); },
            [](const std::uint16_t x)   -> std::string { return EncodeFixed(x); },
            [](const std::uint32_t x)   -> std::string { return EncodeFixed(x); },
            [](const std::uint64_t x)   -> std::string { return EncodeNumericUint64(x); },
            [](const double x)          -> std::string {
                std::uint64_t bits;
                std::memcpy(&bits, &x, sizeof(uint64_t));
                return EncodeFixed(bits);
            },
            [](const float x)           -> std::string {
                std::uint32_t bits;
                std::memcpy(&bits, &x, sizeof(uint32_t));
                return EncodeFixed(bits);
            },
            [](const std::string& s)    -> std::string { return s; },
            [](const char* s)           -> std::string { return s ? std::string{s} : std::string{}; },
            [](const std::vector<std::byte>& bytes) -> std::string {
                std::string out(bytes.size(), '\0');
                std::memcpy(out.data(), bytes.data(), bytes.size());
                return out;
            },
            [](const timestamp& tp)     -> std::string {
                using namespace std::chrono;
                // PostgreSQL epoch starts at 2000-01-01 00:00:00 UTC (Unix epoch + 946684800s).
                constexpr std::int64_t kPgEpochOffsetUs = 946684800LL * 1'000'000LL;
                const auto us = duration_cast<microseconds>(tp.time_since_epoch()).count() - kPgEpochOffsetUs;
                return EncodeFixed(us);
            }
        }, v);
    }

    inline pg_param_detail MakePgParamBuffer(const std::string_view query, const std::span<const supported_type> params)
    {
        pg_param_detail out(query, params.size());

        for (std::size_t i = 0; i < params.size(); ++i) {
            out.formats[i] = 1; // binary format for all params
            if (std::holds_alternative<std::nullptr_t>(params[i])) {
                out.buffers[i] = nullptr; // SQL NULL — length and format are ignored by libpq
                out.lengths[i] = 0;
                continue;
            }

            out.text[i]     = ToBinary(params[i]);
            out.buffers[i]  = out.text[i].data();
            out.lengths[i]  = static_cast<int>(out.text[i].size());
        }

        return out;
    }

    template <std::size_t N>
    pg_param_detail MakePgParamBuffer(const std::string_view query, const std::array<supported_type, N>& params)
    {
        return MakePgParamBuffer(query,std::span<const supported_type>(params.data(), params.size()));
    }

} // namespace Database::internal
