//
// Created by Shinnosuke Kawai on 3/12/26.
//

#pragma once
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include "row.h"

namespace database::result {
    struct result_deleter {
        void operator()(PGresult* result) const noexcept {
            if (result) PQclear(result);
        }
    };

    using unique_pg_result = std::unique_ptr<PGresult, result_deleter>;

    class table {
    public:
        explicit table(unique_pg_result pg_res) : m_pg_res(std::move(pg_res)) {
            const int n_rows = PQntuples(m_pg_res.get());
            const int n_cols = PQnfields(m_pg_res.get());

            std::vector<std::string> col_names(n_cols);
            for (int c = 0; c < n_cols; ++c)
                col_names[c] = PQfname(m_pg_res.get(), c);

            for (int r = 0; r < n_rows; ++r) {
                row row{};
                row.m_columns.reserve(n_cols);
                for (int c = 0; c < n_cols; ++c) {
                    const Oid oid = PQftype(m_pg_res.get(), c);
                    int val_len = 0;
                    char* val = nullptr;
                    const bool is_null = PQgetisnull(m_pg_res.get(), r, c);
                    if (!is_null) {
                        val_len = PQgetlength(m_pg_res.get(), r, c);
                        val = PQgetvalue(m_pg_res.get(), r, c);
                    }
                    colum column{oid, val, val_len, is_null};
                    row.m_columns[col_names[c]] = std::move(column);
                }
                m_table.emplace_back(std::move(row));
            }
        }

        table(const table&) = delete;
        table& operator=(const table&) = delete;

        table(table&& other) noexcept = default;
        table& operator=(table&& other) noexcept = default;

        const std::deque<row>& rows() const noexcept {
            return m_table;
        }

        size_t size() const noexcept {
            return m_table.size();
        }

    private:
        std::deque<row> m_table;
        unique_pg_result  m_pg_res;
    };
}
