#include "pg_type_codec.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* 辅助函数：去除空白字符 */
static void trim_whitespace(const char *str, size_t len, const char **start, size_t *new_len) {
    *start = str;
    *new_len = len;

    while (*new_len > 0 && isspace((unsigned char)**start)) {
        (*start)++;
        (*new_len)--;
    }

    while (*new_len > 0 && isspace((unsigned char)(*start)[*new_len - 1])) {
        (*new_len)--;
    }
}

/* BOOL */
int pg_bool_from_text(const char *text, size_t len, int *value) {
    const char *start;
    size_t new_len;

    if (text == NULL || value == NULL) {
        return -1;
    }

    trim_whitespace(text, len, &start, &new_len);

    if (new_len == 0) {
        *value = 0;
        return 0;
    }

    if (strncasecmp(start, "t", new_len) == 0 ||
        strncasecmp(start, "true", new_len) == 0 ||
        strncasecmp(start, "yes", new_len) == 0 ||
        strncasecmp(start, "y", new_len) == 0 ||
        strncasecmp(start, "1", new_len) == 0) {
        *value = 1;
        return 0;
    }

    if (strncasecmp(start, "f", new_len) == 0 ||
        strncasecmp(start, "false", new_len) == 0 ||
        strncasecmp(start, "no", new_len) == 0 ||
        strncasecmp(start, "n", new_len) == 0 ||
        strncasecmp(start, "0", new_len) == 0) {
        *value = 0;
        return 0;
    }

    return -1;
}

int pg_bool_to_text(int value, char *text, size_t *len) {
    if (text == NULL || len == NULL) {
        return -1;
    }

    if (value) {
        if (*len < 5) {
            return -1;
        }
        strcpy(text, "true");
        *len = 4;
    } else {
        if (*len < 6) {
            return -1;
        }
        strcpy(text, "false");
        *len = 5;
    }

    return 0;
}

/* INT2 */
int pg_int2_from_text(const char *text, size_t len, int16_t *value) {
    const char *start;
    size_t new_len;
    char temp[32];

    if (text == NULL || value == NULL) {
        return -1;
    }

    trim_whitespace(text, len, &start, &new_len);

    if (new_len == 0) {
        return -1;
    }

    if (new_len >= sizeof(temp)) {
        return -1;
    }

    memcpy(temp, start, new_len);
    temp[new_len] = '\0';

    long val = strtol(temp, NULL, 10);
    if (val < INT16_MIN || val > INT16_MAX) {
        return -1;
    }

    *value = (int16_t)val;
    return 0;
}

int pg_int2_to_text(int16_t value, char *text, size_t *len) {
    if (text == NULL || len == NULL) {
        return -1;
    }

    int written = snprintf(text, *len, "%hd", value);
    if (written < 0 || (size_t)written >= *len) {
        return -1;
    }

    *len = written;
    return 0;
}

/* INT4 */
int pg_int4_from_text(const char *text, size_t len, int32_t *value) {
    const char *start;
    size_t new_len;
    char temp[32];

    if (text == NULL || value == NULL) {
        return -1;
    }

    trim_whitespace(text, len, &start, &new_len);

    if (new_len == 0) {
        return -1;
    }

    if (new_len >= sizeof(temp)) {
        return -1;
    }

    memcpy(temp, start, new_len);
    temp[new_len] = '\0';

    long val = strtol(temp, NULL, 10);
    if (val < INT32_MIN || val > INT32_MAX) {
        return -1;
    }

    *value = (int32_t)val;
    return 0;
}

int pg_int4_to_text(int32_t value, char *text, size_t *len) {
    if (text == NULL || len == NULL) {
        return -1;
    }

    int written = snprintf(text, *len, "%d", value);
    if (written < 0 || (size_t)written >= *len) {
        return -1;
    }

    *len = written;
    return 0;
}

/* INT8 */
int pg_int8_from_text(const char *text, size_t len, int64_t *value) {
    const char *start;
    size_t new_len;
    char temp[32];

    if (text == NULL || value == NULL) {
        return -1;
    }

    trim_whitespace(text, len, &start, &new_len);

    if (new_len == 0) {
        return -1;
    }

    if (new_len >= sizeof(temp)) {
        return -1;
    }

    memcpy(temp, start, new_len);
    temp[new_len] = '\0';

    long long val = strtoll(temp, NULL, 10);
    *value = (int64_t)val;
    return 0;
}

int pg_int8_to_text(int64_t value, char *text, size_t *len) {
    if (text == NULL || len == NULL) {
        return -1;
    }

    int written = snprintf(text, *len, "%lld", (long long)value);
    if (written < 0 || (size_t)written >= *len) {
        return -1;
    }

    *len = written;
    return 0;
}

/* FLOAT4 */
int pg_float4_from_text(const char *text, size_t len, float *value) {
    const char *start;
    size_t new_len;
    char temp[64];

    if (text == NULL || value == NULL) {
        return -1;
    }

    trim_whitespace(text, len, &start, &new_len);

    if (new_len == 0) {
        return -1;
    }

    if (new_len >= sizeof(temp)) {
        return -1;
    }

    memcpy(temp, start, new_len);
    temp[new_len] = '\0';

    *value = strtof(temp, NULL);
    return 0;
}

int pg_float4_to_text(float value, char *text, size_t *len) {
    if (text == NULL || len == NULL) {
        return -1;
    }

    int written = snprintf(text, *len, "%f", value);
    if (written < 0 || (size_t)written >= *len) {
        return -1;
    }

    *len = written;
    return 0;
}

/* FLOAT8 */
int pg_float8_from_text(const char *text, size_t len, double *value) {
    const char *start;
    size_t new_len;
    char temp[64];

    if (text == NULL || value == NULL) {
        return -1;
    }

    trim_whitespace(text, len, &start, &new_len);

    if (new_len == 0) {
        return -1;
    }

    if (new_len >= sizeof(temp)) {
        return -1;
    }

    memcpy(temp, start, new_len);
    temp[new_len] = '\0';

    *value = strtod(temp, NULL);
    return 0;
}

int pg_float8_to_text(double value, char *text, size_t *len) {
    if (text == NULL || len == NULL) {
        return -1;
    }

    int written = snprintf(text, *len, "%lf", value);
    if (written < 0 || (size_t)written >= *len) {
        return -1;
    }

    *len = written;
    return 0;
}

/* TEXT / VARCHAR */
int pg_text_from_text(const char *text, size_t len, char *value, size_t *value_len) {
    if (text == NULL || value == NULL || value_len == NULL) {
        return -1;
    }

    if (*value_len < len + 1) {
        return -1;
    }

    memcpy(value, text, len);
    value[len] = '\0';
    *value_len = len;
    return 0;
}

int pg_text_to_text(const char *text, size_t len, char *value, size_t *value_len) {
    return pg_text_from_text(text, len, value, value_len);
}

/* TIMESTAMP (简化版，假设是 Unix 时间戳) */
int pg_timestamp_from_text(const char *text, size_t len, int64_t *value) {
    return pg_int8_from_text(text, len, value);
}

int pg_timestamp_to_text(int64_t value, char *text, size_t *len) {
    return pg_int8_to_text(value, text, len);
}

/* DATE (简化版，假设是 Unix 时间戳天数) */
int pg_date_from_text(const char *text, size_t len, int32_t *value) {
    return pg_int4_from_text(text, len, value);
}

int pg_date_to_text(int32_t value, char *text, size_t *len) {
    return pg_int4_to_text(value, text, len);
}

/* NUMERIC (简化版，只支持整数) */
int pg_numeric_from_text(const char *text, size_t len, int64_t *value) {
    return pg_int8_from_text(text, len, value);
}

int pg_numeric_to_text(int64_t value, char *text, size_t *len) {
    return pg_int8_to_text(value, text, len);
}

/* 通用转换 */
int pg_value_from_text(pg_oid_t oid, const char *text, size_t len, pg_value_t *value) {
    if (value == NULL) {
        return -1;
    }

    value->oid = oid;
    value->is_null = (text == NULL);

    if (value->is_null) {
        value->length = 0;
        value->data = NULL;
        return 0;
    }

    value->length = len;
    value->data = (char*)malloc(len);
    if (value->data == NULL) {
        return -1;
    }

    memcpy(value->data, text, len);
    return 0;
}

int pg_value_to_text(const pg_value_t *value, char *text, size_t *len) {
    if (value == NULL || text == NULL || len == NULL) {
        return -1;
    }

    if (value->is_null) {
        text[0] = '\0';
        *len = 0;
        return 0;
    }

    if (*len < value->length + 1) {
        return -1;
    }

    memcpy(text, value->data, value->length);
    text[value->length] = '\0';
    *len = value->length;
    return 0;
}

/* 释放 pg_value */
void pg_value_free(pg_value_t *value) {
    if (value == NULL) {
        return;
    }

    if (value->data != NULL) {
        free(value->data);
        value->data = NULL;
    }

    value->length = 0;
    value->is_null = 1;
}

/* OID 类型名称 */
const char* pg_oid_to_name(pg_oid_t oid) {
    switch (oid) {
        case PG_OID_BOOL: return "bool";
        case PG_OID_BYTEA: return "bytea";
        case PG_OID_CHAR: return "char";
        case PG_OID_NAME: return "name";
        case PG_OID_INT8: return "int8";
        case PG_OID_INT2: return "int2";
        case PG_OID_INT4: return "int4";
        case PG_OID_TEXT: return "text";
        case PG_OID_OID: return "oid";
        case PG_OID_FLOAT4: return "float4";
        case PG_OID_FLOAT8: return "float8";
        case PG_OID_ABSTIME: return "abstime";
        case PG_OID_RELTIME: return "reltime";
        case PG_OID_TINTERVAL: return "tinterval";
        case PG_OID_UNKNOWN: return "unknown";
        case PG_OID_VARCHAR: return "varchar";
        case PG_OID_DATE: return "date";
        case PG_OID_TIME: return "time";
        case PG_OID_TIMESTAMP: return "timestamp";
        case PG_OID_TIMESTAMPTZ: return "timestamptz";
        case PG_OID_NUMERIC: return "numeric";
        case PG_OID_JSONB: return "jsonb";
        default: return "unknown";
    }
}
