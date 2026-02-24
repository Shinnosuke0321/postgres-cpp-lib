add_executable(DbConnectionPool_tests
        test.cpp
)

target_link_libraries(DbConnectionPool_tests
        PRIVATE
        DbConnectionPool::DbConnectionPool
)

add_test(NAME DbConnectionPool.smoke COMMAND DbConnectionPool_tests)