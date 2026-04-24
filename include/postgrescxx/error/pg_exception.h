//
// Created by Shinnosuke Kawai on 2/19/26.
//

#pragma once
#include <core/error/base_error.h>

namespace postgres_cxx::error {
    enum class types {
        ConnectionFailed, IOError, QueryFailed, EventLoopError,
    };
    class pg_exception: public core::error::typed_error<pg_exception, types> {
    public:
        ERROR_CLASS_CATEGORY(Postgres)
        COPY_SEMANTICS(pg_exception, default);
        MOVE_SEMANTICS(pg_exception, default);
        ~pg_exception() override = default;
    };
}