//
// Created by Shinnosuke Kawai on 4/25/26.
//
#pragma once
#include <core/error/base_error.h>
#include <core/memory/intrusive_ptr.h>
#include <event2/event.h>

namespace postgres_cxx {
    class event_context: public core::ref_counted<event_context> {
    public:
        explicit event_context(event_base* ev_base): m_ev_base(ev_base) {}
        event_context& operator=(event_context&& other) noexcept {
            if (this != &other) {
                m_ev_base = other.m_ev_base;
                other.m_ev_base = nullptr;
            }
            return *this;
        }
        event_context(event_context&& other) noexcept : m_ev_base(other.m_ev_base) {
            other.m_ev_base = nullptr;
        }
        COPY_SEMANTICS(event_context, delete);
        ~event_context() override {
            if (m_ev_base)
                event_base_free(m_ev_base);
        };
        event_base* ev_base() const noexcept { return m_ev_base; }
    private:
        event_base* m_ev_base = nullptr;
    };
}