include(FetchContent)

# Component names expected by find_package(Libevent): core, extra, pthreads.
# event_pthreads is POSIX-only — omit it on Windows.
if (WIN32)
    set(_libevent_components core extra)
else ()
    set(_libevent_components core extra pthreads)
endif ()

if (APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    find_package(Libevent QUIET COMPONENTS ${_libevent_components} PATHS "/opt/homebrew/opt/libevent" NO_DEFAULT_PATH)
else ()
    if (APPLE)
        list(PREPEND CMAKE_PREFIX_PATH "/usr/local/opt/libevent")
    endif ()
    find_package(Libevent QUIET COMPONENTS ${_libevent_components})
endif ()
if (NOT Libevent_FOUND)
    message(STATUS "libevent not found — fetching from source")
    FetchContent_Declare(
            libevent
            GIT_REPOSITORY https://github.com/libevent/libevent.git
            GIT_TAG        release-2.1.12-stable
    )

    set(EVENT__DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "" FORCE)
    set(EVENT__LIBRARY_TYPE "STATIC" CACHE STRING "" FORCE)
    set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "" FORCE)

    if (APPLE)
        set(EVENT__FORCE_KQUEUE_CHECK ON CACHE BOOL "" FORCE)
    endif ()
    if (WIN32)
        set(EVENT__MSVC_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
    endif ()

    FetchContent_MakeAvailable(libevent)
endif ()

# event_pthreads is POSIX-only and does not exist on Windows
if (WIN32)
    target_link_libraries(postgresql-cpp-driver PRIVATE event_core event_extra)
else ()
    target_link_libraries(postgresql-cpp-driver PRIVATE event_core event_extra event_pthreads)
endif ()

# Help the linker find Homebrew-installed libevent dylibs on macOS
if (APPLE AND Libevent_FOUND)
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        target_link_directories(postgresql-cpp-driver BEFORE PRIVATE /opt/homebrew/opt/libevent/lib)
    else ()
        target_link_directories(postgresql-cpp-driver BEFORE PRIVATE /usr/local/opt/libevent/lib)
    endif ()
endif ()