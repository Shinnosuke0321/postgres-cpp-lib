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

namespace database {
    using timestamp = std::chrono::system_clock::time_point;
    using SupportedType = std::variant<std::nullptr_t,
                                       bool,
                                       int16_t, int32_t, int64_t, uint16_t, uint32_t, uint64_t,
                                       double, float,
                                       const char*, char*, std::string,
                                       std::byte,
                                       timestamp>;

    struct PgParamDetail {
        std::string query;
        std::vector<std::string> text;      // size == n; empty string for NULL
        std::vector<const char*> buffers;    // size == n; nullptr for NULL
        std::vector<int> lengths;           // size == n
        std::vector<int> formats;           // size == n (all 0)

        PgParamDetail() = default;
        explicit PgParamDetail(const std::string_view query, const std::size_t n)
        : query(std::string{query}), text(n), buffers(n, nullptr), lengths(n, 0), formats(n, 0) {}

        [[nodiscard]] int count() const noexcept { return static_cast<int>(buffers.size()); }

        PgParamDetail (const PgParamDetail&) = delete;
        PgParamDetail& operator=(const PgParamDetail&) = delete;

        PgParamDetail(PgParamDetail&& other) noexcept
        : query(std::move(other.query)),
          text(std::move(other.text)),
          buffers(std::move(other.buffers)),
          lengths(std::move(other.lengths)),
          formats(std::move(other.formats)) {}

        PgParamDetail& operator=(PgParamDetail&& other) noexcept {
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

namespace database::internal {
    template<class Param>
    constexpr bool IsSupported() noexcept {
        using D = std::decay_t<Param>;
        constexpr bool is_valid = std::is_same_v<D, std::nullptr_t> ||
                                  std::is_same_v<D, std::byte> ||
                                  (std::is_integral_v<D> && !std::is_same_v<D, bool>) ||
                                  std::is_same_v<D, bool> || std::is_same_v<D, float> || std::is_same_v<D, double> ||
                                  std::is_same_v<D, std::string> || std::is_same_v<D, char*> || std::is_same_v<D, const char*> ||
                                  std::is_same_v<D, std::chrono::system_clock::time_point>;
        return is_valid;
    };

    template<typename Integral>
    constexpr SupportedType NormalizeIntegral(Integral data)
    {
        using D = std::decay_t<Integral>;
        static_assert(std::is_integral_v<D> && !std::is_same_v<D, bool>, "Integral type must be signed or unsigned");
        if constexpr (std::is_signed_v<D>) {
            if constexpr (sizeof(D) <= 2)
                return SupportedType{ static_cast<std::int16_t>(data) };
            else if constexpr (sizeof(D) <= 4)
                return SupportedType{ static_cast<std::int32_t>(data) };
            else
                return SupportedType{ static_cast<std::int64_t>(data) };
        }
        else {
            if constexpr (sizeof(D) <= 2)
                return SupportedType{ static_cast<std::uint16_t>(data) };
            else if constexpr (sizeof(D) <= 4)
                return SupportedType{ static_cast<std::uint32_t>(data) };
            else
                return SupportedType{ static_cast<std::uint64_t>(data) };
        }
    }

    template<class Type>
    constexpr SupportedType CreateSingleData(Type&& param)
    {
        static_assert(internal::IsSupported<Type>(), "Allowed types: integral (except bool), bool, float, double, std::string, string literal, Timestamp, std::byte.");
        using D = std::decay_t<Type>;
        if constexpr (std::is_integral_v<D> && !std::is_same_v<D, bool>) {
            return NormalizeIntegral(param);
        }
        else if constexpr (std::is_same_v<D, std::string>) {
            return SupportedType {std::forward<D>(param)};
        }
        else if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*>) {
            return SupportedType {std::string(param)};
        }
        else {
            return SupportedType {param};
        }
    }
}

namespace database::internal {

    // A small helper for std::visit
    template <class... Ts>
    struct Overloaded : Ts... { using Ts::operator()...; };
    template <class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;

    inline std::string ToByteaHex(const std::byte b)
    {
        static constexpr char kHex[] = "0123456789abcdef";
        const auto v = std::to_integer<unsigned>(b);
        std::string out;
        out.reserve(4);
        out += "\\x";
        out += kHex[(v >> 4) & 0xF];
        out += kHex[v & 0xF];
        return out;
    }

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

    // Encodes a SupportedType value into its PostgreSQL binary wire format.
    // Integers/floats: big-endian; text: raw UTF-8 bytes (no null terminator);
    // bool: 1 byte; byte: 1 byte; timestamp: int64 µs since PostgreSQL epoch (2000-01-01 UTC).
    inline std::string ToBinary(const SupportedType& v)
    {
        return std::visit(Overloaded{
            [](std::nullptr_t)          -> std::string { return {}; },
            [](const bool b)     { return std::string(1, b ? '\x01' : '\x00'); },
            [](const std::int16_t x)    -> std::string { return EncodeFixed(x); },
            [](const std::int32_t x)    -> std::string { return EncodeFixed(x); },
            [](const std::int64_t x)    -> std::string { return EncodeFixed(x); },
            [](const std::uint16_t x)   -> std::string { return EncodeFixed(x); },
            [](const std::uint32_t x)   -> std::string { return EncodeFixed(x); },
            [](const std::uint64_t x)   -> std::string { return EncodeFixed(x); },
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
            [](const std::byte b)       -> std::string { return {1, static_cast<char>(b)}; },
            [](const timestamp& tp)     -> std::string {
                using namespace std::chrono;
                // PostgreSQL epoch starts at 2000-01-01 00:00:00 UTC (Unix epoch + 946684800s).
                constexpr std::int64_t kPgEpochOffsetUs = 946684800LL * 1'000'000LL;
                const auto us = duration_cast<microseconds>(tp.time_since_epoch()).count() - kPgEpochOffsetUs;
                return EncodeFixed(us);
            }
        }, v);
    }

    inline PgParamDetail MakePgParamBuffer(const std::string_view query, const std::span<const SupportedType> params)
    {
        PgParamDetail out(query, params.size());

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
    PgParamDetail MakePgParamBuffer(const std::string_view query, const std::array<SupportedType, N>& params)
    {
        return MakePgParamBuffer(query,std::span<const SupportedType>(params.data(), params.size()));
    }

} // namespace Database::internal
