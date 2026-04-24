//
// Created by Shinnosuke Kawai on 4/19/25.
//
#include "postgrescxx/client.h"

namespace postgres_cxx {
    std::expected<void, core::error::exception> client::connect() const noexcept {
        std::promise<std::expected<void, core::error::exception>> prom;
        auto fut = prom.get_future();
        m_transport_ptr->connect_async(m_uri, [&prom](std::expected<void, error::pg_exception> res) {
            if (res) {
                prom.set_value({});
            } else {
                auto err = res.error().to_exception();
                prom.set_value(std::unexpected(std::move(err)));
            }
        });
        return fut.get();
    }
}