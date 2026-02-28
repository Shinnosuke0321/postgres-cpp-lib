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

# Set the desired sanitizer type
set(SANITIZER_TYPE "address") # or "address"

# Add compile and link options
list(APPEND CMAKE_C_FLAGS "-fsanitize=${SANITIZER_TYPE}")
list(APPEND CMAKE_CXX_FLAGS "-fsanitize=${SANITIZER_TYPE}")
list(APPEND CMAKE_EXE_LINKER_FLAGS "-fsanitize=${SANITIZER_TYPE}")

if(SANITIZER_TYPE STREQUAL "thread")
    set(CTEST_MEMORYCHECK_TYPE "ThreadSanitizer")
elseif(SANITIZER_TYPE STREQUAL "address")
    set(CTEST_MEMORYCHECK_TYPE "AddressSanitizer")
endif()

add_executable(DbConnectionPool_tests db_pool_test.cpp)
add_executable(IntrusivePtr_tests intrusive_ptr_test.cpp)

target_link_libraries(DbConnectionPool_tests PRIVATE
        DbConnectionPool::DbConnectionPool
        GTest::gtest_main
)
target_link_libraries(IntrusivePtr_tests PRIVATE
        DbConnectionPool::DbConnectionPool
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(DbConnectionPool_tests)
gtest_discover_tests(IntrusivePtr_tests)