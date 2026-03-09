# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure (from repo root)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Run a specific test binary
./build/test/DbConnectionPool_tests
./build/test/IntrusivePtr_tests

# Run a single test by name
./build/test/DbConnectionPool_tests --gtest_filter=ConnectionPoolTest.PoolTest
./build/test/IntrusivePtr_tests --gtest_filter=IntrusivePtrTest.MultiThreads
```

**Sanitizers** (Clang/GCC only): Configure with `-DSANITIZER_TYPE=address|thread|undefined|none` (default: `address`).

**Standard**: C++23 required (`std::expected`, `std::print`, `std::jthread`, `std::counting_semaphore`).

## Architecture

This is a **header-only** C++23 library (`DbConnectionPool` is an `INTERFACE` CMake target). All implementation lives in `include/`.

### Two independent subsystems

**1. `include/core/ref.h` — Intrusive reference counting**
- `Core::RefCounted<Derived>` — CRTP base class. Stores an `atomic_uint32_t` ref count. `increment()`/`release()` are private, accessible only to `std_ex::intrusive_ptr<T>` via friendship.
- `std_ex::intrusive_ptr<T>` — Smart pointer for `RefCounted` objects. Provides `make_intrusive<T>(args...)` factory.
- Unlike `shared_ptr`, the ref count lives inside the object (no separate control block). `T` must derive from `RefCounted<T>`.

**2. `include/database/` — Connection pool**

| File | Role |
|------|------|
| `connection.h` | `IConnection` interface; `ConnectionError` value type with typed error codes; `ConnectionResult` alias |
| `connection_factory.h` | `ConnectionFactory` — type-erased factory registry. `register_factory<T>(fn)` stores a creator by `std::type_index`; `create_connection<T>()` looks it up and returns `unique_ptr<T>` |
| `connection_pool.h` | `ConnectionPool<T>` template — the pool itself. Inherits `RefCounted<ConnectionPool<T>>` so it can be managed via `intrusive_ptr`. `PoolConfig` controls `init_size`, `max_size`, and `is_eager` |
| `connection_pool_impl.h` | Implementation of `ConnectionPool<T>` methods (included at the bottom of `connection_pool.h`) |
| `connection_manager.h` | `ConnectionManager<T>` — RAII handle returned by `acquire()`. Returns the connection to the pool on destruction via a captured `intrusive_ptr` to the pool (this keeps the pool alive as long as any manager is alive) |

### Key design decisions

- **Semaphore-based capacity control**: `m_capacity` (a `counting_semaphore`) tracks available connection slots. Initial value is `init_size`; after warmup, it is expanded by `max_size - init_size`.
- **Eager vs. lazy warmup**: When `is_eager = true`, `init_size` `jthread`s are spawned in the constructor to pre-fill the pool via `fill_pool()`. `wait_for_warmup()` blocks until `m_pool_ready` is set.
- **Connection return**: `wrap_connection()` captures an `intrusive_ptr` to `this` inside the `ConnectionManager` releaser lambda. When the manager is destroyed, the lambda pushes the connection back into `m_connections` and calls `m_capacity.release()`.
- **Thread safety**: `m_mutex` guards `m_connections`; `m_shared_mutex` in `ConnectionFactory` allows concurrent reads with exclusive writes.

### CI matrix

GitHub Actions tests against Ubuntu 24.04 (GCC-14, Clang-19), Windows (MSVC), and macOS (Apple Clang, GCC) across Debug / Release / RelWithDebInfo build types.
