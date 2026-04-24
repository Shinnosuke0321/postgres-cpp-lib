include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.16.0
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

if (NOT TEST_SOURCES)
    message(STATUS "No test sources found in tests/ — skipping test targets")
    return()
endif ()

include(GoogleTest)

foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    add_executable(${TEST_NAME} ${TEST_SOURCE})

    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    target_link_libraries(${TEST_NAME} PRIVATE
            PostgresSQLCppDriver::PostgresSQLCppDriver
            GTest::gtest_main
            GTest::gmock
    )

    gtest_discover_tests(${TEST_NAME})
endforeach()