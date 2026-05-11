#ifndef PG_SQL_PARSER_H
#define PG_SQL_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

/* SQL 类型 */
typedef enum {
    PG_SQL_UNKNOWN,
    PG_SQL_SELECT,
    PG_SQL_INSERT,
    PG_SQL_UPDATE,
    PG_SQL_DELETE,
    PG_SQL_BEGIN,
    PG_SQL_COMMIT,
    PG_SQL_ROLLBACK,
    PG_SQL_CREATE,
    PG_SQL_DROP,
    PG_SQL_ALTER,
    PG_SQL_TRUNCATE,
    PG_SQL_COPY,
    PG_SQL_VACUUM,
    PG_SQL_ANALYZE,
    PG_SQL_EXPLAIN,
    PG_SQL_SHOW,
    PG_SQL_OTHER
} pg_sql_type_t;

/* SQL 分类 */
typedef enum {
    PG_SQL_CLASS_READ,      /* 读操作：SELECT, SHOW, EXPLAIN */
    PG_SQL_CLASS_WRITE,     /* 写操作：INSERT, UPDATE, DELETE */
    PG_SQL_CLASS_DDL,       /* DDL：CREATE, DROP, ALTER, TRUNCATE */
    PG_SQL_CLASS_TRANSACTION, /* 事务：BEGIN, COMMIT, ROLLBACK */
    PG_SQL_CLASS_UTILITY,   /* 工具：VACUUM, ANALYZE, COPY */
    PG_SQL_CLASS_UNKNOWN
} pg_sql_class_t;

/* 解析 SQL 类型 */
pg_sql_type_t pg_sql_parse_type(const char *sql);

/* 获取 SQL 分类 */
pg_sql_class_t pg_sql_get_class(pg_sql_type_t type);

/* 判断是否为读操作 */
int pg_sql_is_read(const char *sql);

/* 判断是否为写操作 */
int pg_sql_is_write(const char *sql);

/* 判断是否为事务操作 */
int pg_sql_is_transaction(const char *sql);

/* 判断是否需要独占连接 */
int pg_sql_needs_exclusive_connection(const char *sql);

/* 获取 SQL 类型名称 */
const char* pg_sql_type_name(pg_sql_type_t type);

/* 获取 SQL 分类名称 */
const char* pg_sql_class_name(pg_sql_class_t sql_class);

/* 提取表名（简单实现） */
int pg_sql_extract_table_name(const char *sql, char *table_name, int max_len);

#ifdef __cplusplus
}
#endif

#endif /* PG_SQL_PARSER_H */
