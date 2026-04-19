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
set(test_headers
        gtest/postgres_lib_test.h
        gtest/test_values.h
        gtest/test_queries.h
)
add_executable(SqlParser_tests sql_parser_test.cpp)
add_executable(PostgresSQL_tests ${test_headers} postgres_query_test.cpp)
add_executable(Migration_tests ${test_headers} migration_test.cpp)
add_executable(PostgresError_tests postgres_error_test.cpp)
add_executable(PostgresTransaction_tests ${test_headers} postgres_transaction_test.cpp)
add_executable(PostgresCbPool_tests ${test_headers} postgres_cb_pool_test.cpp)

target_link_libraries(SqlParser_tests PRIVATE
        PostgresLib::PostgresLib
        GTest::gtest_main
)

target_link_libraries(PostgresSQL_tests PRIVATE
        PostgresLib::PostgresLib
        GTest::gtest_main
)

configure_file(docker/init_test_users.sql ${CMAKE_CURRENT_BINARY_DIR}/init_test_users.sql COPYONLY)

target_compile_definitions(Migration_tests PRIVATE
        TEST_MIGRATIONS_SQL_PATH="${CMAKE_CURRENT_BINARY_DIR}/init_test_users.sql"
)

target_link_libraries(Migration_tests PRIVATE
        PostgresLib::PostgresLib
        GTest::gtest_main
)

target_link_libraries(PostgresError_tests PRIVATE
        PostgresLib::PostgresLib
        GTest::gtest_main
)

target_link_libraries(PostgresTransaction_tests PRIVATE
        PostgresLib::PostgresLib
        GTest::gtest_main
)

target_link_libraries(PostgresCbPool_tests PRIVATE
        PostgresLib::PostgresLib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(SqlParser_tests)
gtest_discover_tests(Migration_tests)
gtest_discover_tests(PostgresSQL_tests)
gtest_discover_tests(PostgresError_tests)
gtest_discover_tests(PostgresTransaction_tests)
gtest_discover_tests(PostgresCbPool_tests)