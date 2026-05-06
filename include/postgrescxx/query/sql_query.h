//
// Created by Shinnosuke Kawai on 4/25/26.
//
#pragma once
#include <expected>
#include <functional>
#include "postgrescxx/result/table.h"
#include "postgrescxx/error/pg_exception.h"
#include "postgrescxx/internal/query_detail.h"

namespace postgres_cxx {
    class sql_query {
    public:
        using on_result_t = std::function<void(std::expected<result::table, error::pg_exception>)>;
        sql_query(pg_param_detail&& detail, on_result_t&& on_result) noexcept
            : m_detail(std::move(detail)), m_on_result(std::move(on_result)) {}
        pg_param_detail m_detail{};
        on_result_t m_on_result = nullptr;
    };
}
