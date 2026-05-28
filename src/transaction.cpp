//
// Created by Shinnosuke Kawai on 3/17/26.
//

#include "postgrescxx/transaction.h"
#include "postgrescxx/pg_transport.h"
using namespace postgres_cxx::error;
using types::ConnectionFailed;
namespace postgres_cxx {
    transaction::transaction(smart_ptr::intrusive_ptr<pg_transport> transport): m_transport(std::move(transport)) {}
    transaction::~transaction() {
        cleanup();
    }

    void pg_transport::make_transaction(std::function<void(std::expected<transaction, pg_exception>)> &&on_transaction_created) noexcept {
        auto self = this->intrusive_from_this();
        m_exe->post([self, on_txn_created = std::move(on_transaction_created)] mutable {
            if (!self->m_connector.is_connected()) {
                on_txn_created(MAKE_UNEXPECTED_ERROR(pg_exception, ConnectionFailed, "not connected"));
                return;
            }
            self->m_query_executor.run_query(
                pg_param_detail("BEGIN", std::span<supported_type>{}),
                [self, on_txn_created = std::move(on_txn_created)](std::expected<result::table, pg_exception> res) mutable {
                    if (!res) {
                        on_txn_created(std::unexpected(std::move(res.error())));
                        return;
                    }
                    on_txn_created(transaction(self->intrusive_from_this()));
            });
        });
    }

    void transaction::cleanup() noexcept {

    }
}
