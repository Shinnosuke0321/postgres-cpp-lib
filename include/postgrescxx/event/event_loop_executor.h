//
// Created by Shinnosuke Kawai on 4/19/26.
//
#pragma once
#include <expected>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <event2/event.h>
#include <event2/thread.h>
#include <core/memory/intrusive_ptr.h>
#include <core/error/base_error.h>
#include "postgrescxx/error/pg_exception.h"
#include "postgrescxx/context/event_context.h"

namespace postgres_cxx {
    class event_loop_executor: public core::ref_counted<event_loop_executor> {
    public:
        struct init_result_t {
            smart_ptr::intrusive_ptr<event_loop_executor> loop_executor;
            smart_ptr::intrusive_ptr<event_context> event_context;
        };
        static std::expected<init_result_t, error::pg_exception> run_loop() noexcept {
            const auto ev_loop = smart_ptr::intrusive_ptr(new event_loop_executor());
            if (auto res = ev_loop->initialize_event_loop(); !res)
                return std::unexpected(std::move(res.error()));
            return init_result_t{ev_loop->intrusive_from_this(),ev_loop->m_ev_base->intrusive_from_this()};
        }

        void post(std::function<void()>&& task) noexcept {
            if (std::this_thread::get_id() == m_thread_id) {
                task();
                return;
            }
            {
                std::lock_guard lock(m_mutex);
                m_tasks.emplace(std::move(task));
            }
            if (m_wakeup_event)
                event_active(m_wakeup_event, EV_WRITE, 0);
        }
    private:
        std::expected<void, error::pg_exception> initialize_event_loop() noexcept {
            using namespace error;
            using types::EventLoopError;
            std::lock_guard lock(m_mutex);
            if (is_initialized)
                return {};

            auto publish = [this](std::expected<void, pg_exception> res) {
                is_initialized = true;
                return res;
            };

            std::println("Event loop initialization. This should happen only once");
            if (evthread_use_pthreads() < 0) {
                return publish(std::unexpected(CREATE_ERROR(pg_exception, EventLoopError, "Failed to init libevent")));
            }

            m_ev_base = smart_ptr::make_intrusive<event_context>(event_base_new());
            if (!m_ev_base->ev_base())
                return publish(std::unexpected(CREATE_ERROR(pg_exception, EventLoopError, "Failed to init base")));

            m_wakeup_event = event_new(m_ev_base->ev_base(), -1, EV_PERSIST, [](evutil_socket_t, short, void* priv) {
                static_cast<event_loop_executor*>(priv)->drain();
            }, this);

            if (!m_wakeup_event)
                return publish(std::unexpected(CREATE_ERROR(pg_exception, EventLoopError, "Failed to init event")));

            if (event_add(m_wakeup_event, nullptr) < 0)
                return publish(std::unexpected(CREATE_ERROR(pg_exception, EventLoopError, "Failed to register event")));

            std::atomic_bool thread_started{false};
            m_worker_thread = std::jthread([this, &thread_started](const std::stop_token&) {
                m_thread_id = std::this_thread::get_id();
                thread_started.store(true, std::memory_order_release);
                event_base_loop(m_ev_base->ev_base(), EVLOOP_NO_EXIT_ON_EMPTY);
            });
            while (!thread_started.load(std::memory_order_acquire))
                std::this_thread::yield();

            return publish({});
        }
    public:
        event_loop_executor() = default;
        ~event_loop_executor() override {
            stop();
            if (m_wakeup_event)
                event_free(m_wakeup_event);
        }
    private:
        void drain() noexcept {
            std::queue<std::function<void()>> local;
            {
                std::lock_guard lk(m_mutex);
                std::swap(local, m_tasks);
            }
            while (!local.empty()) {
                local.front()();
                local.pop();
            }
            // Break the loop from within the event thread only after all tasks
            // have been drained, so no queued task is silently dropped on shutdown.
            if (m_stopping.load(std::memory_order_acquire))
                event_base_loopbreak(m_ev_base->ev_base());
        }
        void stop() {
            if (m_stopping.exchange(true))
                return;
            // Activate one final drain; drain() will call event_base_loopbreak
            // once the queue is empty, ensuring all pending tasks execute first.
            if (m_wakeup_event)
                event_active(m_wakeup_event, EV_READ, 0);

            if (m_worker_thread.joinable())
                m_worker_thread.join();
        }
    private:
        std::mutex m_mutex;
        smart_ptr::intrusive_ptr<event_context> m_ev_base;
        event* m_wakeup_event = nullptr;
        std::queue<std::function<void()>> m_tasks;
        bool is_initialized{false};
        std::atomic_bool m_stopping{false};
        std::thread::id m_thread_id;
        std::jthread m_worker_thread;
    };
}
