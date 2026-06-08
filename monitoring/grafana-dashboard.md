# Grafana Dashboard 说明

本文档说明 `monitoring/grafana-dashboard.json` 的用途、导入方式、面板组成、依赖指标以及常见自定义方法。

## 文件用途

`monitoring/grafana-dashboard.json` 是项目的 Grafana 仪表盘定义文件，用于展示 `ruoyi-cpp` 的运行状态、数据库访问、缓存命中率、Redis 连接池以及业务侧用户活动等指标。

该文件是标准 Grafana dashboard JSON，可直接在 Grafana 中导入。

## 文件位置

- Dashboard 定义：`monitoring/grafana-dashboard.json`

## 前置条件

导入和使用该面板前，需要具备以下条件：

1. 已部署 Grafana。
2. 已配置 Prometheus 数据源。
3. 应用已暴露并上报相关 Prometheus 指标。
4. Grafana 中存在一个 Prometheus 数据源，并且其 UID 可以通过模板变量 `${datasource}` 选择。

如果指标未上报，面板会显示 `No data`，这是正常现象。

## 导入方式

### 方式一：Grafana UI 导入

1. 打开 Grafana。
2. 进入 `Dashboards`。
3. 点击 `Import`。
4. 选择或粘贴 `monitoring/grafana-dashboard.json` 内容。
5. 选择 Prometheus 数据源。
6. 完成导入。

### 方式二：作为 provisioned dashboard

如果你使用 Grafana provisioning，可以把该文件放入 dashboards 目录，并在 Grafana 的 provisioning 配置中声明它。

## Dashboard 总览

当前仪表盘主要分为 4 个区域：

1. `System Overview`
2. `Database`
3. `Cache`
4. `Business`

## 1. System Overview

该区域用于观察服务整体健康度与 HTTP 请求表现。

### Uptime

- 面板类型：`stat`
- 指标：`ruoyi_uptime_seconds`
- 含义：应用启动后已运行秒数。
- 用途：快速判断进程是否持续存活、是否刚刚重启。

### HTTP Request Rate

- 面板类型：`timeseries`
- 指标：`sum(rate(http_requests_total[5m])) by (status)`
- 含义：按 HTTP 状态码统计近 5 分钟请求速率。
- 用途：
  - 观察总体流量趋势
  - 快速发现 4xx/5xx 增长
  - 分析高峰时段请求量

### HTTP Latency

- 面板类型：`timeseries`
- 指标：
  - `histogram_quantile(0.50, sum(rate(http_request_duration_seconds_bucket[5m])) by (le))`
  - `histogram_quantile(0.95, sum(rate(http_request_duration_seconds_bucket[5m])) by (le))`
  - `histogram_quantile(0.99, sum(rate(http_request_duration_seconds_bucket[5m])) by (le))`
- 含义：展示 HTTP 请求延迟的 p50 / p95 / p99。
- 用途：
  - 判断系统响应是否退化
  - 分辨是少量慢请求还是整体变慢
  - 用于回归测试和压测观察

## 2. Database

该区域用于观察数据库访问频率与查询延迟。

### Database Query Rate

- 面板类型：`timeseries`
- 指标：`sum(rate(db_query_total[5m])) by (operation)`
- 含义：按操作类型统计数据库查询速率。
- 用途：
  - 识别读写压力
  - 观察不同操作类型的调用趋势
  - 判断是否存在异常放量查询

### Database Query Latency

- 面板类型：`timeseries`
- 指标：
  - `histogram_quantile(0.99, sum(rate(db_query_duration_seconds_bucket[5m])) by (le))`
  - `histogram_quantile(0.95, sum(rate(db_query_duration_seconds_bucket[5m])) by (le))`
- 含义：展示数据库查询耗时的高位分位数。
- 用途：
  - 发现慢 SQL 或数据库抖动
  - 辅助定位接口慢是业务逻辑还是数据库问题

## 3. Cache

该区域用于观察缓存系统使用情况。

### Cache Hit Rate

- 面板类型：`gauge`
- 指标：`(sum(cache_hits_total) / (sum(cache_hits_total) + sum(cache_misses_total))) * 100`
- 含义：缓存命中率百分比。
- ���值：
  - `< 70%` 红色
  - `70% - 90%` 黄色
  - `>= 90%` 绿色
- 用途：
  - 判断缓存是否发挥效果
  - 识别是否存在大量穿透或失效

### Redis Operations

- 面板类型：`timeseries`
- 指标：`sum(rate(redis_operations_total[5m])) by (operation)`
- 含义：按操作类型统计 Redis 操作速率。
- 用途：
  - 观察 Redis 使用模式
  - 识别短时间突增写入/读取

### Redis Connection Pool

- 面板类型：`timeseries`
- 指标：`redis_connection_pool_size`
- 含义：Redis 连接池大小或连接数。
- 用途：
  - 观察连接池是否过小或异常波动
  - 判断 Redis 资源配置是否合理

## 4. Business

该区域用于观察业务层面的用户行为与核心业务指标。

### User Activity

- 面板类型：`timeseries`
- 指标：
  - `online_users`
  - `sum(rate(user_login_total[5m])) by (status)`
- 含义：
  - 在线用户数
  - 按状态统计登录速率
- 用途：
  - 观察活跃用户趋势
  - 识别登录失败异常增加

## 面板依赖的主要 Prometheus 指标

根据当前 JSON，面板依赖以下指标：

- `ruoyi_uptime_seconds`
- `http_requests_total`
- `http_request_duration_seconds_bucket`
- `db_query_total`
- `db_query_duration_seconds_bucket`
- `cache_hits_total`
- `cache_misses_total`
- `redis_operations_total`
- `redis_connection_pool_size`
- `online_users`
- `user_login_total`

如果你要扩展该 dashboard，建议继续使用统一前缀和 histogram 命名规范，例如：

- 计数器：`*_total`
- 延迟分布：`*_duration_seconds_bucket`
- 即时状态值：如 `online_users`

## 适用场景

这个 dashboard 适合以下场景：

- 日常运行监控
- 发布后观测
- 压测/回归测试观测
- 故障排查时快速定位系统层、数据库层、缓存层问题

## 常见定制方式

### 1. 增加业务指标

你可以继续添加新的 timeseries 或 stat 面板，例如：

- 订单创建速率
- 文件上传成功率
- 插件加载次数
- 消息推送成功率

### 2. 增加错误率面板

建议补一个 5xx 错误率面板，例如：

```promql
sum(rate(http_requests_total{status=~"5.."}[5m]))
```

### 3. 增加实例维度

如果未来有多实例部署，可以在查询中增加 `instance`、`job`、`worker` 等标签维度，例如：

```promql
sum(rate(http_requests_total[5m])) by (instance, status)
```

### 4. 优化延迟观察窗口

当前多数查询使用 `[5m]` 窗口。
如果你更关注短时尖峰，可以改成 `[1m]`；如果更关注趋势稳定性，可以改成 `[10m]` 或 `[15m]`。

## 常见问题

### 导入后看不到数据

可能原因：

1. Prometheus 数据源没有选对。
2. 应用没有上报这些指标。
3. 指标名称与 dashboard 中写的不一致。
4. Prometheus 抓取目标失败。

### 延迟面板没有曲线

如果直方图 bucket 指标没有暴露，例如没有 `*_bucket`，则 `histogram_quantile(...)` 无法计算结果。

### 命中率显示异常

当 `cache_hits_total + cache_misses_total = 0` 时，表达式可能没有有效值。通常说明当前时间窗口内没有缓存访问。

## 维护建议

1. 每次新增核心监控指标时，同步评估是否需要补到该 dashboard。
2. 变更指标名时，同时更新 dashboard JSON。
3. 如果面板越来越多，建议按系统、数据库、缓存、业务、插件拆分为多个 dashboard。
4. 如果存在不同环境，建议通过变量区分 `job`、`instance`、`namespace` 或环境名。

## 总结

`monitoring/grafana-dashboard.json` 是 `ruoyi-cpp` 的基础监控看板定义，重点覆盖：

- 服务可用性
- HTTP 请求速率与延迟
- 数据库查询速率与耗时
- 缓存命中率与 Redis 操作
- 用户活跃度与登录情况

如果要让这个 dashboard 真正发挥作用，关键不在 Grafana 本身，而在于应用端是否稳定、持续、规范地暴露 Prometheus 指标。
