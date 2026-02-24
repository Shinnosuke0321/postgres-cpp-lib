cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(DbConnectionPool
        VERSION 1.0.0
        LANGUAGES CXX
)

add_library(DbConnectionPool INTERFACE)

include(FetchContent)
FetchContent_Declare(
        trantor
        GIT_REPOSITORY https://github.com/an-tao/trantor.git
        GIT_TAG v1.5.26
)
FetchContent_MakeAvailable(trantor)
target_link_libraries(DbConnectionPool INTERFACE trantor)

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