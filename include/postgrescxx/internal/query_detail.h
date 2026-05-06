//
// Created by Shinnosuke Kawai on 4/25/26.
//

#pragma once
#include <vector>
#include "type_detail.h"

namespace postgres_cxx {
    class pg_param_detail {
    public:
        std::string_view command() const noexcept { return query; }
        const char** param_values() noexcept {
            return buffers.data();
        }
        const int* param_lengths() const noexcept {
            return lengths.data();
        }
        const int* param_formats() const noexcept {
            return formats.data();
        }

        [[nodiscard]]
        int count() const noexcept { return static_cast<int>(buffers.size()); }

        explicit pg_param_detail(const std::string_view query, const std::span<const supported_type> params)
        : query(std::string{query}),
          text(params.size()),
          buffers(params.size(), nullptr),
          lengths(params.size(), 0),
          formats(params.size(), 0)
        {
            for (std::size_t i = 0; i < params.size(); ++i) {
                formats[i] = 1; // binary format for all params
                if (std::holds_alternative<std::nullptr_t>(params[i])) {
                    buffers[i] = nullptr; // SQL NULL — length and format are ignored by libpq
                    lengths[i] = 0;
                    continue;
                }

                text[i]     = internal::ToBinary(params[i]);
                buffers[i]  = text[i].data();
                lengths[i]  = static_cast<int>(text[i].size());
            }
        }
        pg_param_detail() = default;
        COPY_SEMANTICS(pg_param_detail, delete);
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
    private:
        std::string query;
        std::vector<std::string> text;      // size == n; empty string for NULL
        std::vector<const char*> buffers;    // size == n; nullptr for NULL
        std::vector<int> lengths;           // size == n
        std::vector<int> formats;           // size == n (all 0)
    };

    inline pg_param_detail MakePgParamBuffer(const std::string_view query, const std::span<const supported_type> params)
    {
        pg_param_detail out(query, params);
        return out;
    }

    template <std::size_t N>
    pg_param_detail MakePgParamBuffer(const std::string_view query, const std::array<supported_type, N>& params)
    {
        return MakePgParamBuffer(query,std::span<const supported_type>(params.data(), params.size()));
    }

}