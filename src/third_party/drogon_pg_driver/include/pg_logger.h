#ifndef PG_LOGGER_H
#define PG_LOGGER_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 日志级别 */
typedef enum {
    PG_LOG_TRACE,
    PG_LOG_DEBUG,
    PG_LOG_INFO,
    PG_LOG_WARN,
    PG_LOG_ERROR,
    PG_LOG_FATAL
} pg_log_level_t;

/* 日志输出目标 */
typedef enum {
    PG_LOG_TARGET_NONE = 0,
    PG_LOG_TARGET_CONSOLE = 1,
    PG_LOG_TARGET_FILE = 2,
    PG_LOG_TARGET_CALLBACK = 4
} pg_log_target_t;

/* 日志回调函数类型 */
typedef void (*pg_log_callback_t)(pg_log_level_t level, const char *file, int line, const char *message, void *user_data);

/* 日志配置 */
typedef struct pg_logger_config {
    pg_log_level_t min_level;
    int targets;
    char *log_file_path;
    pg_log_callback_t callback;
    void *callback_user_data;
    int show_timestamp;
    int show_file_line;
    int show_level;
} pg_logger_config_t;

/* 初始化日志系统 */
int pg_logger_init(const pg_logger_config_t *config);

/* 清理日志系统 */
void pg_logger_cleanup(void);

/* 设置日志级别 */
void pg_logger_set_level(pg_log_level_t level);

/* 设置日志文件 */
int pg_logger_set_file(const char *file_path);

/* 设置日志回调 */
void pg_logger_set_callback(pg_log_callback_t callback, void *user_data);

/* 记录日志 */
void pg_logger_log(pg_log_level_t level, const char *file, int line, const char *format, ...);

/* 便捷宏 */
#define PG_LOG_TRACE(...) pg_logger_log(PG_LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define PG_LOG_DEBUG(...) pg_logger_log(PG_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define PG_LOG_INFO(...)  pg_logger_log(PG_LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define PG_LOG_WARN(...)  pg_logger_log(PG_LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define PG_LOG_ERROR(...) pg_logger_log(PG_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define PG_LOG_FATAL(...) pg_logger_log(PG_LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

/* 慢查询日志 */
void pg_logger_slow_query(const char *sql, int64_t elapsed_ms, int threshold_ms);

/* 连接事件日志 */
void pg_logger_connection_event(const char *event, const char *conn_str, int success);

/* 错误日志 */
void pg_logger_error(const char *operation, const char *error_msg);

/* 获取日志级别名称 */
const char* pg_logger_level_name(pg_log_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* PG_LOGGER_H */
