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
add_executable(PostgresSQL_tests postgres_test.cpp)
add_executable(Migration_tests migration_test.cpp)
add_executable(PostgresError_tests postgres_error_test.cpp)

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

include(GoogleTest)
gtest_discover_tests(PostgresSQL_tests)
gtest_discover_tests(Migration_tests)
gtest_discover_tests(PostgresError_tests)