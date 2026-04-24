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

namespace postgres_cxx {
    class event_loop_executor: public core::ref_counted<event_loop_executor> {
    public:
        std::expected<void, error::pg_exception> init() noexcept {
            using namespace error;
            using types::EventLoopError;
            static std::once_flag evthread_init;
            std::call_once(evthread_init, [] {
                if (evthread_use_pthreads() < 0)
                    std::println("Redis: Failed to initialize libevent");
                else
                    std::println("Redis: Initialized libevent");
            });
            if (m_ready.load(std::memory_order_acquire))
                return {};

            m_base = event_base_new();
            if (!m_base) {
                return std::unexpected(CREATE_ERROR(pg_exception, EventLoopError, "Failed to init base"));
            }
            m_wakeup_event = event_new(m_base, -1, EV_PERSIST, [](evutil_socket_t, short, void* priv) {
                static_cast<event_loop_executor*>(priv)->drain();
            }, this);

            if (!m_wakeup_event) {
                return std::unexpected(CREATE_ERROR(pg_exception, EventLoopError, "Failed to init event"));
            }
            if (event_add(m_wakeup_event, nullptr) < 0) {
                return std::unexpected(CREATE_ERROR(pg_exception, EventLoopError, "Failed to register event"));
            }

            m_worker_thread = std::jthread([this](const std::stop_token&) {
                m_thread_id = std::this_thread::get_id();
                m_ready.store(true, std::memory_order_release);
                event_base_loop(m_base, EVLOOP_NO_EXIT_ON_EMPTY);
            });
            while (!m_ready.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            return {};
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

        event_base* base() const noexcept { return m_base; }
    public:
        event_loop_executor() = default;
        ~event_loop_executor() override {
            stop();
            if (m_wakeup_event)
                event_free(m_wakeup_event);
            if (m_base)
                event_base_free(m_base);
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
                event_base_loopbreak(m_base);
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
        event_base* m_base = nullptr;
        event* m_wakeup_event = nullptr;
        std::queue<std::function<void()>> m_tasks;
        std::atomic_bool m_ready{false};
        std::atomic_bool m_stopping{false};
        std::thread::id m_thread_id;
        std::jthread m_worker_thread;
    };
}
