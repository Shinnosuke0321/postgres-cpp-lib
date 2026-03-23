//
// Created by Shinnosuke Kawai on 3/17/26.
//

#include "database/transaction.h"

namespace database {
    transaction::transaction(query_executor& executor) noexcept
    : m_executor(&executor) {}

    transaction::transaction(transaction&& other) noexcept {
        std::unique_lock lock(other.m_mutex);
        m_executor = other.m_executor;
        other.m_executor = nullptr;
        m_rollback_sent = other.m_rollback_sent;
        other.m_rollback_sent = true;
    }

    transaction::~transaction() {
        std::unique_lock sl(m_mutex);
        if (!m_rollback_sent) {
            pg_param_detail commit("COMMIT", 0);
            auto worker_future = m_executor->SendToWorker(std::move(commit));
            if (auto result = worker_future.get(); !result) {
                std::println(stderr, "{}", result.error().to_str());
            }
        }
    }

    void transaction::rollback() noexcept {
        std::unique_lock sl(m_mutex);
        m_rollback_sent = true;
        pg_param_detail rollback("ROLLBACK", 0);
        auto rollback_future = m_executor->SendToWorker(std::move(rollback));
        if (auto result = rollback_future.get(); !result) {
            std::println(stderr, "{}", result.error().to_str());
        }
    }
}
