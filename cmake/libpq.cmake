if(NOT PostgreSQL_FOUND)
    find_package(PostgreSQL REQUIRED)
endif()

target_link_libraries(postgresql-cpp-driver PUBLIC PostgreSQL::PostgreSQL)