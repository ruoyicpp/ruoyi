#include "pg_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* 全局日志配置 */
static struct {
    pg_log_level_t min_level;
    int targets;
    char *log_file_path;
    FILE *log_file;
    pg_log_callback_t callback;
    void *callback_user_data;
    int show_timestamp;
    int show_file_line;
    int show_level;
    int initialized;
} g_logger = {0};

/* 获取日志级别名称 */
const char* pg_logger_level_name(pg_log_level_t level) {
    switch (level) {
        case PG_LOG_TRACE: return "TRACE";
        case PG_LOG_DEBUG: return "DEBUG";
        case PG_LOG_INFO:  return "INFO";
        case PG_LOG_WARN:  return "WARN";
        case PG_LOG_ERROR: return "ERROR";
        case PG_LOG_FATAL: return "FATAL";
        default:           return "UNKNOWN";
    }
}

/* 获取当前时间字符串 */
static void get_timestamp(char *buffer, int size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* 初始化日志系统 */
int pg_logger_init(const pg_logger_config_t *config) {
    if (g_logger.initialized) {
        return 0;
    }

    if (config == NULL) {
        /* 默认配置 */
        g_logger.min_level = PG_LOG_INFO;
        g_logger.targets = PG_LOG_TARGET_CONSOLE;
        g_logger.show_timestamp = 1;
        g_logger.show_level = 1;
        g_logger.show_file_line = 1;
    } else {
        g_logger.min_level = config->min_level;
        g_logger.targets = config->targets;
        g_logger.show_timestamp = config->show_timestamp;
        g_logger.show_level = config->show_level;
        g_logger.show_file_line = config->show_file_line;

        if (config->log_file_path != NULL) {
            g_logger.log_file_path = strdup(config->log_file_path);
            g_logger.log_file = fopen(g_logger.log_file_path, "a");
            if (g_logger.log_file == NULL) {
                g_logger.targets &= ~PG_LOG_TARGET_FILE;
            }
        }

        if (config->callback != NULL) {
            g_logger.callback = config->callback;
            g_logger.callback_user_data = config->callback_user_data;
        }
    }

    g_logger.initialized = 1;
    return 0;
}

/* 清理日志系统 */
void pg_logger_cleanup(void) {
    if (!g_logger.initialized) {
        return;
    }

    if (g_logger.log_file != NULL) {
        fclose(g_logger.log_file);
        g_logger.log_file = NULL;
    }

    if (g_logger.log_file_path != NULL) {
        free(g_logger.log_file_path);
        g_logger.log_file_path = NULL;
    }

    g_logger.initialized = 0;
}

/* 设置日志级别 */
void pg_logger_set_level(pg_log_level_t level) {
    g_logger.min_level = level;
}

/* 设置日志文件 */
int pg_logger_set_file(const char *file_path) {
    if (file_path == NULL) {
        return -1;
    }

    if (g_logger.log_file != NULL) {
        fclose(g_logger.log_file);
        g_logger.log_file = NULL;
    }

    if (g_logger.log_file_path != NULL) {
        free(g_logger.log_file_path);
    }

    g_logger.log_file_path = strdup(file_path);
    if (g_logger.log_file_path == NULL) {
        return -1;
    }

    g_logger.log_file = fopen(g_logger.log_file_path, "a");
    if (g_logger.log_file == NULL) {
        free(g_logger.log_file_path);
        g_logger.log_file_path = NULL;
        return -1;
    }

    g_logger.targets |= PG_LOG_TARGET_FILE;
    return 0;
}

/* 设置日志回调 */
void pg_logger_set_callback(pg_log_callback_t callback, void *user_data) {
    g_logger.callback = callback;
    g_logger.callback_user_data = user_data;

    if (callback != NULL) {
        g_logger.targets |= PG_LOG_TARGET_CALLBACK;
    } else {
        g_logger.targets &= ~PG_LOG_TARGET_CALLBACK;
    }
}

/* 记录日志 */
void pg_logger_log(pg_log_level_t level, const char *file, int line, const char *format, ...) {
    if (!g_logger.initialized) {
        /* 自动初始化为默认配置 */
        pg_logger_init(NULL);
    }

    /* 过滤低级别日志 */
    if (level < g_logger.min_level) {
        return;
    }

    /* 格式化消息 */
    char message[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    /* 构建完整日志行 */
    char log_line[8192];
    int pos = 0;

    /* 时间戳 */
    if (g_logger.show_timestamp) {
        char timestamp[32];
        get_timestamp(timestamp, sizeof(timestamp));
        pos += snprintf(log_line + pos, sizeof(log_line) - pos, "[%s] ", timestamp);
    }

    /* 日志级别 */
    if (g_logger.show_level) {
        pos += snprintf(log_line + pos, sizeof(log_line) - pos, "[%-5s] ", pg_logger_level_name(level));
    }

    /* 文件和行号 */
    if (g_logger.show_file_line && file != NULL) {
        pos += snprintf(log_line + pos, sizeof(log_line) - pos, "(%s:%d) ", file, line);
    }

    /* 消息 */
    pos += snprintf(log_line + pos, sizeof(log_line) - pos, "%s\n", message);

    /* 输出到控制台 */
    if (g_logger.targets & PG_LOG_TARGET_CONSOLE) {
        FILE *out = (level >= PG_LOG_ERROR) ? stderr : stdout;
        fputs(log_line, out);
        fflush(out);
    }

    /* 输出到文件 */
    if ((g_logger.targets & PG_LOG_TARGET_FILE) && g_logger.log_file != NULL) {
        fputs(log_line, g_logger.log_file);
        fflush(g_logger.log_file);
    }

    /* 调用回调 */
    if ((g_logger.targets & PG_LOG_TARGET_CALLBACK) && g_logger.callback != NULL) {
        g_logger.callback(level, file, line, message, g_logger.callback_user_data);
    }
}

/* 慢查询日志 */
void pg_logger_slow_query(const char *sql, int64_t elapsed_ms, int threshold_ms) {
    if (elapsed_ms >= threshold_ms) {
        PG_LOG_WARN("[SLOW_QUERY] %lld ms (threshold=%d ms): %s", 
                    (long long)elapsed_ms, threshold_ms, sql ? sql : "NULL");
    }
}

/* 连接事件日志 */
void pg_logger_connection_event(const char *event, const char *conn_str, int success) {
    if (success) {
        PG_LOG_INFO("[CONNECTION] %s: success", event ? event : "unknown");
    } else {
        PG_LOG_ERROR("[CONNECTION] %s: failed - %s", event ? event : "unknown", conn_str ? conn_str : "NULL");
    }
}

/* 错误日志 */
void pg_logger_error(const char *operation, const char *error_msg) {
    PG_LOG_ERROR("[ERROR] %s: %s", operation ? operation : "unknown", error_msg ? error_msg : "NULL");
}
