cmake_minimum_required(VERSION 3.14)
project(drogon_pg_driver C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 查找 PostgreSQL libpq（允许父项目通过缓存变量传入，避免重复查找）
if(NOT PostgreSQL_LIBRARIES)
    find_package(PostgreSQL REQUIRED)
else()
    message(STATUS "[drogon_pg_driver] 复用父项目 libpq: ${PostgreSQL_LIBRARIES}")
endif()

# 源文件
set(DRIVER_SOURCES
    src/pg_config.c
    src/pg_error.c
    src/pg_type_codec.c
    src/pg_connection.c
    src/pg_pool.c
    src/pg_result.c
    src/pg_fallback_pool.c
    src/pg_circuit_breaker.c
    src/pg_client.c
    src/pg_transaction.c
    src/pg_health_monitor.c
    src/pg_async_write_queue.c
    src/pg_async_connection.c
    src/pg_async_client.c
    src/pg_pool_wait_queue.c
    src/pg_pool_monitor.c
    src/pg_sql_parser.c
    src/pg_reconnect_backoff.c
    src/pg_mutex.c
    src/pg_logger.c
    src/pg_writer_connection.c
)

# 头文件
set(DRIVER_HEADERS
    include/pg_config.h
    include/pg_error.h
    include/pg_type_codec.h
    include/pg_connection.h
    include/pg_pool.h
    include/pg_result.h
    include/pg_fallback_pool.h
    include/pg_circuit_breaker.h
    include/pg_client.h
    include/pg_transaction.h
    include/pg_health_monitor.h
    include/pg_async_write_queue.h
    include/pg_async_connection.h
    include/pg_async_client.h
    include/pg_pool_wait_queue.h
    include/pg_pool_monitor.h
    include/pg_sql_parser.h
    include/pg_reconnect_backoff.h
    include/pg_mutex.h
    include/pg_logger.h
    include/pg_writer_connection.h
)

# 创建静态库
add_library(drogon_pg_driver STATIC ${DRIVER_SOURCES})

# 设置头文件路径
target_include_directories(drogon_pg_driver PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PostgreSQL_INCLUDE_DIRS}
)

# 链接 PostgreSQL libpq
target_link_libraries(drogon_pg_driver PUBLIC ${PostgreSQL_LIBRARIES})

# 编译选项
if(MSVC)
    target_compile_options(drogon_pg_driver PRIVATE /W4)
else()
    target_compile_options(drogon_pg_driver PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 线程库（静态库不 link，但导出依赖供父项目使用）
if(WIN32)
    target_link_libraries(drogon_pg_driver PUBLIC ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(drogon_pg_driver PUBLIC Threads::Threads)
endif()

# 测试
enable_testing()

# 测试可执行文件
add_executable(test_config tests/test_config.c)
target_link_libraries(test_config drogon_pg_driver)
add_test(NAME test_config COMMAND test_config)

add_executable(test_error tests/test_error.c)
target_link_libraries(test_error drogon_pg_driver)
add_test(NAME test_error COMMAND test_error)

add_executable(test_error_simple tests/test_error_simple.c)
target_link_libraries(test_error_simple drogon_pg_driver)
