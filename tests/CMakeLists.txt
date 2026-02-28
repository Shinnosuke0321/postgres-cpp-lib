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
target_link_libraries(PostgresSQL_tests PRIVATE
        PostgresLib::PostgresLib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(PostgresSQL_tests)