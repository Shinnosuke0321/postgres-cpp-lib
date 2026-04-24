//
// Created by Shinnosuke Kawai on 3/15/26.
//

#pragma once
#include "colunm.h"

namespace postgres_cxx::result {
    struct row {
        explicit row() = default;

        row(row&& other) noexcept = default;
        row& operator=(row&& other) noexcept = default;
        row(const row&) = delete;
        row& operator=(const row&) = delete;

        template<typename T>
        std::optional<T> get(const std::string_view f_name) const {
            const auto it = m_columns.find(std::string(f_name));
            if (it == m_columns.end())
                return std::nullopt;
            return it->second.as<T>();
        }

        const colum& operator[](const std::string_view f_name) const {
            static const colum kNull{};
            const auto it = m_columns.find(std::string(f_name));
            return it != m_columns.end() ? it->second : kNull;
        }

    private:
        friend class table;
        std::unordered_map<std::string, colum> m_columns = {};
    };
}