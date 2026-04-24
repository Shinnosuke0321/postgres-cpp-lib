cmake_minimum_required(VERSION 3.26)
project(postgresql-cpp-driver VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build PostgresLib as a shared library" OFF)

set(SOURCES
        src/client_pub.cpp
        src/transaction.cpp
        src/pg_transport.cpp
)

if (BUILD_SHARED_LIBS)
    add_library(postgresql-cpp-driver SHARED ${SOURCES})
    # On Windows, export all symbols automatically for shared builds
    if (WIN32)
        set_target_properties(postgresql-cpp-driver PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif ()
else ()
    add_library(postgresql-cpp-driver STATIC ${SOURCES})
endif ()

target_include_directories(postgresql-cpp-driver
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

add_library(PostgresSQLCppDriver::PostgresSQLCppDriver ALIAS postgresql-cpp-driver)
target_compile_features(postgresql-cpp-driver PUBLIC cxx_std_23)

# Platform-specific settings
if (WIN32)
    target_compile_definitions(postgresql-cpp-driver PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(postgresql-cpp-driver PRIVATE ws2_32)
endif ()

if (MSVC)
    target_compile_options(postgresql-cpp-driver PRIVATE /W4 /utf-8)
else ()
    target_compile_options(postgresql-cpp-driver PRIVATE -Wall -Wextra)
endif ()


include(CTest)
if (PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    add_subdirectory(tests)
endif ()

include(cmake/libpq.cmake)
include(cmake/libevent.cmake)
add_subdirectory(dependencies/core)
target_link_libraries(postgresql-cpp-driver PUBLIC
        DbConnectionPool::DbConnectionPool
)


include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS postgresql-cpp-driver DbConnectionPool
        EXPORT PostgresDriverLibTarget
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT PostgresDriverLibTarget
        FILE PostgresDriverLibTarget.cmake
        NAMESPACE PostgreSqlDriverCpp::
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