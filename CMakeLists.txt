cmake_minimum_required(VERSION 3.26)
project(PostgresLib VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build PostgresLib as a shared library" OFF)

find_package(PostgreSQL REQUIRED)
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