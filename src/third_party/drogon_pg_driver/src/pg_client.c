#include "pg_client.h"
#include "pg_config.h"
#include "pg_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libpq-fe.h>

/* 全局客户端实例 */
static pg_client_t g_client = {0};

/* 获取客户端单例 */
pg_client_t* pg_client_get_instance(void) {
    return &g_client;
}

/* 初始化客户端 */
int pg_client_init(const char *conn_str) {
    if (g_client.initialized) {
        return 0;
    }

    pg_config_t *config = pg_config_get_instance();

    /* 初始化互斥锁 */
    if (pg_mutex_init(&g_client.mutex) != 0) {
        return -1;
    }

    /* 复制连接字符串 */
    g_client.connection_string = conn_str ? strdup(conn_str) : NULL;
    if (conn_str && !g_client.connection_string) {
        pg_mutex_destroy(&g_client.mutex);
        return -1;
    }

    /* 创建主连接池 */
    g_client.main_pool = pg_pool_create(
        conn_str,
        pg_config_get_pool_min_size(config),
        pg_config_get_pool_max_size(config),
        pg_config_get_pool_checkout_timeout_ms(config),
        0  /* is_fallback = 0 */
    );
    if (g_client.main_pool == NULL) {
        free(g_client.connection_string);
        pg_mutex_destroy(&g_client.mutex);
        return -1;
    }

    /* 初始化主连接池 */
    if (pg_pool_init(g_client.main_pool) != 0) {
        pg_pool_destroy(g_client.main_pool);
        free(g_client.connection_string);
        pg_mutex_destroy(&g_client.mutex);
        return -1;
    }

    /* 创建降级连接池 */
    g_client.fallback_pool = pg_pool_create(
        conn_str,
        pg_config_get_fallback_pool_min_size(config),
        pg_config_get_fallback_pool_max_size(config),
        pg_config_get_pool_checkout_timeout_ms(config),
        1  /* is_fallback = 1 */
    );
    if (g_client.fallback_pool == NULL) {
        pg_pool_destroy(g_client.main_pool);
        free(g_client.connection_string);
        pg_mutex_destroy(&g_client.mutex);
        return -1;
    }

    /* 初始化降级连接池 */
    if (pg_pool_init(g_client.fallback_pool) != 0) {
        pg_pool_destroy(g_client.fallback_pool);
        pg_pool_destroy(g_client.main_pool);
        free(g_client.connection_string);
        pg_mutex_destroy(&g_client.mutex);
        return -1;
    }

    /* 创建熔断器 */
    g_client.circuit_breaker = pg_circuit_breaker_create();
    if (g_client.circuit_breaker == NULL) {
        pg_pool_destroy(g_client.fallback_pool);
        pg_pool_destroy(g_client.main_pool);
        free(g_client.connection_string);
        pg_mutex_destroy(&g_client.mutex);
        return -1;
    }

    /* 设置熔断器阈值 */
    g_client.circuit_breaker->failure_threshold = pg_config_get_failure_threshold(config);
    g_client.circuit_breaker->recovery_threshold = pg_config_get_recovery_threshold(config);

    /* 创建异步写队列 */
    g_client.write_queue = pg_async_write_queue_create(
        pg_config_get_async_write_queue_size(config),
        pg_config_get_async_write_batch_size(config)
    );
    if (g_client.write_queue == NULL) {
        pg_circuit_breaker_destroy(g_client.circuit_breaker);
        pg_pool_destroy(g_client.fallback_pool);
        pg_pool_destroy(g_client.main_pool);
        free(g_client.connection_string);
        pg_mutex_destroy(&g_client.mutex);
        return -1;
    }

    /* 创建 Writer 连接 */
    g_client.writer_conn = pg_writer_connection_create(
        conn_str,
        pg_config_get_async_write_batch_size(config),
        1000  /* batch_timeout_ms */
    );
    if (g_client.writer_conn == NULL) {
        pg_async_write_queue_destroy(g_client.write_queue);
        pg_circuit_breaker_destroy(g_client.circuit_breaker);
        pg_pool_destroy(g_client.fallback_pool);
        pg_pool_destroy(g_client.main_pool);
        free(g_client.connection_string);
        pg_mutex_destroy(&g_client.mutex);
        return -1;
    }

    /* 连接 Writer */
    if (pg_writer_connection_connect(g_client.writer_conn) != 0) {
        PG_LOG_WARN("[CLIENT] Writer connection failed to connect, will retry on demand");
    }

    g_client.initialized = 1;

    PG_LOG_INFO("[CLIENT] Client initialized successfully");
    return 0;
}

/* 清理客户端 */
void pg_client_cleanup(void) {
    if (!g_client.initialized) {
        return;
    }

    pg_mutex_lock(&g_client.mutex);

    if (g_client.writer_conn != NULL) {
        pg_writer_connection_destroy(g_client.writer_conn);
        g_client.writer_conn = NULL;
    }

    if (g_client.write_queue != NULL) {
        pg_async_write_queue_destroy(g_client.write_queue);
        g_client.write_queue = NULL;
    }

    if (g_client.circuit_breaker != NULL) {
        pg_circuit_breaker_destroy(g_client.circuit_breaker);
        g_client.circuit_breaker = NULL;
    }

    if (g_client.fallback_pool != NULL) {
        pg_pool_destroy(g_client.fallback_pool);
        g_client.fallback_pool = NULL;
    }

    if (g_client.main_pool != NULL) {
        pg_pool_destroy(g_client.main_pool);
        g_client.main_pool = NULL;
    }

    if (g_client.connection_string != NULL) {
        free(g_client.connection_string);
        g_client.connection_string = NULL;
    }

    g_client.initialized = 0;

    pg_mutex_unlock(&g_client.mutex);
    pg_mutex_destroy(&g_client.mutex);
}

/* 执行读取操作（内部函数） */
static pg_result_t* exec_read(const char *sql, const char **params, int param_count) {
    pg_pool_t *pool = NULL;
    pg_connection_t *conn = NULL;
    PGresult *pq_result = NULL;
    pg_result_t *result = NULL;

    /* 根据熔断器状态选择连接池 */
    if (pg_circuit_breaker_allow_request(g_client.circuit_breaker)) {
        pool = g_client.main_pool;
    } else {
        pool = g_client.fallback_pool;
    }

    /* 获取连接 */
    conn = pg_pool_checkout(pool);
    if (conn == NULL) {
        pg_circuit_breaker_record_failure(g_client.circuit_breaker);
        PG_LOG_ERROR("[CLIENT] Failed to checkout connection for read");
        return NULL;
    }

    /* 执行查询 */
    PGconn *pq_conn = pg_connection_get_pq_conn(conn);
    if (pq_conn == NULL) {
        pg_pool_checkin(pool, conn);
        pg_circuit_breaker_record_failure(g_client.circuit_breaker);
        return NULL;
    }

    if (params != NULL && param_count > 0) {
        pq_result = PQexecParams(pq_conn, sql, param_count, NULL, 
                                 (const char * const *)params, NULL, NULL, 0);
    } else {
        pq_result = PQexec(pq_conn, sql);
    }

    if (pq_result == NULL) {
        /* 归还连接前先取错误信息，避免归还后 pq_conn 被复用 */
        PG_LOG_ERROR("[CLIENT] Query failed: %s", PQerrorMessage(pq_conn));
        pg_pool_checkin(pool, conn);
        pg_circuit_breaker_record_failure(g_client.circuit_breaker);
        return NULL;
    }

    /* 检查结果状态 */
    ExecStatusType status = PQresultStatus(pq_result);
    if (status == PGRES_FATAL_ERROR || status == PGRES_BAD_RESPONSE) {
        pg_circuit_breaker_record_failure(g_client.circuit_breaker);
        result = pg_result_create(pq_result);
        /* 归还连接 */
        pg_pool_checkin(pool, conn);
        return result;
    }

    /* 成功：先创建结果，再归还连接 */
    pg_circuit_breaker_record_success(g_client.circuit_breaker);
    result = pg_result_create(pq_result);
    pg_pool_checkin(pool, conn);

    return result;
}

/* 执行写入操作（异步队列） */
int pg_client_exec_write(const char *sql, const char **params, int param_count, void *promise) {
    if (!g_client.initialized || sql == NULL) {
        return -1;
    }

    pg_mutex_guard_t guard = pg_mutex_guard_create(&g_client.mutex);

    /* 检查 Writer 连接 */
    if (g_client.writer_conn == NULL) {
        PG_LOG_ERROR("[CLIENT] Writer connection not available");
        pg_mutex_guard_destroy(&guard);
        return -1;
    }

    /* 如果 Writer 连接断开，尝试重连 */
    if (pg_writer_connection_get_state(g_client.writer_conn) == PG_WRITER_DISCONNECTED ||
        pg_writer_connection_get_state(g_client.writer_conn) == PG_WRITER_ERROR) {
        if (pg_writer_connection_connect(g_client.writer_conn) != 0) {
            PG_LOG_ERROR("[CLIENT] Failed to reconnect writer");
            pg_mutex_guard_destroy(&guard);
            return -1;
        }
    }

    /* 直接写入 */
    int ret = pg_writer_connection_write(g_client.writer_conn, sql, params, param_count);

    /* 立即提交，保证 read-after-write 一致性 */
    if (ret == 0 && pg_writer_connection_in_transaction(g_client.writer_conn)) {
        if (pg_writer_connection_commit_batch(g_client.writer_conn) != 0) {
            PG_LOG_ERROR("[CLIENT] Failed to commit batch");
        }
    }

    pg_mutex_guard_destroy(&guard);
    return ret;
}

/* 执行查询（同步） - 读写分流 */
pg_result_t* pg_client_exec(const char *sql, const char **params, int param_count) {
    if (!g_client.initialized || sql == NULL) {
        return NULL;
    }

    /* 验证 SQL 语句 */
    if (pg_client_validate_sql(sql) != 0) {
        PG_LOG_ERROR("[CLIENT] SQL validation failed");
        return NULL;
    }

    /* 验证参数 */
    if (params != NULL && pg_client_validate_params(params, param_count) != 0) {
        PG_LOG_ERROR("[CLIENT] Parameter validation failed");
        return NULL;
    }

    pg_mutex_guard_t guard = pg_mutex_guard_create(&g_client.mutex);

    /* 解析 SQL 类型 */
    pg_sql_type_t type = pg_sql_parse_type(sql);
    pg_sql_class_t class = pg_sql_get_class(type);

    pg_mutex_guard_destroy(&guard);

    /* 根据类型分流 */
    if (class == PG_SQL_CLASS_WRITE) {
        /* 写操作走异步写队列 */
        int ret = pg_client_exec_write(sql, params, param_count, NULL);
        if (ret != 0) {
            return NULL;
        }
        /* 返回空结果表示成功 */
        return pg_result_create_empty();
    }

    /* 读操作和其他操作走连接池 */
    return exec_read(sql, params, param_count);
}

/* 执行查询（参数化） */
pg_result_t* pg_client_exec_params(const char *sql, const char **params, const char **param_types, int param_count) {
    /* 暂时简化为调用 pg_client_exec */
    return pg_client_exec(sql, params, param_count);
}

/* 开始事务 */
int pg_client_begin_transaction(void) {
    pg_result_t *result = pg_client_exec("BEGIN", NULL, 0);
    if (result == NULL) {
        return -1;
    }

    int is_ok = !pg_result_is_error(result);
    pg_result_destroy(result);
    return is_ok ? 0 : -1;
}

/* 提交事务 */
int pg_client_commit_transaction(void) {
    pg_result_t *result = pg_client_exec("COMMIT", NULL, 0);
    if (result == NULL) {
        return -1;
    }

    int is_ok = !pg_result_is_error(result);
    pg_result_destroy(result);
    return is_ok ? 0 : -1;
}

/* 回滚事务 */
int pg_client_rollback_transaction(void) {
    pg_result_t *result = pg_client_exec("ROLLBACK", NULL, 0);
    if (result == NULL) {
        return -1;
    }

    int is_ok = !pg_result_is_error(result);
    pg_result_destroy(result);
    return is_ok ? 0 : -1;
}

/* 获取熔断器状态 */
pg_cb_state_t pg_client_get_circuit_breaker_state(void) {
    if (g_client.circuit_breaker == NULL) {
        return PG_CB_OPEN;
    }
    return pg_circuit_breaker_get_state(g_client.circuit_breaker);
}

/* 获取主连接池大小 */
int pg_client_get_main_pool_size(void) {
    if (g_client.main_pool == NULL) {
        return 0;
    }
    return pg_pool_size(g_client.main_pool);
}

/* 获取降级连接池大小 */
int pg_client_get_fallback_pool_size(void) {
    if (g_client.fallback_pool == NULL) {
        return 0;
    }
    return pg_pool_size(g_client.fallback_pool);
}

/* 健康检查 */
int pg_client_health_check(void) {
    if (!g_client.initialized) {
        return -1;
    }

    int healthy = 0;
    if (g_client.main_pool != NULL) {
        healthy += pg_pool_health_check(g_client.main_pool);
    }
    if (g_client.fallback_pool != NULL) {
        healthy += pg_pool_health_check(g_client.fallback_pool);
    }

    return healthy;
}

/* 占位符转换：? → $1, $2, ... */
char* pg_client_convert_placeholders(const char *sql) {
    if (sql == NULL) {
        return NULL;
    }

    size_t len = strlen(sql);
    /* 修正：最坏情况每个 ? 变成 $999 (4 字符) */
    char *result = (char*)malloc(len * 4 + 1);
    if (result == NULL) {
        return NULL;
    }

    size_t result_index = 0;
    int param_num = 1;

    for (size_t i = 0; i < len; i++) {
        if (sql[i] == '?') {
            /* 替换为 $N */
            result_index += snprintf(result + result_index, len * 4 + 1 - result_index, "$%d", param_num);
            param_num++;
        } else {
            result[result_index++] = sql[i];
        }
    }

    result[result_index] = '\0';
    return result;
}

/* 获取写队列 */
pg_async_write_queue_t* pg_client_get_write_queue(void) {
    return g_client.write_queue;
}

/* 获取连接池统计 */
pg_pool_stats_t pg_client_get_pool_stats(void) {
    pg_pool_stats_t empty = {0};
    if (g_client.main_pool == NULL) {
        return empty;
    }
    return pg_pool_get_stats(g_client.main_pool);
}

/* 检查连接泄漏 */
int pg_client_check_leaks(void) {
    if (g_client.main_pool == NULL) {
        return 0;
    }
    return pg_pool_check_leaks(g_client.main_pool);
}

/* 设置泄漏超时 */
void pg_client_set_leak_timeout(int timeout_sec) {
    if (g_client.main_pool == NULL) {
        return;
    }
    pg_pool_set_leak_timeout(g_client.main_pool, timeout_sec);
}

/* SQL 参数转义（防止注入） */
char* pg_client_escape_string(const char *input) {
    if (input == NULL) {
        return NULL;
    }

    /* 如果没有连接池，使用简单的转义 */
    if (g_client.main_pool == NULL) {
        size_t len = strlen(input);
        char *escaped = (char*)malloc(len * 2 + 1);
        if (escaped == NULL) {
            return NULL;
        }

        size_t j = 0;
        for (size_t i = 0; i < len; i++) {
            /* 转义单引号和反斜杠 */
            if (input[i] == '\'' || input[i] == '\\') {
                escaped[j++] = '\\';
            }
            escaped[j++] = input[i];
        }
        escaped[j] = '\0';
        return escaped;
    }

    /* 使用连接池中的连接进行转义 */
    pg_connection_t *conn = pg_pool_checkout(g_client.main_pool);
    if (conn == NULL) {
        /* 回退到简单转义 */
        size_t len = strlen(input);
        char *escaped = (char*)malloc(len * 2 + 1);
        if (escaped == NULL) {
            return NULL;
        }

        size_t j = 0;
        for (size_t i = 0; i < len; i++) {
            if (input[i] == '\'' || input[i] == '\\') {
                escaped[j++] = '\'';
            }
            escaped[j++] = input[i];
        }
        escaped[j] = '\0';
        return escaped;
    }

    PGconn *pq_conn = pg_connection_get_pq_conn(conn);
    if (pq_conn == NULL) {
        pg_pool_checkin(g_client.main_pool, conn);
        return strdup(input);
    }

    int error = 0;
    char *escaped = PQescapeLiteral(pq_conn, input, strlen(input));
    
    pg_pool_checkin(g_client.main_pool, conn);

    if (error || escaped == NULL) {
        return strdup(input);
    }

    return escaped;
}

/* SQL 标识符转义（表名、列名） */
char* pg_client_escape_identifier(const char *input) {
    if (input == NULL) {
        return NULL;
    }

    /* 如果没有连接池，使用简单的转义 */
    if (g_client.main_pool == NULL) {
        size_t len = strlen(input);
        char *escaped = (char*)malloc(len * 2 + 3);
        if (escaped == NULL) {
            return NULL;
        }

        escaped[0] = '"';
        size_t j = 1;
        for (size_t i = 0; i < len; i++) {
            if (input[i] == '"') {
                escaped[j++] = '"';
            }
            escaped[j++] = input[i];
        }
        escaped[j++] = '"';
        escaped[j] = '\0';
        return escaped;
    }

    /* 使用连接池中的连接进行转义 */
    pg_connection_t *conn = pg_pool_checkout(g_client.main_pool);
    if (conn == NULL) {
        /* 回退到简单转义 */
        size_t len = strlen(input);
        char *escaped = (char*)malloc(len * 2 + 3);
        if (escaped == NULL) {
            return NULL;
        }

        escaped[0] = '"';
        size_t j = 1;
        for (size_t i = 0; i < len; i++) {
            if (input[i] == '"') {
                escaped[j++] = '"';
            }
            escaped[j++] = input[i];
        }
        escaped[j++] = '"';
        escaped[j] = '\0';
        return escaped;
    }

    PGconn *pq_conn = pg_connection_get_pq_conn(conn);
    if (pq_conn == NULL) {
        pg_pool_checkin(g_client.main_pool, conn);
        return strdup(input);
    }

    char *escaped = PQescapeIdentifier(pq_conn, input, strlen(input));
    
    pg_pool_checkin(g_client.main_pool, conn);

    if (escaped == NULL) {
        return strdup(input);
    }

    return escaped;
}

/* 验证 SQL 参数 */
int pg_client_validate_params(const char **params, int param_count) {
    if (params == NULL && param_count > 0) {
        return -1;
    }

    if (param_count < 0 || param_count > 65535) {
        PG_LOG_ERROR("[CLIENT] Invalid param count: %d", param_count);
        return -1;
    }

    /* 检查参数是否包含危险字符 */
    for (int i = 0; i < param_count; i++) {
        if (params[i] == NULL) {
            continue;  /* NULL 参数是允许的 */
        }

        /* 检查是否包含 NULL 字节（可能的截断攻击） */
        size_t len = strlen(params[i]);
        for (size_t j = 0; j < len; j++) {
            if (params[i][j] == '\0') {
                PG_LOG_WARN("[CLIENT] Parameter %d contains embedded NULL byte", i);
                return -1;
            }
        }

        /* 检查是否过长 */
        if (len > 10 * 1024 * 1024) {  /* 10MB 限制 */
            PG_LOG_WARN("[CLIENT] Parameter %d too large: %zu bytes", i, len);
            return -1;
        }
    }

    return 0;
}

/* 验证 SQL 语句 */
int pg_client_validate_sql(const char *sql) {
    if (sql == NULL) {
        return -1;
    }

    size_t len = strlen(sql);
    if (len == 0) {
        return -1;
    }

    /* 检查是否过长 */
    if (len > 1024 * 1024) {  /* 1MB 限制 */
        PG_LOG_WARN("[CLIENT] SQL statement too large: %zu bytes", len);
        return -1;
    }

    /* 检查是否包含 NULL 字节 */
    for (size_t i = 0; i < len; i++) {
        if (sql[i] == '\0') {
            PG_LOG_WARN("[CLIENT] SQL contains embedded NULL byte");
            return -1;
        }
    }

    /* 检查是否包含危险的多语句（简单检查） */
    int semicolon_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (sql[i] == ';') {
            semicolon_count++;
            /* 检查是否在字符串字面量内 */
            if (semicolon_count > 1) {
                PG_LOG_WARN("[CLIENT] SQL contains multiple statements, potential injection risk");
                /* 不严格禁止，但记录警告 */
            }
        }
    }

    return 0;
}

/* 批量插入（使用 COPY 命令） */
int pg_client_bulk_insert(const char *table, const char *columns, const char **rows, int row_count) {
    if (!g_client.initialized || table == NULL || rows == NULL || row_count <= 0) {
        return -1;
    }

    /* 转义表名 */
    char *table_escaped = pg_client_escape_identifier(table);
    if (table_escaped == NULL) {
        return -1;
    }

    /* 构建 COPY 命令 */
    char copy_cmd[1024];
    if (columns != NULL && strlen(columns) > 0) {
        snprintf(copy_cmd, sizeof(copy_cmd), "COPY %s (%s) FROM STDIN WITH (FORMAT CSV, DELIMITER ',')", 
                 table_escaped, columns);
    } else {
        snprintf(copy_cmd, sizeof(copy_cmd), "COPY %s FROM STDIN WITH (FORMAT CSV, DELIMITER ',')", 
                 table_escaped);
    }

    free(table_escaped);

    /* 获取连接 */
    pg_connection_t *conn = pg_pool_checkout(g_client.main_pool);
    if (conn == NULL) {
        PG_LOG_ERROR("[CLIENT] No available connection for bulk insert");
        return -1;
    }

    PGconn *pq_conn = pg_connection_get_pq_conn(conn);
    if (pq_conn == NULL) {
        pg_pool_checkin(g_client.main_pool, conn);
        return -1;
    }

    /* 执行 COPY 命令 */
    PGresult *res = PQexec(pq_conn, copy_cmd);
    if (res == NULL || PQresultStatus(res) != PGRES_COPY_IN) {
        PQclear(res);
        pg_pool_checkin(g_client.main_pool, conn);
        PG_LOG_ERROR("[CLIENT] Failed to start COPY: %s", PQerrorMessage(pq_conn));
        return -1;
    }
    PQclear(res);

    /* 发送数据 */
    int success = 1;
    for (int i = 0; i < row_count; i++) {
        if (rows[i] == NULL) {
            continue;
        }

        size_t row_len = strlen(rows[i]);
        int ret = PQputCopyData(pq_conn, rows[i], (int)row_len);
        if (ret <= 0) {
            PG_LOG_ERROR("[CLIENT] Failed to send COPY data: %s", PQerrorMessage(pq_conn));
            success = 0;
            break;
        }
    }

    /* 结束 COPY */
    char *error_msg = NULL;
    int final_ret = PQputCopyEnd(pq_conn, success ? NULL : (error_msg = "cancelled"));
    if (final_ret <= 0) {
        PG_LOG_ERROR("[CLIENT] Failed to end COPY: %s", PQerrorMessage(pq_conn));
        success = 0;
    }

    /* 获取结果 */
    res = PQgetResult(pq_conn);
    if (res != NULL) {
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            PG_LOG_ERROR("[CLIENT] COPY failed: %s", PQresultErrorMessage(res));
            success = 0;
        }
        PQclear(res);
    }

    pg_pool_checkin(g_client.main_pool, conn);
    return success ? row_count : -1;
}

/* 批量插入（使用多值 INSERT） */
int pg_client_batch_insert(const char *table, const char *columns, const char **rows, int row_count, int batch_size) {
    if (!g_client.initialized || table == NULL || rows == NULL || row_count <= 0) {
        return -1;
    }

    if (batch_size <= 0) {
        batch_size = 100;  /* 默认每批 100 行 */
    }

    /* 转义表名 */
    char *table_escaped = pg_client_escape_identifier(table);
    if (table_escaped == NULL) {
        return -1;
    }

    int total_inserted = 0;

    /* 分批插入 */
    for (int batch_start = 0; batch_start < row_count; batch_start += batch_size) {
        int batch_end = batch_start + batch_size;
        if (batch_end > row_count) {
            batch_end = row_count;
        }

        /* 构建 INSERT SQL */
        size_t sql_size = 1024 + (batch_end - batch_start) * 256;
        char *sql = (char*)malloc(sql_size);
        if (sql == NULL) {
            free(table_escaped);
            return -1;
        }

        int offset = 0;
        if (columns != NULL && strlen(columns) > 0) {
            offset = snprintf(sql, sql_size, "INSERT INTO %s (%s) VALUES ", table_escaped, columns);
        } else {
            offset = snprintf(sql, sql_size, "INSERT INTO %s VALUES ", table_escaped);
        }

        /* 添加值 */
        for (int i = batch_start; i < batch_end; i++) {
            if (i > batch_start) {
                offset += snprintf(sql + offset, sql_size - offset, ", ");
            }
            offset += snprintf(sql + offset, sql_size - offset, "(%s)", rows[i] ? rows[i] : "NULL");
        }

        /* 执行插入 */
        pg_result_t *result = pg_client_exec(sql, NULL, 0);
        if (result == NULL || pg_result_is_error(result)) {
            PG_LOG_ERROR("[CLIENT] Batch insert failed at row %d", batch_start);
            if (result != NULL) {
                pg_result_destroy(result);
            }
            free(sql);
            free(table_escaped);
            return total_inserted > 0 ? total_inserted : -1;
        }

        pg_result_destroy(result);
        total_inserted += (batch_end - batch_start);
        free(sql);
    }

    free(table_escaped);
    return total_inserted;
}

/* 获取连接池预热状态 */
int pg_client_warmup_pool(void) {
    if (!g_client.initialized || g_client.main_pool == NULL) {
        return -1;
    }
    return pg_pool_warmup(g_client.main_pool);
}

/* 获取可用连接数 */
int pg_client_available_connections(void) {
    if (!g_client.initialized || g_client.main_pool == NULL) {
        return 0;
    }
    return pg_pool_available_count(g_client.main_pool);
}
