# ruoyi-cpp 可观测性手册

本文档说明 ruoyi-cpp 内置的 Prometheus 指标体系、推荐告警规则和 Grafana 面板配置。

---

## 1. 端点

| 端点 | 方法 | 说明 |
|------|------|------|
| `/actuator/health` | GET | 健康检查（k8s liveness/readiness）|
| `/actuator/info` | GET | 应用名/版本 |
| `/actuator/metrics` | GET | Prometheus 文本格式指标（核心）|
| `/actuator/db` | GET | 数据库后端状态（PG/SQLite/连接状态）|
| `/actuator/reload` | POST | 触发配置热重载（**仅 loopback** 127.0.0.1）|

示例：

```bash
curl http://localhost:18080/actuator/health
# {"status":"UP"}

curl http://localhost:18080/actuator/metrics
# ruoyi_requests_total{status="2xx"} 1234
# ruoyi_request_duration_ms_bucket{le="100"} 1100
# ...
```

---

## 2. 指标清单

### 2.1 HTTP 请求

| 指标 | 类型 | 说明 |
|------|------|------|
| `ruoyi_requests_total{status="2xx\|3xx\|4xx\|5xx"}` | counter | 按状态码分类的请求数 |
| `ruoyi_errors_total` | counter | 5xx 错误总数 |
| `ruoyi_slow_requests_total` | counter | 超过 `slowReqMs`（默认 1000ms）的请求数 |
| `ruoyi_request_duration_ms_bucket{le="..."}` | histogram | 请求耗时直方图（12 桶：5/10/25/50/100/250/500/1000/2500/5000/10000/30000ms）|
| `ruoyi_request_duration_ms_count` | counter | 总观测次数 |
| `ruoyi_request_duration_ms_sum` | counter | 总耗时（ms）|

### 2.2 数据库

| 指标 | 类型 | 说明 |
|------|------|------|
| `ruoyi_db_queries_total{op="query"}` | counter | SELECT 类查询次数 |
| `ruoyi_db_queries_total{op="exec"}` | counter | INSERT/UPDATE/DELETE/DDL 次数 |
| `ruoyi_db_slow_queries_total` | counter | 超过慢查询阈值（默认 200ms）|
| `ruoyi_db_errors_total` | counter | 数据库错误次数 |

### 2.3 认证 & 限流

| 指标 | 类型 | 说明 |
|------|------|------|
| `ruoyi_login_success_total` | counter | 登录成功次数 |
| `ruoyi_login_fail_total` | counter | 登录失败次数 |
| `ruoyi_rate_limit_rejected_total` | counter | 被 RateLimiter 拒绝的请求数 |

### 2.4 进程

| 指标 | 类型 | 说明 |
|------|------|------|
| `ruoyi_uptime_seconds` | gauge | 进程运行时间（秒）|

---

## 3. Prometheus 抓取配置

`prometheus.yml`：

```yaml
scrape_configs:
  - job_name: 'ruoyi-cpp'
    metrics_path: /actuator/metrics
    scrape_interval: 15s
    static_configs:
      - targets:
          - 'app1.example.com:18080'
          - 'app2.example.com:18080'
        labels:
          env: production
          service: ruoyi-cpp
```

多进程编排器（WorkerOrchestrator）每个 worker 监听不同端口时：

```yaml
    static_configs:
      - targets:
          - 'app1:18080'   # worker 0
          - 'app1:18081'   # worker 1
          - 'app1:18082'   # worker 2
          - 'app1:18083'   # worker 3
```

---

## 4. 常用 PromQL 查询

### 4.1 QPS（每秒请求数）

```promql
sum(rate(ruoyi_requests_total[1m]))
```

按状态码拆分：

```promql
sum by (status) (rate(ruoyi_requests_total[1m]))
```

### 4.2 错误率

```promql
sum(rate(ruoyi_requests_total{status="5xx"}[5m]))
  /
sum(rate(ruoyi_requests_total[5m]))
```

### 4.3 请求耗时 P50 / P95 / P99

```promql
histogram_quantile(0.50, sum by (le) (rate(ruoyi_request_duration_ms_bucket[5m])))
histogram_quantile(0.95, sum by (le) (rate(ruoyi_request_duration_ms_bucket[5m])))
histogram_quantile(0.99, sum by (le) (rate(ruoyi_request_duration_ms_bucket[5m])))
```

### 4.4 平均耗时

```promql
rate(ruoyi_request_duration_ms_sum[5m])
  /
rate(ruoyi_request_duration_ms_count[5m])
```

### 4.5 慢请求占比

```promql
rate(ruoyi_slow_requests_total[5m])
  /
rate(ruoyi_request_duration_ms_count[5m])
```

### 4.6 数据库 QPS

```promql
sum by (op) (rate(ruoyi_db_queries_total[1m]))
```

### 4.7 登录失败率（暴破检测）

```promql
rate(ruoyi_login_fail_total[5m])
```

短时间内骤增表示可能的暴破攻击。

### 4.8 限流命中率

```promql
rate(ruoyi_rate_limit_rejected_total[5m])
  /
rate(ruoyi_requests_total[5m])
```

---

## 5. AlertManager 告警规则

`rules/ruoyi-cpp.yml`：

```yaml
groups:
  - name: ruoyi-cpp.rules
    rules:
      # 错误率高
      - alert: HighErrorRate
        expr: |
          sum(rate(ruoyi_requests_total{status="5xx"}[5m]))
            / sum(rate(ruoyi_requests_total[5m])) > 0.05
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "5xx 错误率超过 5%"
          description: "{{ $value | humanizePercentage }} 错误率，需检查应用日志"

      # P95 延迟过高
      - alert: HighLatencyP95
        expr: |
          histogram_quantile(0.95,
            sum by (le) (rate(ruoyi_request_duration_ms_bucket[5m]))
          ) > 1000
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "P95 请求耗时 > 1s"
          description: "P95={{ $value }}ms，考虑扩容或排查慢查询"

      # 数据库错误
      - alert: DatabaseErrors
        expr: rate(ruoyi_db_errors_total[5m]) > 0.1
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "DB 错误率 > 0.1/s"
          description: "DB 错误数：{{ $value }}/s，检查 PG 连接或熔断状态"

      # 登录失败激增（暴破攻击）
      - alert: LoginBruteForce
        expr: rate(ruoyi_login_fail_total[1m]) > 5
        for: 2m
        labels:
          severity: warning
        annotations:
          summary: "登录失败率 > 5/s 持续 2 分钟"
          description: "可能存在暴破攻击，检查 sys_logininfor"

      # 限流触发频繁
      - alert: RateLimitHit
        expr: rate(ruoyi_rate_limit_rejected_total[5m]) > 1
        for: 5m
        labels:
          severity: info
        annotations:
          summary: "限流命中率 > 1/s"
          description: "检查是否真实流量或攻击；必要时调整阈值"

      # 进程重启检测
      - alert: ProcessRestarted
        expr: ruoyi_uptime_seconds < 60
        for: 1m
        labels:
          severity: warning
        annotations:
          summary: "进程在 1 分钟内重启"
          description: "uptime={{ $value }}s，检查崩溃日志 ./logs"

      # 端点不可达
      - alert: ServiceDown
        expr: up{job="ruoyi-cpp"} == 0
        for: 2m
        labels:
          severity: critical
        annotations:
          summary: "服务不可达"
          description: "{{ $labels.instance }} 抓取失败 2 分钟"
```

---

## 6. Grafana 面板

### 6.1 推荐面板布局

| 行 | Panel | Query | 图表类型 |
|---|-------|-------|---------|
| 1 | QPS | `sum(rate(ruoyi_requests_total[1m]))` | Stat |
| 1 | Error Rate | 错误率 PromQL（见 4.2）| Stat (red 阈值 5%) |
| 1 | Uptime | `ruoyi_uptime_seconds` | Stat (单位 s → h) |
| 2 | Status 分布 | `sum by (status) (rate(ruoyi_requests_total[1m]))` | Time series stacked |
| 2 | P50/P95/P99 耗时 | 见 4.3 | Time series multi-line |
| 3 | DB QPS | `sum by (op) (rate(ruoyi_db_queries_total[1m]))` | Time series |
| 3 | DB 慢查询 | `rate(ruoyi_db_slow_queries_total[5m])` | Time series |
| 3 | DB 错误 | `rate(ruoyi_db_errors_total[5m])` | Time series |
| 4 | 登录成功/失败 | `rate(ruoyi_login_*_total[5m])` | Time series |
| 4 | 限流拒绝 | `rate(ruoyi_rate_limit_rejected_total[5m])` | Time series |

### 6.2 导入步骤

1. Grafana → Dashboards → New → Import
2. 选择 Prometheus 数据源
3. 按上表逐个建 Panel（或导出已有 dashboard JSON 复用）

未来可考虑发布 `docs/grafana-dashboard.json` 一键导入。

---

## 7. 慢请求与慢查询日志

除指标外，运行时还会输出文本日志（NDJSON 格式，见 `JsonLogger`）：

### 慢请求（>= `slowReqMs`，默认 1000ms）

```json
{"ts":"...","level":"WARN","msg":"[SlowReq] GET /api/system/user/list status=200 duration=1234ms"}
```

### 慢查询（>= `slowQueryWarnMs`，默认 200ms）

```text
[SlowSQL][WARN][queryParams] 350ms SQL: SELECT ... FROM sys_user WHERE ...
[SlowSQL][ERR][query] 1500ms SQL: ...   ← 超过 slowQueryErrMs (默认 1000ms)
```

可通过 config.json 调整阈值：

```json
{
  "database": {
    "slow_query_warn_ms": 200,
    "slow_query_err_ms":  1000
  }
}
```

---

## 8. 与日志/审计的关系

| 维度 | 工具 | 用途 |
|------|------|------|
| **指标** | Prometheus `/actuator/metrics` | 聚合趋势、容量规划、告警 |
| **日志** | `JsonLogger` NDJSON 文件 | 单次请求调试、错误堆栈、链路追踪 |
| **审计** | `sys_oper_log` 表（DB）| 合规审计、用户操作追溯 |
| **登录日志** | `sys_logininfor` 表（DB）| 登录历史、IP 地理定位 |

三者互补：指标看"健康度"，日志看"发生了什么"，审计看"谁做了什么"。

---

## 9. 安全注意

- `/actuator/*` 默认开放给所有访问。**生产环境强烈建议**：
  - 用 nginx/反代限制 `/actuator/*` 仅内网可访问
  - 或在 ruoyi-cpp `nginx_like.ip_acl` 配置允许 IP
- `/actuator/reload` 已硬编码仅 loopback (127.0.0.1) 可调用

示例 nginx 配置：

```nginx
location /actuator/ {
    allow 10.0.0.0/8;
    allow 192.168.0.0/16;
    deny all;
    proxy_pass http://ruoyi-cpp-backend;
}
```

---

## 10. 调优建议

| 现象 | 可能原因 | 排查 |
|------|---------|------|
| `db_slow_queries_total` 持续上升 | 缺索引 / 大表全扫 / 锁等待 | 查 SlowSQL 日志，`EXPLAIN ANALYZE` |
| P99 远高于 P95 | 长尾请求（GC/锁/慢 IO）| 看慢请求日志单条 |
| `login_fail_total` 突增 | 暴破攻击 | 查 sys_logininfor、查 IP，必要时封禁 |
| `rate_limit_rejected_total` 高 | 真实流量增 / 阈值过严 | 调高 `RateLimiter` 阈值 |
| `uptime_seconds` 频繁归零 | 崩溃重启 | 看 `./logs/crash-*.log` |

---

## 11. 参考

- [Prometheus Best Practices](https://prometheus.io/docs/practices/naming/)
- [RED Method](https://www.weave.works/blog/the-red-method-key-metrics-for-microservices-architecture/)
- [USE Method](http://www.brendangregg.com/usemethod.html)
- 项目源码：`src/common/MetricsCollector.h`
