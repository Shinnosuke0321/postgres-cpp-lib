# cmake/libevent.cmake
# Finds libevent using two strategies in order:
#   1. find_package(Libevent) — works on CMake < 4.0 (FindLibevent.cmake was
#      removed in CMake 4.0; on 4.x this silently no-ops).
#   2. pkg_check_modules      — works whenever .pc files are present (Homebrew,
#      apt, dnf, etc.) and is the primary path on CMake 4.x.
# Falls back to platform-specific installation and retries both strategies.
# Windows last resort: build libevent from source via FetchContent.

# Unified found flag and link-target list (populated by whichever strategy wins).
set(_libevent_found FALSE)
set(_libevent_link_targets)

# Clear cached NOTFOUND values left by a previous find_package(Libevent) so
# subsequent calls actually re-search (e.g. after brew/apt install).
macro(_libevent_clear_cache)
    unset(LIBEVENT_LIBRARY          CACHE)
    unset(LIBEVENT_CORE_LIBRARY     CACHE)
    unset(LIBEVENT_PTHREADS_LIBRARY CACHE)
    unset(LIBEVENT_INCLUDE_DIR      CACHE)
endmacro()

# Strategy 1: CMake's FindLibevent module (CMake 3.19–3.x; absent in 4.x).
macro(_libevent_try_find_package)
    if(NOT _libevent_found)
        _libevent_clear_cache()
        if(WIN32)
            find_package(Libevent QUIET COMPONENTS core)
        else()
            find_package(Libevent QUIET COMPONENTS core pthreads)
        endif()
        if(Libevent_FOUND)
            set(_libevent_found TRUE)
            if(WIN32)
                list(APPEND _libevent_link_targets Libevent::core)
            else()
                list(APPEND _libevent_link_targets Libevent::core Libevent::pthreads)
            endif()
        endif()
    endif()
endmacro()

# Strategy 2: pkg-config (primary path on CMake 4.x where FindLibevent is gone).
# IMPORTED_TARGET makes CMake create a proper PkgConfig::<prefix> target.
macro(_libevent_try_pkg_config)
    if(NOT _libevent_found)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LIBEVENT_CORE QUIET IMPORTED_TARGET libevent_core)
            if(LIBEVENT_CORE_FOUND)
                set(_libevent_found TRUE)
                list(APPEND _libevent_link_targets PkgConfig::LIBEVENT_CORE)
                if(NOT WIN32)
                    pkg_check_modules(LIBEVENT_PTHREADS QUIET IMPORTED_TARGET libevent_pthreads)
                    if(LIBEVENT_PTHREADS_FOUND)
                        list(APPEND _libevent_link_targets PkgConfig::LIBEVENT_PTHREADS)
                    endif()
                endif()
            endif()
        endif()
    endif()
endmacro()

# On macOS, libevent is keg-only so its prefix is not on the default paths.
# Extend CMAKE_PREFIX_PATH and PKG_CONFIG_PATH before searching.
macro(_libevent_prepend_brew_prefix)
    find_program(_brew brew)
    if(_brew)
        execute_process(COMMAND "${_brew}" --prefix libevent
                OUTPUT_VARIABLE _brew_libevent_prefix
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE _brew_result)
        if(_brew_result EQUAL 0 AND _brew_libevent_prefix)
            list(PREPEND CMAKE_PREFIX_PATH "${_brew_libevent_prefix}")
            set(ENV{PKG_CONFIG_PATH}
                "${_brew_libevent_prefix}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
        endif()
        unset(_brew_result)
        unset(_brew_libevent_prefix)
    endif()
    unset(_brew CACHE)
endmacro()

# ── Initial search ────────────────────────────────────────────────────────────
if(APPLE)
    _libevent_prepend_brew_prefix()
endif()
_libevent_try_find_package()
_libevent_try_pkg_config()

# ── Platform-specific acquisition fallback ────────────────────────────────────
if(NOT _libevent_found)
    message(STATUS "libevent not found — attempting platform-specific acquisition")

    if(APPLE)
        # ── macOS: Homebrew ──────────────────────────────────────────────────
        find_program(_brew brew)
        if(_brew)
            message(STATUS "Installing libevent via Homebrew...")
            execute_process(
                COMMAND "${_brew}" install libevent
                RESULT_VARIABLE _brew_result
                OUTPUT_QUIET ERROR_QUIET
            )
            if(_brew_result EQUAL 0)
                _libevent_prepend_brew_prefix()
                _libevent_try_find_package()
                _libevent_try_pkg_config()
            else()
                message(WARNING "brew install libevent failed (exit ${_brew_result})")
            endif()
        else()
            message(WARNING "Homebrew not found — cannot install libevent automatically")
        endif()
        unset(_brew CACHE)

    elseif(UNIX)
        # ── Linux: try common package managers in priority order ─────────────
        foreach(_mgr IN ITEMS apt-get dnf yum pacman zypper)
            find_program(_mgr_exe "${_mgr}")
            if(_mgr_exe)
                if(_mgr STREQUAL "apt-get")
                    set(_mgr_args install -y libevent-dev)
                elseif(_mgr STREQUAL "dnf")
                    set(_mgr_args install -y libevent-devel)
                elseif(_mgr STREQUAL "yum")
                    set(_mgr_args install -y libevent-devel)
                elseif(_mgr STREQUAL "pacman")
                    set(_mgr_args -S --noconfirm libevent)
                elseif(_mgr STREQUAL "zypper")
                    set(_mgr_args install -y libevent-devel)
                endif()

                message(STATUS "Installing libevent via ${_mgr}...")
                execute_process(
                    COMMAND sudo "${_mgr_exe}" ${_mgr_args}
                    RESULT_VARIABLE _pkg_result
                    OUTPUT_QUIET
                )
                unset(_mgr_exe CACHE)

                if(_pkg_result EQUAL 0)
                    _libevent_try_find_package()
                    _libevent_try_pkg_config()
                    break()
                else()
                    message(WARNING "${_mgr} failed (exit ${_pkg_result})")
                endif()
            else()
                unset(_mgr_exe CACHE)
            endif()
        endforeach()

    elseif(WIN32)
        # ── Windows: build from source via FetchContent ──────────────────────
        # libevent has no official prebuilt Windows binaries; build is the
        # cleanest CMake-native fallback.
        include(FetchContent)
        message(STATUS "Building libevent from source (Windows fallback)...")

        set(EVENT__DISABLE_TESTS     ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_SAMPLES   ON CACHE BOOL "" FORCE)
        set(EVENT__DISABLE_OPENSSL   ON CACHE BOOL "" FORCE)

        FetchContent_Declare(libevent_src
            GIT_REPOSITORY https://github.com/libevent/libevent.git
            GIT_TAG        release-2.1.12-stable
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(libevent_src)

        if(TARGET event_core)
            list(APPEND _libevent_link_targets event_core)
            set(_libevent_found TRUE)
        endif()
    endif()
endif()

# ── Hard failure with manual-install instructions ─────────────────────────────
if(NOT _libevent_found)
    message(FATAL_ERROR
        "libevent not found and automatic acquisition failed.\n"
        "Install it manually:\n"
        "  macOS:    brew install libevent\n"
        "  Debian:   sudo apt-get install libevent-dev\n"
        "  Fedora:   sudo dnf install libevent-devel\n"
        "  Arch:     sudo pacman -S libevent\n"
        "  openSUSE: sudo zypper install libevent-devel\n"
        "  Windows:  https://github.com/libevent/libevent/releases\n"
    )
endif()

# ── Normalize IMPORTED_LOCATION across all CMake build configurations ─────────
# FindLibevent targets may only have IMPORTED_LOCATION_RELEASE populated.
# pkg-config IMPORTED_TARGET and FetchContent targets don't need this fix.
foreach(_tgt IN LISTS _libevent_link_targets)
    if(TARGET "${_tgt}")
        get_target_property(_loc "${_tgt}" IMPORTED_LOCATION_RELEASE)
        if(_loc)
            set_target_properties("${_tgt}" PROPERTIES
                    IMPORTED_LOCATION                "${_loc}"
                    IMPORTED_LOCATION_DEBUG          "${_loc}"
                    IMPORTED_LOCATION_RELWITHDEBINFO "${_loc}"
                    IMPORTED_LOCATION_MINSIZEREL     "${_loc}"
            )
        endif()
        unset(_loc)
    endif()
endforeach()
unset(_tgt)

target_link_libraries(postgresql-cpp-driver PRIVATE ${_libevent_link_targets})
unset(_libevent_link_targets)
unset(_libevent_found)