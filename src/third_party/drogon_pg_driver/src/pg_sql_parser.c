#include "pg_sql_parser.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* 大小写不敏感字符串查找（Windows 兼容） */
static const char* pg_strcasestr(const char *haystack, const char *needle) {
    if (haystack == NULL || needle == NULL) {
        return NULL;
    }

    int needle_len = strlen(needle);
    int haystack_len = strlen(haystack);

    if (needle_len == 0 || needle_len > haystack_len) {
        return NULL;
    }

    for (int i = 0; i <= haystack_len - needle_len; i++) {
        int match = 1;
        for (int j = 0; j < needle_len; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) {
            return haystack + i;
        }
    }

    return NULL;
}

/* 跳过空白字符 */
static const char* skip_whitespace(const char *sql) {
    while (sql && *sql && isspace((unsigned char)*sql)) {
        sql++;
    }
    return sql;
}

/* 解析 SQL 类型 */
pg_sql_type_t pg_sql_parse_type(const char *sql) {
    if (sql == NULL || *sql == '\0') {
        return PG_SQL_UNKNOWN;
    }

    /* 跳过空白 */
    sql = skip_whitespace(sql);

    /* 跳过注释 */
    if (*sql == '-' && *(sql + 1) == '-') {
        /* 单行注释，跳过到行尾 */
        while (*sql && *sql != '\n') {
            sql++;
        }
        sql = skip_whitespace(sql);
    } else if (*sql == '/' && *(sql + 1) == '*') {
        /* 多行注释，跳过到 */
        sql += 2;
        while (*sql && !(*sql == '*' && *(sql + 1) == '/')) {
            sql++;
        }
        if (*sql) {
            sql += 2;
        }
        sql = skip_whitespace(sql);
    }

    /* 解析关键字 */
    if (strncasecmp(sql, "SELECT", 6) == 0 && !isalnum((unsigned char)*(sql + 6))) {
        return PG_SQL_SELECT;
    }
    if (strncasecmp(sql, "INSERT", 6) == 0 && !isalnum((unsigned char)*(sql + 6))) {
        return PG_SQL_INSERT;
    }
    if (strncasecmp(sql, "UPDATE", 6) == 0 && !isalnum((unsigned char)*(sql + 6))) {
        return PG_SQL_UPDATE;
    }
    if (strncasecmp(sql, "DELETE", 6) == 0 && !isalnum((unsigned char)*(sql + 6))) {
        return PG_SQL_DELETE;
    }
    if (strncasecmp(sql, "BEGIN", 5) == 0 && !isalnum((unsigned char)*(sql + 5))) {
        return PG_SQL_BEGIN;
    }
    if (strncasecmp(sql, "COMMIT", 6) == 0 && !isalnum((unsigned char)*(sql + 6))) {
        return PG_SQL_COMMIT;
    }
    if (strncasecmp(sql, "ROLLBACK", 8) == 0 && !isalnum((unsigned char)*(sql + 8))) {
        return PG_SQL_ROLLBACK;
    }
    if (strncasecmp(sql, "CREATE", 6) == 0 && !isalnum((unsigned char)*(sql + 6))) {
        return PG_SQL_CREATE;
    }
    if (strncasecmp(sql, "DROP", 4) == 0 && !isalnum((unsigned char)*(sql + 4))) {
        return PG_SQL_DROP;
    }
    if (strncasecmp(sql, "ALTER", 5) == 0 && !isalnum((unsigned char)*(sql + 5))) {
        return PG_SQL_ALTER;
    }
    if (strncasecmp(sql, "TRUNCATE", 8) == 0 && !isalnum((unsigned char)*(sql + 8))) {
        return PG_SQL_TRUNCATE;
    }
    if (strncasecmp(sql, "COPY", 4) == 0 && !isalnum((unsigned char)*(sql + 4))) {
        return PG_SQL_COPY;
    }
    if (strncasecmp(sql, "VACUUM", 6) == 0 && !isalnum((unsigned char)*(sql + 6))) {
        return PG_SQL_VACUUM;
    }
    if (strncasecmp(sql, "ANALYZE", 7) == 0 && !isalnum((unsigned char)*(sql + 7))) {
        return PG_SQL_ANALYZE;
    }
    if (strncasecmp(sql, "EXPLAIN", 7) == 0 && !isalnum((unsigned char)*(sql + 7))) {
        return PG_SQL_EXPLAIN;
    }
    if (strncasecmp(sql, "SHOW", 4) == 0 && !isalnum((unsigned char)*(sql + 4))) {
        return PG_SQL_SHOW;
    }

    return PG_SQL_OTHER;
}

/* 获取 SQL 分类 */
pg_sql_class_t pg_sql_get_class(pg_sql_type_t type) {
    switch (type) {
        case PG_SQL_SELECT:
        case PG_SQL_SHOW:
        case PG_SQL_EXPLAIN:
            return PG_SQL_CLASS_READ;

        case PG_SQL_INSERT:
        case PG_SQL_UPDATE:
        case PG_SQL_DELETE:
            return PG_SQL_CLASS_WRITE;

        case PG_SQL_CREATE:
        case PG_SQL_DROP:
        case PG_SQL_ALTER:
        case PG_SQL_TRUNCATE:
            return PG_SQL_CLASS_DDL;

        case PG_SQL_BEGIN:
        case PG_SQL_COMMIT:
        case PG_SQL_ROLLBACK:
            return PG_SQL_CLASS_TRANSACTION;

        case PG_SQL_VACUUM:
        case PG_SQL_ANALYZE:
        case PG_SQL_COPY:
            return PG_SQL_CLASS_UTILITY;

        default:
            return PG_SQL_CLASS_UNKNOWN;
    }
}

/* 判断是否为读操作 */
int pg_sql_is_read(const char *sql) {
    pg_sql_type_t type = pg_sql_parse_type(sql);
    return pg_sql_get_class(type) == PG_SQL_CLASS_READ;
}

/* 判断是否为写操作 */
int pg_sql_is_write(const char *sql) {
    pg_sql_type_t type = pg_sql_parse_type(sql);
    return pg_sql_get_class(type) == PG_SQL_CLASS_WRITE;
}

/* 判断是否为事务操作 */
int pg_sql_is_transaction(const char *sql) {
    pg_sql_type_t type = pg_sql_parse_type(sql);
    return pg_sql_get_class(type) == PG_SQL_CLASS_TRANSACTION;
}

/* 判断是否需要独占连接 */
int pg_sql_needs_exclusive_connection(const char *sql) {
    pg_sql_type_t type = pg_sql_parse_type(sql);
    
    /* 事务操作需要独占连接 */
    if (pg_sql_get_class(type) == PG_SQL_CLASS_TRANSACTION) {
        return 1;
    }

    /* BEGIN 开始事务，需要独占连接 */
    if (type == PG_SQL_BEGIN) {
        return 1;
    }

    return 0;
}

/* 获取 SQL 类型名称 */
const char* pg_sql_type_name(pg_sql_type_t type) {
    switch (type) {
        case PG_SQL_SELECT:   return "SELECT";
        case PG_SQL_INSERT:   return "INSERT";
        case PG_SQL_UPDATE:   return "UPDATE";
        case PG_SQL_DELETE:   return "DELETE";
        case PG_SQL_BEGIN:    return "BEGIN";
        case PG_SQL_COMMIT:   return "COMMIT";
        case PG_SQL_ROLLBACK: return "ROLLBACK";
        case PG_SQL_CREATE:   return "CREATE";
        case PG_SQL_DROP:     return "DROP";
        case PG_SQL_ALTER:    return "ALTER";
        case PG_SQL_TRUNCATE: return "TRUNCATE";
        case PG_SQL_COPY:     return "COPY";
        case PG_SQL_VACUUM:   return "VACUUM";
        case PG_SQL_ANALYZE:  return "ANALYZE";
        case PG_SQL_EXPLAIN:  return "EXPLAIN";
        case PG_SQL_SHOW:     return "SHOW";
        case PG_SQL_OTHER:    return "OTHER";
        default:              return "UNKNOWN";
    }
}

/* 获取 SQL 分类名称 */
const char* pg_sql_class_name(pg_sql_class_t sql_class) {
    switch (sql_class) {
        case PG_SQL_CLASS_READ:        return "READ";
        case PG_SQL_CLASS_WRITE:       return "WRITE";
        case PG_SQL_CLASS_DDL:         return "DDL";
        case PG_SQL_CLASS_TRANSACTION: return "TRANSACTION";
        case PG_SQL_CLASS_UTILITY:     return "UTILITY";
        default:                       return "UNKNOWN";
    }
}

/* 提取表名（简单实现） */
int pg_sql_extract_table_name(const char *sql, char *table_name, int max_len) {
    if (sql == NULL || table_name == NULL || max_len <= 0) {
        return -1;
    }

    sql = skip_whitespace(sql);
    pg_sql_type_t type = pg_sql_parse_type(sql);

    const char *table_start = NULL;

    switch (type) {
        case PG_SQL_SELECT:
            /* SELECT ... FROM table */
            table_start = pg_strcasestr(sql, "FROM");
            if (table_start) {
                table_start += 4;
                table_start = skip_whitespace(table_start);
            }
            break;

        case PG_SQL_INSERT:
            /* INSERT INTO table */
            table_start = pg_strcasestr(sql, "INTO");
            if (table_start) {
                table_start += 4;
                table_start = skip_whitespace(table_start);
            }
            break;

        case PG_SQL_UPDATE:
            /* UPDATE table */
            sql = skip_whitespace(sql);
            sql += 6; /* skip UPDATE */
            table_start = skip_whitespace(sql);
            break;

        case PG_SQL_DELETE:
            /* DELETE FROM table */
            table_start = pg_strcasestr(sql, "FROM");
            if (table_start) {
                table_start += 4;
                table_start = skip_whitespace(table_start);
            }
            break;

        default:
            return -1;
    }

    if (table_start == NULL || *table_start == '\0') {
        return -1;
    }

    /* 复制表名 */
    int i = 0;
    while (i < max_len - 1 && *table_start && !isspace((unsigned char)*table_start) && *table_start != '(' && *table_start != ';') {
        table_name[i++] = *table_start++;
    }
    table_name[i] = '\0';

    return i > 0 ? 0 : -1;
}
