//
// Created by Shinnosuke Kawai on 3/17/26.
//

#pragma once
#include <future>
#include <expected>
#include <array>
#include "query_executor.h"
#include "internal/type_detail.h"

namespace database {
    class transaction {
    public:
        explicit transaction(query_executor& executor) noexcept;
        ~transaction();

        transaction(transaction&&) noexcept;
        transaction(const transaction&) = delete;
        transaction& operator=(const transaction&) = delete;
        transaction& operator=(transaction&&) = delete;

        template<typename... param>
        std::future<std::expected<result::table, sql_error>> execute(std::string_view query, param&&... params) {
            const std::array<supported_type, sizeof...(params)> arr = {
                internal::CreateSingleData(std::forward<param>(params))...
            };
            std::unique_lock lock(m_mutex);
            if (m_rollback_sent) {
                std::promise<std::expected<result::table, sql_error>> p;
                p.set_value(std::unexpected(sql_error::TransactionRolledBack()));
                return p.get_future();
            }
            return m_executor->SendToWorker(internal::MakePgParamBuffer(query, arr));
        }

        template<typename... Args>
        void execute_async(std::string_view query, result_callback&& on_success, error_callback&& on_error, Args&&... params) {
            const std::array<supported_type, sizeof...(params)> arr = {
                internal::CreateSingleData(std::forward<Args>(params))...
            };
            std::unique_lock lock(m_mutex);
            if (m_rollback_sent) {
                on_error(sql_error::TransactionRolledBack());
                return;
            }
            m_executor->EnqueueAsync(
                internal::MakePgParamBuffer(query, arr),
                std::move(on_success),
                std::move(on_error));
        }

        void rollback() noexcept; // sends ROLLBACK and waits; marks done

    private:
        std::mutex m_mutex;
        query_executor* m_executor = nullptr;
        bool m_rollback_sent = false;
    };
}
