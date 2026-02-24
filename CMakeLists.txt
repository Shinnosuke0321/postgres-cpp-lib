cmake_minimum_required(VERSION 3.20)
project(PostgresLib LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(PostgresLib STATIC
        src/postgres_priv.cpp
        src/postgres_pub.cpp
)

target_include_directories(PostgresLib
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(PostgresLib::PostgresLib ALIAS PostgresLib)

target_compile_features(PostgresLib PUBLIC cxx_std_23)

find_package(PostgreSQL REQUIRED)
if (NOT PostgreSQL_FOUND)

endif ()

target_link_libraries(PostgresLib PUBLIC
        PostgreSQL::PostgreSQL
)

add_subdirectory(vendor/core)
target_link_libraries(PostgresLib PUBLIC
        DbConnectionPool::DbConnectionPool
)

