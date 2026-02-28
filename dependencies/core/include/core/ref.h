//
// Created by Shinnosuke Kawai on 2/24/26.
//

#pragma once
#include <atomic>
#include <concepts>
#include <print>

namespace std_ex {
    template<class T>
    class intrusive_ptr;
}

namespace Core {

    template<class Derived>
    class RefCounted {
    public:
        RefCounted() = default;
        std_ex::intrusive_ptr<Derived> intrusive_from_this() noexcept {
            return std_ex::intrusive_ptr<Derived>(static_cast<Derived*>(this));
        }
        std_ex::intrusive_ptr<const Derived> intrusive_from_this() const noexcept {
            return std_ex::intrusive_ptr<const Derived>(static_cast<const Derived*>(this));
        }
        uint32_t ref_count() const noexcept { return m_ref_count.load(std::memory_order_relaxed); }
    protected:
        virtual ~RefCounted() = default;

    private:
        void increment() const noexcept {
            m_ref_count.fetch_add(1, std::memory_order_relaxed);
        }
        bool release() const noexcept {
            if (m_ref_count.fetch_sub(1, std::memory_order_release) == 1) {
                std::atomic_thread_fence(std::memory_order_acquire);
                return true;
            }
            return false;
        }
        template<class T>
        friend class std_ex::intrusive_ptr;
    private:

        mutable std::atomic_uint32_t m_ref_count = 0;
    };
}

namespace std_ex {
    template<class T>
    class intrusive_ptr {
        using U = std::remove_const_t<T>;
        static_assert(std::derived_from<U, Core::RefCounted<U>>, "T must be derived from RefCounted");
    public:
        intrusive_ptr() = default;

        explicit intrusive_ptr(T* instance) noexcept: m_ptr(const_cast<U*>(instance)) {
            increment_ref();
        }
        ~intrusive_ptr() { decrement_ref(); }

        intrusive_ptr(const intrusive_ptr& other) noexcept: m_ptr(other.m_ptr) {
            increment_ref();
        }

        intrusive_ptr& operator=(const intrusive_ptr& other) {
            if (this == &other)
                return *this;
            T* new_temp = other.m_ptr;
            if (new_temp)
                new_temp->increment();
            decrement_ref();
            m_ptr = new_temp;
            return *this;
        }

        intrusive_ptr(intrusive_ptr&& other) noexcept: m_ptr(other.m_ptr) {
            other.m_ptr = nullptr;
        }

        intrusive_ptr& operator=(intrusive_ptr&& other) noexcept {
            if (this == &other)
                return *this;
            decrement_ref();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
            return *this;
        }

        T* get() const noexcept { return m_ptr; }

        explicit operator bool() const { return m_ptr != nullptr; }

        T* operator->() const noexcept { return m_ptr; }
        T& operator*() const noexcept { return *m_ptr; }

        void reset(T* ptr = nullptr) noexcept {
            U* newp = const_cast<U*>(ptr);
            if (newp)
                newp->increment();
            decrement_ref();
            m_ptr = newp;
        }
    private:
        void decrement_ref() noexcept {
            if (m_ptr && m_ptr->release()) {
                delete m_ptr;
            }
            m_ptr = nullptr;
        }
        void increment_ref() const {
            if (m_ptr) {
                m_ptr->increment();
            }
        }
    private:
        T* m_ptr = nullptr;
    };

    template<class T, typename ...Arg>
    intrusive_ptr<T> make_intrusive(Arg&&... args) {
        T* ptr = new T(std::forward<Arg>(args)...);
        return intrusive_ptr(ptr);
    }
}
