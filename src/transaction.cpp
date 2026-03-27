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
        m_state = other.m_state;
    }

    transaction::~transaction() {
        std::unique_lock lock(m_mutex);
        if (m_state != state::active || m_executor == nullptr) {
            return;
        }
        m_state = state::rollback;
        lock.unlock();

        pg_param_detail rollback_cmd("ROLLBACK", 0);
        auto future = m_executor->SendToWorker(std::move(rollback_cmd));
        auto result = future.get();
        if (!result) {
            std::println(stderr, "{}", result.error().to_str());
        }
    }

    void transaction::commit() noexcept {
        std::unique_lock sl(m_mutex);
        if (m_state != state::active) {
            std::println(stderr, "transaction is not active or rolled back");
            return;
        }
        m_state = state::commit;
        sl.unlock();
        pg_param_detail commit("COMMIT", 0);
        auto commit_future = m_executor->SendToWorker(std::move(commit));
        if (auto result = commit_future.get(); !result) {
            std::println(stderr, "{}", result.error().to_str());
        }
    }

    void transaction::rollback() noexcept {
        std::unique_lock sl(m_mutex);
        if (m_state != state::active) {
            return;
        }
        m_state = state::rollback;
        sl.unlock();
        pg_param_detail rollback("ROLLBACK", 0);
        auto rollback_future = m_executor->SendToWorker(std::move(rollback));
        if (auto result = rollback_future.get(); !result) {
            std::println(stderr, "{}", result.error().to_str());
        }
    }
}
