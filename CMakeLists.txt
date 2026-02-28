cmake_minimum_required(VERSION 3.26)
project(DbConnectionPool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(DbConnectionPool INTERFACE)

target_include_directories(DbConnectionPool INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

add_library(DbConnectionPool::DbConnectionPool ALIAS DbConnectionPool)

target_compile_features(DbConnectionPool
        INTERFACE cxx_std_23
)

include(CTest)
if (PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    add_subdirectory(test)
endif ()


include(GNUInstallDirs)

install(TARGETS DbConnectionPool
        EXPORT DbConnectionPoolTargets
)

install(EXPORT DbConnectionPoolTargets
        NAMESPACE DbConnectionPool::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DbConnectionPool
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)