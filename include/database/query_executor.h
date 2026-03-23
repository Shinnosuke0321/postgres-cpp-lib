//
// Created by Shinnosuke Kawai on 3/18/26.
//

#pragma once
#include <future>
#include <expected>
#include <functional>
#include "internal/type_detail.h"
#include "result/table.h"
#include "postgres_error.h"

namespace database {
    using result_callback = std::function<void(result::table)>;
    using error_callback  = std::function<void(const sql_error&)>;

    class query_executor {
    public:
        virtual ~query_executor() = default;
        virtual std::future<std::expected<result::table, sql_error>> SendToWorker(pg_param_detail&&) const = 0;
        virtual void EnqueueAsync(pg_param_detail&&, result_callback&&, error_callback&&) const noexcept = 0;
    };
}
