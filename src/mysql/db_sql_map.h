#ifndef DB_SQL_MAP_H
#define DB_SQL_MAP_H

// ============================================================
// 数据库语法适配层
// ============================================================
// 使用方式：
//   1. 定义 USE_MYSQL 或 USE_POSTGRESQL 宏
//   2. #include "db_sql_map.h"
//   3. 业务代码中使用 SQL_EQ_xxx 宏
// ============================================================

// ============================================================
// 自增序列
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_SERIAL           INT AUTO_INCREMENT
    #define SQL_EQ_BIGSERIAL        BIGINT AUTO_INCREMENT
#else
    #define SQL_EQ_SERIAL           SERIAL
    #define SQL_EQ_BIGSERIAL        BIGSERIAL
#endif

// ============================================================
// 布尔类型
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_BOOL             TINYINT(1)
    #define SQL_EQ_TRUE             1
    #define SQL_EQ_FALSE            0
#else
    #define SQL_EQ_BOOL             BOOLEAN
    #define SQL_EQ_TRUE             TRUE
    #define SQL_EQ_FALSE            FALSE
#endif

// ============================================================
// 数值类型
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_SMALLINT         SMALLINT
    #define SQL_EQ_INT              INT
    #define SQL_EQ_BIGINT           BIGINT
    #define SQL_EQ_NUMERIC(p,s)     DECIMAL(p,s)
    #define SQL_EQ_FLOAT            FLOAT
    #define SQL_EQ_DOUBLE           DOUBLE
#else
    #define SQL_EQ_SMALLINT         SMALLINT
    #define SQL_EQ_INT              INTEGER
    #define SQL_EQ_BIGINT           BIGINT
    #define SQL_EQ_NUMERIC(p,s)     NUMERIC(p,s)
    #define SQL_EQ_FLOAT            REAL
    #define SQL_EQ_DOUBLE           DOUBLE PRECISION
#endif

// ============================================================
// 文本类型
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_TEXT             LONGTEXT
    #define SQL_EQ_SMALLTEXT        VARCHAR(255)
    #define SQL_EQ_CHAR(n)          CHAR(n)
    #define SQL_EQ_VARCHAR(n)       VARCHAR(n)
#else
    #define SQL_EQ_TEXT             TEXT
    #define SQL_EQ_SMALLTEXT        VARCHAR(255)
    #define SQL_EQ_CHAR(n)          CHAR(n)
    #define SQL_EQ_VARCHAR(n)       VARCHAR(n)
#endif

// ============================================================
// 日期时间类型
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_TIMESTAMP        DATETIME
    #define SQL_EQ_TIMESTAMPTZ      TIMESTAMP
    #define SQL_EQ_DATE             DATE
    #define SQL_EQ_TIME             TIME
#else
    #define SQL_EQ_TIMESTAMP        TIMESTAMP
    #define SQL_EQ_TIMESTAMPTZ      TIMESTAMPTZ
    #define SQL_EQ_DATE             DATE
    #define SQL_EQ_TIME             TIME
#endif

// ============================================================
// 二进制类型
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_BYTEA            BLOB
    #define SQL_EQ_BYTEA_LONG       LONGBLOB
#else
    #define SQL_EQ_BYTEA            BYTEA
    #define SQL_EQ_BYTEA_LONG       BYTEA
#endif

// ============================================================
// 数组类型（注意：MySQL 不支持原生数组，使用 JSON 模拟）
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_ARRAY_INT        JSON
    #define SQL_EQ_ARRAY_TEXT       JSON
#else
    #define SQL_EQ_ARRAY_INT        INT[]
    #define SQL_EQ_ARRAY_TEXT       TEXT[]
#endif

// ============================================================
// 字符串拼接
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_CONCAT(a, b)     CONCAT(a, b)
#else
    #define SQL_EQ_CONCAT(a, b)     (a || b)
#endif

// ============================================================
// 冲突处理（UPSERT）
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_CONFLICT_START   ON DUPLICATE KEY UPDATE
#else
    #define SQL_EQ_CONFLICT_START   ON CONFLICT DO UPDATE SET
#endif

// ============================================================
// RETURNING 子句
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_RETURNING(col)   /* MySQL: use SQL_EQ_LAST_INSERT_ID() instead */
#else
    #define SQL_EQ_RETURNING(col)   RETURNING col
#endif

// ============================================================
// 当前时间戳
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_NOW()            NOW()
    #define SQL_EQ_CURRENT_DATE()   CURDATE()
    #define SQL_EQ_CURRENT_TIME()   CURTIME()
#else
    #define SQL_EQ_NOW()            NOW()
    #define SQL_EQ_CURRENT_DATE()   CURRENT_DATE
    #define SQL_EQ_CURRENT_TIME()   CURRENT_TIME
#endif

// ============================================================
// 日期间隔
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_INTERVAL_DAY(n)      INTERVAL n DAY
    #define SQL_EQ_INTERVAL_HOUR(n)     INTERVAL n HOUR
    #define SQL_EQ_INTERVAL_MINUTE(n)   INTERVAL n MINUTE
    #define SQL_EQ_INTERVAL_SECOND(n)   INTERVAL n SECOND
    #define SQL_EQ_INTERVAL_MONTH(n)    INTERVAL n MONTH
    #define SQL_EQ_INTERVAL_YEAR(n)     INTERVAL n YEAR
#else
    #define SQL_EQ_INTERVAL_DAY(n)      ((n) || ' days')::INTERVAL
    #define SQL_EQ_INTERVAL_HOUR(n)     ((n) || ' hours')::INTERVAL
    #define SQL_EQ_INTERVAL_MINUTE(n)   ((n) || ' minutes')::INTERVAL
    #define SQL_EQ_INTERVAL_SECOND(n)   ((n) || ' seconds')::INTERVAL
    #define SQL_EQ_INTERVAL_MONTH(n)    ((n) || ' months')::INTERVAL
    #define SQL_EQ_INTERVAL_YEAR(n)     ((n) || ' years')::INTERVAL
#endif

// ============================================================
// 字符串函数
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_LENGTH(s)            LENGTH(s)
    #define SQL_EQ_CHAR_LENGTH(s)       CHAR_LENGTH(s)
    #define SQL_EQ_UPPER(s)             UPPER(s)
    #define SQL_EQ_LOWER(s)             LOWER(s)
    #define SQL_EQ_SUBSTR(s, p, l)      SUBSTRING(s, p, l)
    #define SQL_EQ_TRIM(s)              TRIM(s)
    #define SQL_EQ_REPLACE(s, a, b)     REPLACE(s, a, b)
    #define SQL_EQ_POSITION(a, s)       LOCATE(a, s)
    #define SQL_EQ_COALESCE(a, b)       IFNULL(a, b)
    #define SQL_EQ_NULLIF(a, b)         NULLIF(a, b)
    #define SQL_EQ_RANDOM()             RAND()
    #define SQL_EQ_LPAD(s, n, c)        LPAD(s, n, c)
    #define SQL_EQ_RPAD(s, n, c)        RPAD(s, n, c)
#else
    #define SQL_EQ_LENGTH(s)            LENGTH(s)
    #define SQL_EQ_CHAR_LENGTH(s)       CHAR_LENGTH(s)
    #define SQL_EQ_UPPER(s)             UPPER(s)
    #define SQL_EQ_LOWER(s)             LOWER(s)
    #define SQL_EQ_SUBSTR(s, p, l)      SUBSTRING(s, p, l)
    #define SQL_EQ_TRIM(s)              TRIM(s)
    #define SQL_EQ_REPLACE(s, a, b)     REPLACE(s, a, b)
    #define SQL_EQ_POSITION(a, s)       POSITION(a IN s)
    #define SQL_EQ_COALESCE(a, b)       COALESCE(a, b)
    #define SQL_EQ_NULLIF(a, b)         NULLIF(a, b)
    #define SQL_EQ_RANDOM()             RANDOM()
    #define SQL_EQ_LPAD(s, n, c)        LPAD(s, n, c)
    #define SQL_EQ_RPAD(s, n, c)        RPAD(s, n, c)
#endif

// ============================================================
// 正则表达式
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_REGEXP(col, pat)     (col REGEXP pat)
    #define SQL_EQ_REGEXP_ICASE(col,pat)(col REGEXP pat)
#else
    #define SQL_EQ_REGEXP(col, pat)     (col ~ pat)
    #define SQL_EQ_REGEXP_ICASE(col,pat)(col ~* pat)
#endif

// ============================================================
// 数字函数
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_ABS(x)               ABS(x)
    #define SQL_EQ_CEIL(x)              CEIL(x)
    #define SQL_EQ_FLOOR(x)             FLOOR(x)
    #define SQL_EQ_ROUND(x, n)          ROUND(x, n)
    #define SQL_EQ_MOD(a, b)            MOD(a, b)
    #define SQL_EQ_POWER(x, y)          POW(x, y)
    #define SQL_EQ_SQRT(x)              SQRT(x)
    #define SQL_EQ_LOG(x)               LOG(x)
#else
    #define SQL_EQ_ABS(x)               ABS(x)
    #define SQL_EQ_CEIL(x)              CEIL(x)
    #define SQL_EQ_FLOOR(x)             FLOOR(x)
    #define SQL_EQ_ROUND(x, n)          ROUND(x, n)
    #define SQL_EQ_MOD(a, b)            MOD(a, b)
    #define SQL_EQ_POWER(x, y)          POWER(x, y)
    #define SQL_EQ_SQRT(x)              SQRT(x)
    #define SQL_EQ_LOG(x)               LOG(x)
#endif

// ============================================================
// 日期时间函数
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_EXTRACT_YEAR(d)      YEAR(d)
    #define SQL_EQ_EXTRACT_MONTH(d)     MONTH(d)
    #define SQL_EQ_EXTRACT_DAY(d)       DAY(d)
    #define SQL_EQ_EXTRACT_HOUR(t)      HOUR(t)
    #define SQL_EQ_EXTRACT_MINUTE(t)    MINUTE(t)
    #define SQL_EQ_EXTRACT_SECOND(t)    SECOND(t)
    #define SQL_EQ_AGE(ts1, ts2)        TIMESTAMPDIFF(SECOND, ts2, ts1)
    #define SQL_EQ_TO_CHAR(dt, fmt)     DATE_FORMAT(dt, fmt)
    #define SQL_EQ_TO_DATE(s, fmt)      STR_TO_DATE(s, fmt)
#else
    #define SQL_EQ_EXTRACT_YEAR(d)      EXTRACT(YEAR FROM d)
    #define SQL_EQ_EXTRACT_MONTH(d)     EXTRACT(MONTH FROM d)
    #define SQL_EQ_EXTRACT_DAY(d)       EXTRACT(DAY FROM d)
    #define SQL_EQ_EXTRACT_HOUR(t)      EXTRACT(HOUR FROM t)
    #define SQL_EQ_EXTRACT_MINUTE(t)    EXTRACT(MINUTE FROM t)
    #define SQL_EQ_EXTRACT_SECOND(t)    EXTRACT(SECOND FROM t)
    #define SQL_EQ_AGE(ts1, ts2)        AGE(ts1, ts2)
    #define SQL_EQ_TO_CHAR(dt, fmt)     TO_CHAR(dt, fmt)
    #define SQL_EQ_TO_DATE(s, fmt)      TO_DATE(s, fmt)
#endif

// ============================================================
// JSON 操作
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_JSON_TYPE            JSON
    #define SQL_EQ_JSON_GET(col, key)   (JSON_EXTRACT(col, CONCAT('$.', key)))
    #define SQL_EQ_JSON_GET_TEXT(col, key) (JSON_UNQUOTE(JSON_EXTRACT(col, CONCAT('$.', key))))
    #define SQL_EQ_JSON_SET(col, key, val) (JSON_SET(col, CONCAT('$.', key), val))
#else
    #define SQL_EQ_JSON_TYPE            JSONB
    #define SQL_EQ_JSON_GET(col, key)   (col -> key)
    #define SQL_EQ_JSON_GET_TEXT(col, key) (col ->> key)
    #define SQL_EQ_JSON_SET(col, key, val) (jsonb_set(col, ARRAY[key], val))
#endif

// ============================================================
// 条件判断
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_IF(cond, t, f)       IF(cond, t, f)
#else
    #define SQL_EQ_IF(cond, t, f)       (CASE WHEN cond THEN t ELSE f END)
#endif

// ============================================================
// 分页查询
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_LIMIT_OFFSET(pageSize, offset)   LIMIT offset, pageSize
#else
    #define SQL_EQ_LIMIT_OFFSET(pageSize, offset)   LIMIT pageSize OFFSET offset
#endif

// ============================================================
// 聚合函数
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_COUNT(col)           COUNT(col)
    #define SQL_EQ_SUM(col)             SUM(col)
    #define SQL_EQ_AVG(col)             AVG(col)
    #define SQL_EQ_MAX(col)             MAX(col)
    #define SQL_EQ_MIN(col)             MIN(col)
    #define SQL_EQ_STRING_AGG(col, sep) GROUP_CONCAT(col SEPARATOR sep)
#else
    #define SQL_EQ_COUNT(col)           COUNT(col)
    #define SQL_EQ_SUM(col)             SUM(col)
    #define SQL_EQ_AVG(col)             AVG(col)
    #define SQL_EQ_MAX(col)             MAX(col)
    #define SQL_EQ_MIN(col)             MIN(col)
    #define SQL_EQ_STRING_AGG(col, sep) STRING_AGG(col, sep)
#endif

// ============================================================
// DML 语句
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_TRUNCATE(t)          TRUNCATE TABLE t
    #define SQL_EQ_LAST_INSERT_ID()     LAST_INSERT_ID()
#else
    #define SQL_EQ_TRUNCATE(t)          TRUNCATE TABLE t RESTART IDENTITY
    #define SQL_EQ_LAST_INSERT_ID()     LASTVAL()
#endif

// ============================================================
// 约束和注释
// ============================================================
#ifdef USE_MYSQL
    #define SQL_EQ_COMMENT(text)        COMMENT #text
    #define SQL_EQ_DEFAULT_NULL         DEFAULT NULL
    #define SQL_EQ_NOT_NULL             NOT NULL
    #define SQL_EQ_PRIMARY_KEY          PRIMARY KEY
    #define SQL_EQ_UNIQUE               UNIQUE
    #define SQL_EQ_INDEX(col)           INDEX (col)
    #define SQL_EQ_AUTO_INCREMENT       AUTO_INCREMENT
#else
    #define SQL_EQ_COMMENT(text)        COMMENT #text
    #define SQL_EQ_DEFAULT_NULL         DEFAULT NULL
    #define SQL_EQ_NOT_NULL             NOT NULL
    #define SQL_EQ_PRIMARY_KEY          PRIMARY KEY
    #define SQL_EQ_UNIQUE               UNIQUE
    #define SQL_EQ_INDEX(col)           INDEX (col)
    #define SQL_EQ_AUTO_INCREMENT       /* PG uses SERIAL type */
#endif

// ============================================================
// 标识符引用
// ============================================================
#ifdef USE_MYSQL
    #define SQL_QUOTE_ID(name)      "`" #name "`"
#else
    #define SQL_QUOTE_ID(name)      "\"" #name "\""
#endif

// ============================================================
// 参数占位符
// ============================================================
#ifdef USE_MYSQL
    #define SQL_PARAM(n)            ?
#else
    #define SQL_PARAM(n)            "$" #n
#endif

#endif  // DB_SQL_MAP_H
