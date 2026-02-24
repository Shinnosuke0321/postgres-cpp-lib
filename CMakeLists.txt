cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(DbConnectionPool
        VERSION 1.0.0
        LANGUAGES CXX
)

add_library(DbConnectionPool INTERFACE)

target_include_directories(DbConnectionPool INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/lib/include")

add_library(DbConnectionPool::DbConnectionPool ALIAS DbConnectionPool)

target_compile_features(DbConnectionPool
        INTERFACE cxx_std_23
)