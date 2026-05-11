#ifndef PG_TYPE_CODEC_H
#define PG_TYPE_CODEC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PostgreSQL OID 类型定义 */
typedef uint32_t pg_oid_t;

/* 常用 PostgreSQL OID */
#define PG_OID_BOOL 16
#define PG_OID_BYTEA 17
#define PG_OID_CHAR 18
#define PG_OID_NAME 19
#define PG_OID_INT8 20
#define PG_OID_INT2 21
#define PG_OID_INT4 23
#define PG_OID_TEXT 25
#define PG_OID_OID 26
#define PG_OID_FLOAT4 700
#define PG_OID_FLOAT8 701
#define PG_OID_ABSTIME 702
#define PG_OID_RELTIME 703
#define PG_OID_TINTERVAL 704
#define PG_OID_UNKNOWN 705
#define PG_OID_VARCHAR 1043
#define PG_OID_DATE 1082
#define PG_OID_TIME 1083
#define PG_OID_TIMESTAMP 1114
#define PG_OID_TIMESTAMPTZ 1184
#define PG_OID_NUMERIC 1700
#define PG_OID_JSONB 3802

/* C 类型值 */
typedef struct pg_value {
    pg_oid_t oid;
    int is_null;
    size_t length;
    char *data;
} pg_value_t;

/* 类型转换函数 */

/* BOOL */
int pg_bool_from_text(const char *text, size_t len, int *value);
int pg_bool_to_text(int value, char *text, size_t *len);

/* INT2 (smallint) */
int pg_int2_from_text(const char *text, size_t len, int16_t *value);
int pg_int2_to_text(int16_t value, char *text, size_t *len);

/* INT4 (integer) */
int pg_int4_from_text(const char *text, size_t len, int32_t *value);
int pg_int4_to_text(int32_t value, char *text, size_t *len);

/* INT8 (bigint) */
int pg_int8_from_text(const char *text, size_t len, int64_t *value);
int pg_int8_to_text(int64_t value, char *text, size_t *len);

/* FLOAT4 (real) */
int pg_float4_from_text(const char *text, size_t len, float *value);
int pg_float4_to_text(float value, char *text, size_t *len);

/* FLOAT8 (double precision) */
int pg_float8_from_text(const char *text, size_t len, double *value);
int pg_float8_to_text(double value, char *text, size_t *len);

/* TEXT / VARCHAR */
int pg_text_from_text(const char *text, size_t len, char *value, size_t *value_len);
int pg_text_to_text(const char *text, size_t len, char *value, size_t *value_len);

/* TIMESTAMP */
int pg_timestamp_from_text(const char *text, size_t len, int64_t *value);
int pg_timestamp_to_text(int64_t value, char *text, size_t *len);

/* DATE */
int pg_date_from_text(const char *text, size_t len, int32_t *value);
int pg_date_to_text(int32_t value, char *text, size_t *len);

/* NUMERIC (简化版，只支持整数) */
int pg_numeric_from_text(const char *text, size_t len, int64_t *value);
int pg_numeric_to_text(int64_t value, char *text, size_t *len);

/* 通用转换 */
int pg_value_from_text(pg_oid_t oid, const char *text, size_t len, pg_value_t *value);
int pg_value_to_text(const pg_value_t *value, char *text, size_t *len);

/* 释放 pg_value */
void pg_value_free(pg_value_t *value);

/* OID 类型名称 */
const char* pg_oid_to_name(pg_oid_t oid);

#ifdef __cplusplus
}
#endif

#endif /* PG_TYPE_CODEC_H */
