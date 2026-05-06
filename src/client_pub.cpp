//
// Created by Shinnosuke Kawai on 4/19/25.
//
#include "postgrescxx/client.h"

namespace postgres_cxx {
    std::expected<void, core::error::exception> client::connect() const noexcept {
        if (m_init_err) {
            auto err = m_init_err->to_exception();
            return std::unexpected(std::move(err));
        }
        std::promise<std::expected<void, core::error::exception>> prom;
        auto fut = prom.get_future();
        m_transport_ptr->connect_async([&prom](std::expected<void, error::pg_exception> res) {
            if (res) {
                prom.set_value({});
            } else {
                auto err = res.error().to_exception();
                prom.set_value(std::unexpected(std::move(err)));
            }
        });
        return fut.get();
    }

    bool client::is_connected() const noexcept {
        return m_transport_ptr->check_connection();
    }
}