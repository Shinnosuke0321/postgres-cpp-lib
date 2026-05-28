//
// Created by Shinnosuke Kawai on 3/17/26.
//

#pragma once
#include <core/error/base_error.h>
#include "core/memory/intrusive_ptr.h"

namespace postgres_cxx { class pg_transport; }

namespace postgres_cxx {
    class transaction {
    public:
        explicit transaction(smart_ptr::intrusive_ptr<pg_transport> transport);
        ~transaction();
    public:
        COPY_SEMANTICS(transaction, delete);
        MOVE_SEMANTICS(transaction, default);
    private:
        void cleanup() noexcept;
    private:
        smart_ptr::intrusive_ptr<pg_transport> m_transport;
    };
}
