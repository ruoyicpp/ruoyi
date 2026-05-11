# Drogon PG Driver (C)

基于官方 libpq 的 PostgreSQL 数据库驱动（C 语言版本）。

## 架构

```
业务代码
    ↓
PgClient 统一门面
    ↓
路由分发
    ├── 读 → PgPool → PgConnection
    ├── 写 → AsyncWriteQueue → WriterConn
    └── 事务 → PgPool.checkoutExclusive() → 独占连接
    ↓
Circuit Breaker 模式选择器 (CLOSED/OPEN/HALF_OPEN)
    ├── CLOSED → 主连接池
    ├── OPEN → FallbackPool (2~3连接 mini-pool)
    └── HALF_OPEN → 试探恢复
    ↓
官方 libpq
    ↓
PostgreSQL Server
```

## 特性

- **读写分流**：SELECT/SHOW → 主连接池，INSERT/UPDATE → 异步写队列
- **连接池管理**：主连接池（3~24连接）+ 降级连接池（2~3连接）
- **Circuit Breaker**：自动降级/恢复机制
- **预编译语句缓存**：LRU 128 条/连接
- **类型转换**：OID → C 类型映射
- **健康监控**：心跳检测、指数退避重连、慢查询日志
- **异步写队列**：ring buffer 50,000 槽位，batch 200 条/事务
- **线程安全**：互斥锁保护关键操作
- **SQL 注入防护**：参数验证和转义
- **批量插入**：COPY 和多值 INSERT 两种方式
- **连接池预热**：启动时预创建连接

## 构建

```bash
mkdir build
cd build
cmake ..
cmake --build .
ctest --output-on-failure  # 运行测试
```

## 使用示例

### 基本配置

```c
#include "pg_config.h"
#include "pg_client.h"

int main(void) {
    pg_config_t *config = pg_config_get_instance();

    // 设置连接字符串
    pg_config_set_connection_string(config, "host=localhost port=5432 dbname=test user=postgres");

    // 设置主连接池配置
    pg_config_set_pool_min_size(config, 3);
    pg_config_set_pool_init_size(config, 8);
    pg_config_set_pool_max_size(config, 24);

    // 设置降级连接池配置
    pg_config_set_fallback_pool_min_size(config, 2);
    pg_config_set_fallback_pool_init_size(config, 2);
    pg_config_set_fallback_pool_max_size(config, 3);

    // 设置 Circuit Breaker
    pg_config_set_failure_threshold(config, 5);
    pg_config_set_recovery_threshold(config, 10);

    // 设置健康监控
    pg_config_set_health_check_interval_sec(config, 30);
    pg_config_set_slow_query_threshold_ms(config, 100);

    // 设置异步写队列
    pg_config_set_async_write_queue_size(config, 50000);
    pg_config_set_async_write_batch_size(config, 200);

    // 初始化客户端
    pg_client_init();

    // 预热连接池
    pg_client_warmup_pool();

    // ... 业务代码 ...

    // 清理
    pg_client_cleanup();
    pg_config_cleanup(config);

    return 0;
}
```

### 执行查询

```c
#include "pg_client.h"
#include "pg_result.h"

// 简单查询
pg_result_t *result = pg_client_exec("SELECT * FROM users WHERE id = 1", NULL, 0);
if (result != NULL && !pg_result_is_error(result)) {
    // 处理结果
    int rows = pg_result_row_count(result);
    for (int i = 0; i < rows; i++) {
        const char *name = pg_result_get_value(result, i, 0);
        printf("Name: %s\n", name);
    }
    pg_result_destroy(result);
}

// 参数化查询（防止 SQL 注入）
const char *params[] = {"John", "25"};
pg_result_t *result = pg_client_exec(
    "SELECT * FROM users WHERE name = $1 AND age = $2",
    params, 2
);
```

### 事务操作

```c
#include "pg_transaction.h"

// 创建事务（使用独占连接）
pg_transaction_t *txn = pg_transaction_create_with_pool(pool);

// 开始事务
if (pg_transaction_begin(txn) == 0) {
    // 执行操作（都在同一个连接上）
    pg_transaction_exec(txn, "INSERT INTO orders (user_id) VALUES (1)", NULL, 0);
    pg_transaction_exec(txn, "UPDATE users SET order_count = order_count + 1 WHERE id = 1", NULL, 0);
    
    // 提交或回滚
    if (/* 成功 */) {
        pg_transaction_commit(txn);
    } else {
        pg_transaction_rollback(txn);
    }
}

// 销毁事务（释放独占连接）
pg_transaction_destroy(txn);
```

### 批量插入

```c
#include "pg_client.h"

// 使用 COPY 命令（最快）
const char *rows[] = {
    "1,John,25",
    "2,Jane,30",
    "3,Bob,35"
};
int inserted = pg_client_bulk_insert("users", "id,name,age", rows, 3);

// 使用多值 INSERT（更灵活）
const char *rows2[] = {
    "'Alice', 28",
    "'Charlie', 32"
};
int inserted2 = pg_client_batch_insert("users", "name,age", rows2, 2, 100);
```

### 异步查询

```c
#include "pg_async_client.h"

// 定义回调函数
void on_result(pg_result_t *result, void *user_data) {
    if (!pg_result_is_error(result)) {
        int count = pg_result_row_count(result);
        printf("Got %d rows\n", count);
    }
    pg_result_destroy(result);
}

// 异步执行
pg_async_client_exec_async("SELECT * FROM users", NULL, 0, on_result, NULL);

// 在事件循环中轮询
void on_socket_readable(int fd) {
    pg_async_client_handle_read(fd);
}

void on_socket_writable(int fd) {
    pg_async_client_handle_write(fd);
}
```

### SQL 注入防护

```c
#include "pg_client.h"

// 转义字符串参数
char *escaped = pg_client_escape_string("O'Brien");
// 结果: 'O''Brien'

// 转义标识符（表名、列名）
char *ident = pg_client_escape_identifier("user-table");
// 结果: "user-table"

// 验证 SQL
if (pg_client_validate_sql(sql) != 0) {
    printf("Invalid SQL\n");
}

// 验证参数
const char *params[] = {"value1", "value2"};
if (pg_client_validate_params(params, 2) != 0) {
    printf("Invalid parameters\n");
}
```

## 文件结构

```
drogon_pg_driver/
├── include/
│   ├── pg_config.h           # 配置管理
│   ├── pg_error.h            # 错误处理
│   ├── pg_connection.h       # 连接封装
│   ├── pg_pool.h             # 连接池
│   ├── pg_pool_monitor.h     # 连接池监控
│   ├── pg_pool_wait_queue.h  # 等待队列
│   ├── pg_result.h           # 结果封装
│   ├── pg_client.h           # 统一门面
│   ├── pg_transaction.h      # 事务封装
│   ├── pg_circuit_breaker.h  # 熔断器
│   ├── pg_health_monitor.h   # 健康监控
│   ├── pg_async_client.h     # 异步客户端
│   ├── pg_async_connection.h # 异步连接
│   ├── pg_async_write_queue.h # 异步写队列
│   ├── pg_writer_connection.h # Writer 连接
│   ├── pg_sql_parser.h       # SQL 解析器
│   ├── pg_reconnect_backoff.h # 重连退避
│   ├── pg_mutex.h            # 互斥锁
│   ├── pg_logger.h           # 日志
│   └── pg_type_codec.h       # 类型转换
├── src/
│   └── ...                   # 实现文件
├── tests/
│   ├── test_config.c         # 配置测试
│   └── test_error.c          # 错误测试
├── CMakeLists.txt            # CMake 构建配置
└── README.md                 # 本文件
```

## 依赖

- PostgreSQL libpq（官方）
- C11 标准库
- CMake 3.14+

## 配置参数

### 主连接池

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `pool_min_size` | 3 | 最小连接数 |
| `pool_init_size` | 8 | 初始连接数 |
| `pool_max_size` | 24 | 最大连接数 |
| `pool_checkout_timeout_ms` | 5000 | 获取连接超时(ms) |

### 降级连接池

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `fallback_pool_min_size` | 2 | 最小连接数 |
| `fallback_pool_init_size` | 2 | 初始连接数 |
| `fallback_pool_max_size` | 3 | 最大连接数 |

### Circuit Breaker

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `failure_threshold` | 5 | 连续失败次数触发降级 |
| `recovery_threshold` | 10 | 连续成功次数恢复 |

### 健康监控

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `health_check_interval_sec` | 30 | 心跳检测间隔(s) |
| `slow_query_threshold_ms` | 100 | 慢查询阈值(ms) |
| `reconnect_backoff_max_sec` | 60 | 重连退避上限(s) |

### 异步写队列

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `async_write_queue_size` | 50000 | 队列大小 |
| `async_write_batch_size` | 200 | 批量大小 |

## API 参考

### PgClient

```c
// 初始化/清理
int pg_client_init(void);
void pg_client_cleanup(void);

// 执行查询
pg_result_t* pg_client_exec(const char *sql, const char **params, int param_count);
pg_result_t* pg_client_exec_params(const char *sql, const char **params, const char **param_types, int param_count);

// 事务
int pg_client_begin_transaction(void);
int pg_client_commit_transaction(void);
int pg_client_rollback_transaction(void);

// 批量插入
int pg_client_bulk_insert(const char *table, const char *columns, const char **rows, int row_count);
int pg_client_batch_insert(const char *table, const char *columns, const char **rows, int row_count, int batch_size);

// SQL 注入防护
char* pg_client_escape_string(const char *input);
char* pg_client_escape_identifier(const char *input);
int pg_client_validate_params(const char **params, int param_count);
int pg_client_validate_sql(const char *sql);

// 连接池状态
int pg_client_warmup_pool(void);
int pg_client_available_connections(void);
pg_pool_stats_t pg_client_get_pool_stats(void);
int pg_client_check_leaks(void);
```

### PgPool

```c
// 创建/销毁
pg_pool_t* pg_pool_create(const char *conn_str, int min_size, int max_size, int timeout_ms, int is_fallback);
void pg_pool_destroy(pg_pool_t *pool);

// 获取/归还连接
pg_connection_t* pg_pool_checkout(pg_pool_t *pool);
pg_connection_t* pg_pool_checkout_timeout(pg_pool_t *pool, int timeout_ms);
pg_connection_t* pg_pool_checkout_exclusive(pg_pool_t *pool);
void pg_pool_checkin(pg_pool_t *pool, pg_connection_t *conn);
void pg_pool_checkin_exclusive(pg_pool_t *pool, pg_connection_t *conn);

// 状态
int pg_pool_size(const pg_pool_t *pool);
int pg_pool_available_count(const pg_pool_t *pool);
int pg_pool_in_use_count(const pg_pool_t *pool);
int pg_pool_is_full(const pg_pool_t *pool);
int pg_pool_is_empty(const pg_pool_t *pool);
int pg_pool_warmup(pg_pool_t *pool);
```

### PgTransaction

```c
// 创建/销毁
pg_transaction_t* pg_transaction_create(void);
pg_transaction_t* pg_transaction_create_with_pool(pg_pool_t *pool);
void pg_transaction_destroy(pg_transaction_t *txn);

// 事务操作
int pg_transaction_begin(pg_transaction_t *txn);
int pg_transaction_commit(pg_transaction_t *txn);
int pg_transaction_rollback(pg_transaction_t *txn);
pg_result_t* pg_transaction_exec(pg_transaction_t *txn, const char *sql, const char **params, int param_count);

// 保存点
int pg_transaction_savepoint(pg_transaction_t *txn, const char *name);
int pg_transaction_rollback_to_savepoint(pg_transaction_t *txn, const char *name);
int pg_transaction_release_savepoint(pg_transaction_t *txn, const char *name);
```

### PgHealthMonitor

```c
// 创建/销毁
pg_health_monitor_t* pg_health_monitor_create(void);
void pg_health_monitor_destroy(pg_health_monitor_t *monitor);

// 控制
int pg_health_monitor_start(pg_health_monitor_t *monitor);
void pg_health_monitor_stop(pg_health_monitor_t *monitor);
int pg_health_monitor_check(pg_health_monitor_t *monitor);

// 配置
void pg_health_monitor_set_check_interval(pg_health_monitor_t *monitor, int interval_sec);
void pg_health_monitor_set_slow_query_threshold(pg_health_monitor_t *monitor, int threshold_ms);

// 统计
int pg_health_monitor_get_healthy_connections(const pg_health_monitor_t *monitor);
int pg_health_monitor_get_failed_connections(const pg_health_monitor_t *monitor);
int pg_health_monitor_get_slow_queries(const pg_health_monitor_t *monitor);
int pg_health_monitor_get_consecutive_failures(const pg_health_monitor_t *monitor);
```

## 错误代码

| 代码 | 说明 |
|------|------|
| `PG_ERROR_SUCCESS` | 成功 |
| `PG_ERROR_CONNECTION_FAILED` | 连接失败 |
| `PG_ERROR_CONNECTION_TIMEOUT` | 连接超时 |
| `PG_ERROR_QUERY_FAILED` | 查询失败 |
| `PG_ERROR_PARAMETER_ERROR` | 参数错误 |
| `PG_ERROR_TYPE_CONVERSION_ERROR` | 类型转换错误 |
| `PG_ERROR_TRANSACTION_ERROR` | 事务错误 |
| `PG_ERROR_POOL_EXHAUSTED` | 连接池耗尽 |
| `PG_ERROR_POOL_TIMEOUT` | 连接池超时 |
| `PG_ERROR_STATEMENT_CACHE_ERROR` | 语句缓存错误 |
| `PG_ERROR_CIRCUIT_BREAKER_OPEN` | 熔断器开启 |
| `PG_ERROR_HEALTH_CHECK_FAILED` | 健康检查失败 |
| `PG_ERROR_UNKNOWN_ERROR` | 未知错误 |

## 线程安全

- `pg_client_exec` 等函数使用互斥锁保护，可在多线程环境使用
- 每个 `pg_transaction_t` 实例使用独占连接，不同事务互不干扰
- 建议每个线程使用独立的事务实例

## 性能优化建议

1. **连接池预热**：启动时调用 `pg_client_warmup_pool()` 预创建连接
2. **批量插入**：大量数据使用 `pg_client_bulk_insert()` (COPY) 或 `pg_client_batch_insert()` (多值 INSERT)
3. **参数化查询**：使用参数化查询避免 SQL 解析开销和注入风险
4. **异步写入**：写操作自动路由到异步写队列，提高吞吐量
5. **连接池大小**：根据并发量调整 `pool_max_size`，通常为 CPU 核心数 * 2 + 有效磁盘数

## 许可证

MIT License
