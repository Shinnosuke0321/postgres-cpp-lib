cmake_minimum_required(VERSION 3.26)
project(PostgresLib VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build PostgresLib as a shared library" OFF)

# Use pg_config to pre-populate hints so FindPostgreSQL resolves the actual
# library path on all platforms (especially Homebrew keg-only installs on macOS).
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

find_package(PostgreSQL REQUIRED)

# FindPostgreSQL may only set IMPORTED_LOCATION_RELEASE, leaving other
# configurations without a location. Populate all variants from the variable.
if(TARGET PostgreSQL::PostgreSQL AND PostgreSQL_LIBRARY)
    set_target_properties(PostgreSQL::PostgreSQL PROPERTIES
        IMPORTED_LOCATION                "${PostgreSQL_LIBRARY}"
        IMPORTED_LOCATION_DEBUG          "${PostgreSQL_LIBRARY}"
        IMPORTED_LOCATION_RELEASE        "${PostgreSQL_LIBRARY}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${PostgreSQL_LIBRARY}"
        IMPORTED_LOCATION_MINSIZEREL     "${PostgreSQL_LIBRARY}"
    )
endif()

add_subdirectory(dependencies/core)

add_library(PostgresLib
        src/postgres_priv.cpp
        src/postgres_pub.cpp
        src/transaction.cpp
)

add_library(PostgresLib::PostgresLib ALIAS PostgresLib)

target_compile_features(PostgresLib PUBLIC cxx_std_23)

target_link_libraries(PostgresLib PUBLIC
        PostgreSQL::PostgreSQL
        DbConnectionPool::DbConnectionPool
        $<$<PLATFORM_ID:Windows>:ws2_32>
)

target_include_directories(PostgresLib
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

include(CTest)
if (PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    add_subdirectory(tests)
endif ()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS PostgresLib DbConnectionPool
        EXPORT PostgresLibTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT PostgresLibTargets
        FILE PostgresLibTargets.cmake
        NAMESPACE PostgresLib::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PostgresLib
)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/PostgresLibConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PostgresLibConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/PostgresLibConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PostgresLib
)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/PostgresLibConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/PostgresLibConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PostgresLib
)