include(FetchContent)
find_package(GTest QUIET)
if (NOT GTest_FOUND)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
    )
    FetchContent_MakeAvailable(googletest)
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sanitizer option
set(SANITIZER_TYPE "address" CACHE STRING "Sanitizer type (address, thread, undefined, none)")

add_executable(DbConnectionPool_tests db_pool_test.cpp)
add_executable(IntrusivePtr_tests intrusive_ptr_test.cpp)
add_executable(ConnectionError_tests connection_error_test.cpp)
add_executable(ConnectionFactory_tests connection_factory_test.cpp)
add_executable(ConnectionManager_tests connection_manager_test.cpp)
add_executable(ConnectionPoolAdvanced_tests connection_pool_advanced_test.cpp)

target_link_libraries(DbConnectionPool_tests PRIVATE
        DbConnectionPool::DbConnectionPool
        GTest::gtest_main
)
target_link_libraries(IntrusivePtr_tests PRIVATE
        DbConnectionPool::DbConnectionPool
        GTest::gtest_main
)
target_link_libraries(ConnectionError_tests PRIVATE
        DbConnectionPool::DbConnectionPool
        GTest::gtest_main
)
target_link_libraries(ConnectionFactory_tests PRIVATE
        DbConnectionPool::DbConnectionPool
        GTest::gtest_main
)
target_link_libraries(ConnectionManager_tests PRIVATE
        DbConnectionPool::DbConnectionPool
        GTest::gtest_main
)
target_link_libraries(ConnectionPoolAdvanced_tests PRIVATE
        DbConnectionPool::DbConnectionPool
        GTest::gtest_main
)

# Apply sanitizer only for supported compilers (Homebrew GCC on macOS does not ship ASan)
if (NOT SANITIZER_TYPE STREQUAL "none")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT APPLE)

        foreach(target
                DbConnectionPool_tests
                IntrusivePtr_tests
                ConnectionError_tests
                ConnectionFactory_tests
                ConnectionManager_tests
                ConnectionPoolAdvanced_tests
        )
            target_compile_options(${target} PRIVATE
                    -fsanitize=${SANITIZER_TYPE}
                    -fno-omit-frame-pointer
            )

            target_link_options(${target} PRIVATE
                    -fsanitize=${SANITIZER_TYPE}
            )
        endforeach()
    endif()
endif()

include(GoogleTest)
gtest_discover_tests(DbConnectionPool_tests)
gtest_discover_tests(IntrusivePtr_tests)
gtest_discover_tests(ConnectionError_tests)
gtest_discover_tests(ConnectionFactory_tests)
gtest_discover_tests(ConnectionManager_tests)
gtest_discover_tests(ConnectionPoolAdvanced_tests)