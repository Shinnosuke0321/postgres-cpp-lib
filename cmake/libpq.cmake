# cmake/libpq.cmake
# Finds libpq via pg_config / FindPostgreSQL.
# Falls back to platform-specific acquisition when not found:
#   macOS   — Homebrew (brew install postgresql@16)
#   Linux   — system package manager (apt-get / dnf / yum / pacman / zypper)
#   Windows — EDB prebuilt binary zip (downloaded into the build tree)

# Populate FindPostgreSQL hints from pg_config, then clear cached NOTFOUND so
# repeated calls can re-search after a package install.
macro(_libpq_hints_from_pg_config)
    unset(PostgreSQL_LIBRARY CACHE)
    find_program(_pg_config pg_config)
    if(_pg_config)
        execute_process(COMMAND "${_pg_config}" --libdir
                OUTPUT_VARIABLE _pg_libdir OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND "${_pg_config}" --includedir
                OUTPUT_VARIABLE _pg_incdir OUTPUT_STRIP_TRAILING_WHITESPACE)
        find_library(PostgreSQL_LIBRARY NAMES pq HINTS "${_pg_libdir}" NO_DEFAULT_PATH)
        set(PostgreSQL_INCLUDE_DIR "${_pg_incdir}" CACHE PATH "PostgreSQL include directory")
        unset(_pg_libdir)
        unset(_pg_incdir)
    endif()
    unset(_pg_config CACHE)
endmacro()

# Initial search
_libpq_hints_from_pg_config()
find_package(PostgreSQL QUIET)

# Platform-specific fallback
if(NOT PostgreSQL_FOUND)
    message(STATUS "libpq not found — attempting platform-specific acquisition")

    if(APPLE)
        # macOS: Homebrew
        find_program(_brew brew)
        if(_brew)
            message(STATUS "Installing postgresql@16 via Homebrew...")
            execute_process(
                COMMAND "${_brew}" install postgresql@16
                RESULT_VARIABLE _brew_result
                OUTPUT_QUIET ERROR_QUIET
            )
            if(_brew_result EQUAL 0)
                _libpq_hints_from_pg_config()
                find_package(PostgreSQL QUIET)
            else()
                message(WARNING "brew install postgresql@16 failed (exit ${_brew_result})")
            endif()
        else()
            message(WARNING "Homebrew not found — cannot install libpq automatically")
        endif()
        unset(_brew CACHE)

    elseif(UNIX)
        # Linux: try common package managers in priority order
        foreach(_mgr IN ITEMS apt-get dnf yum pacman zypper)
            find_program(_mgr_exe "${_mgr}")
            if(_mgr_exe)
                if(_mgr STREQUAL "apt-get")
                    set(_mgr_args install -y libpq-dev)
                elseif(_mgr STREQUAL "dnf")
                    set(_mgr_args install -y libpq-devel)
                elseif(_mgr STREQUAL "yum")
                    set(_mgr_args install -y postgresql-devel)
                elseif(_mgr STREQUAL "pacman")
                    set(_mgr_args -S --noconfirm postgresql-libs)
                elseif(_mgr STREQUAL "zypper")
                    set(_mgr_args install -y libpq5-devel)
                endif()

                message(STATUS "Installing libpq via ${_mgr}...")
                execute_process(
                    COMMAND sudo "${_mgr_exe}" ${_mgr_args}
                    RESULT_VARIABLE _pkg_result
                    OUTPUT_QUIET
                )
                unset(_mgr_exe CACHE)

                if(_pkg_result EQUAL 0)
                    _libpq_hints_from_pg_config()
                    find_package(PostgreSQL QUIET)
                    break()
                else()
                    message(WARNING "${_mgr} failed (exit ${_pkg_result})")
                endif()
            else()
                unset(_mgr_exe CACHE)
            endif()
        endforeach()

    elseif(WIN32)
        # ── Windows: download EDB prebuilt binary zip ────────────────────────
        set(_pg_version "16.8-1")
        set(_pg_dir     "${CMAKE_BINARY_DIR}/_deps/postgresql")
        set(_pg_zip     "${_pg_dir}/postgresql.zip")
        set(_pg_root    "${_pg_dir}/pgsql")

        if(NOT EXISTS "${_pg_root}/lib/libpq.lib")
            message(STATUS "Downloading PostgreSQL ${_pg_version} prebuilt binaries (Windows x64)...")
            file(MAKE_DIRECTORY "${_pg_dir}")
            file(DOWNLOAD
                "https://get.enterprisedb.com/postgresql/postgresql-${_pg_version}-windows-x64-binaries.zip"
                "${_pg_zip}"
                SHOW_PROGRESS
                STATUS _dl_status
                TLS_VERIFY ON
            )
            list(GET _dl_status 0 _dl_code)
            if(_dl_code EQUAL 0)
                message(STATUS "Extracting PostgreSQL...")
                execute_process(
                    COMMAND "${CMAKE_COMMAND}" -E tar xf "${_pg_zip}"
                    WORKING_DIRECTORY "${_pg_dir}"
                    RESULT_VARIABLE _extract_result
                )
                if(NOT _extract_result EQUAL 0)
                    message(WARNING "Failed to extract PostgreSQL archive")
                endif()
            else()
                list(GET _dl_status 1 _dl_err)
                message(WARNING "Failed to download PostgreSQL: ${_dl_err}")
            endif()
        endif()

        if(EXISTS "${_pg_root}/lib/libpq.lib")
            set(PostgreSQL_LIBRARY     "${_pg_root}/lib/libpq.lib" CACHE FILEPATH "PostgreSQL library")
            set(PostgreSQL_INCLUDE_DIR "${_pg_root}/include"       CACHE PATH     "PostgreSQL include directory")
            find_package(PostgreSQL QUIET)

            # Copy the runtime DLL so executables can find it
            if(PostgreSQL_FOUND AND EXISTS "${_pg_root}/bin/libpq.dll")
                if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
                    set(_runtime_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
                else()
                    set(_runtime_dir "${CMAKE_BINARY_DIR}")
                endif()
                file(COPY "${_pg_root}/bin/libpq.dll" DESTINATION "${_runtime_dir}")
            endif()
        endif()
    endif()
endif()

# ── Hard failure with manual-install instructions ─────────────────────────────
if(NOT PostgreSQL_FOUND)
    message(FATAL_ERROR
        "libpq (PostgreSQL client library) not found and automatic acquisition failed.\n"
        "Install it manually:\n"
        "  macOS:    brew install postgresql@16\n"
        "  Debian:   sudo apt-get install libpq-dev\n"
        "  Fedora:   sudo dnf install libpq-devel\n"
        "  Arch:     sudo pacman -S postgresql-libs\n"
        "  openSUSE: sudo zypper install libpq5-devel\n"
        "  Windows:  https://www.postgresql.org/download/windows/\n"
    )
endif()

# ── Normalize IMPORTED_LOCATION across all CMake build configurations ─────────
# FindPostgreSQL may only set IMPORTED_LOCATION_RELEASE, leaving Debug/RelWithDebInfo
# builds without a valid location on some platforms.
if(TARGET PostgreSQL::PostgreSQL AND PostgreSQL_LIBRARY)
    set_target_properties(PostgreSQL::PostgreSQL PROPERTIES
            IMPORTED_LOCATION                "${PostgreSQL_LIBRARY}"
            IMPORTED_LOCATION_DEBUG          "${PostgreSQL_LIBRARY}"
            IMPORTED_LOCATION_RELEASE        "${PostgreSQL_LIBRARY}"
            IMPORTED_LOCATION_RELWITHDEBINFO "${PostgreSQL_LIBRARY}"
            IMPORTED_LOCATION_MINSIZEREL     "${PostgreSQL_LIBRARY}"
    )
endif()

target_link_libraries(postgresql-cpp-driver PUBLIC PostgreSQL::PostgreSQL)