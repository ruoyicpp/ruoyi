# 日志聚合分析系统（第二阶段已实现，第三阶段已启动）

## 当前状态

`src/log` 目录已经完成前两阶段落地，并已开始第三阶段的本地规则化预警实现。目前模块具备可编译、可接入主工程、可通过 HTTP API 对本地日志进行聚合检索、分析与预警摘要输出的能力。

当前已实现：

- `LogCollector`：支持主目录、额外目录、命名来源的采集状态汇总
- `LogAnalyzer`：支持基础统计、趋势分析、热点分析、异常摘要、预设查询、本地规则化预警摘要
- `LogSearchEngine`：支持本地日志文件扫描、结构化字段提取、按条件过滤
- `LogQuery`：已实现查询、结果、热点、异常、分析、预警等领域模型
- `LogCtrl`：已实现日志监控 API，并暴露第二阶段分析结果与第三阶段预警结果
- `config.json` 中 `log.path`、`paths`、`sources`、`max_files`、`max_results`、`elasticsearch`、`kibana` 配置解析
- 主工程编译接入：已启用 `src/log/*.cc`

当前仍未实现：

- 真正的 Elasticsearch 写入与查询
- Logstash 传输链路
- Kibana 仪表盘自动配置
- 持久化告警中心
- 规则存储、静默、通知发送

## 第一、二阶段已实现能力

当前模块已经支持：

- 从 `log.path` 读取默认日志目录
- 未配置时回退到 `./logs`
- 扫描 `.log` / `.txt` / `.jsonl` 文件
- 额外目录 `paths` 扫描
- 命名来源 `sources` 扫描
- 关键词过滤
- 级别过滤
- 文件名/来源过滤
- 时间范围过滤
- 时间戳提取
- 日志级别识别
- `thread` / `logger` 等括号字段提取
- 基础统计与趋势分析
- 热点文件统计
- 热点消息统计
- 异常摘要分析
- JSON API 返回搜索与分析结果

## 第三阶段当前已实现能力

当前已落地的第三阶段能力：

- 基于分析结果生成本地告警摘要
- 支持高错误率预警
- 支持错误突增预警
- 支持未知级别过多预警
- 支持解析失败过多预警
- 支持重复热点消息预警
- 通过独立 API 输出告警列表

## 已提供的 API

当前模块已提供以下接口：

- `GET /monitor/logs/search`
- `GET /monitor/logs/stats`
- `GET /monitor/logs/trends`
- `GET /monitor/logs/analysis`
- `GET /monitor/logs/collector`
- `GET /monitor/logs/hot-files`
- `GET /monitor/logs/hot-messages`
- `GET /monitor/logs/anomalies`
- `GET /monitor/logs/alerts`
- `GET /monitor/logs/errors`
- `GET /monitor/logs/warnings`
- `GET /monitor/logs/performance`

## 当前设计说明

前三阶段当前实现优先保证：

- 可编译
- 可运行
- 可接入主工程
- 可在不依赖外部 ELK 的前提下独立工作
- 为后续规则预警增强与外部系统集成留好扩展点

因此当前搜索与分析体系采用的是基于 `std::filesystem` + 行扫描 + 轻量结构化解析 + 本地规则评估的实现，而不是倒排索引、外部搜索引擎或独立告警平台。

## 配置示例

```json
{
  "log": {
    "enabled": true,
    "path": "./logs",
    "paths": [
      "./logs/app",
      "./logs/jobs"
    ],
    "sources": [
      {
        "name": "gateway",
        "path": "./logs/gateway",
        "enabled": true
      },
      {
        "name": "worker",
        "path": "./logs/worker",
        "enabled": true
      }
    ],
    "max_files": 20,
    "max_results": 500,
    "alerts": {
      "high_error_rate": 0.20,
      "critical_error_rate": 0.40,
      "error_spike_threshold": 5,
      "critical_error_spike_count": 3,
      "unknown_level_ratio": 0.10,
      "warning_unknown_level_ratio": 0.25,
      "parse_failure_ratio": 0.10,
      "warning_parse_failure_ratio": 0.30,
      "repeated_message_count": 20,
      "repeated_message_ratio": 0.15,
      "warning_repeated_message_ratio": 0.30
    },
    "elasticsearch": {
      "enabled": false,
      "host": "127.0.0.1",
      "port": 9200,
      "index_prefix": "ruoyi-logs"
    },
    "kibana": {
      "enabled": false,
      "host": "127.0.0.1",
      "port": 5601
    }
  }
}
```

说明：

- `path` 是默认日志目录
- `paths` 是额外目录列表
- `sources` 用于声明具名日志来源
- `alerts` 用于覆盖本地预警规则阈值
- 如果 `path` 为空，则自动回退到 `./logs`
- `elasticsearch` / `kibana` 目前仍是配置预留，不会真正联网使用

## 第三阶段实施说明

第三阶段当前先实现“规则化预警”本地闭环，而不是直接跳到 ELK 接入。

### 当前规则示例

当前默认会评估以下规则：

- `high_error_rate`：错误率超过阈值
- `error_spike`：5 分钟窗口内存在错误突增
- `unknown_level_ratio`：未知级别日志占比过高
- `parse_failure_ratio`：时间戳解析失败占比过高
- `repeated_hot_message`：单条热点消息重复过多

### `GET /monitor/logs/alerts` 返回内容

该接口当前会返回：

- `hasAlerts`：当前是否命中告警
- `totalAlerts`：命中告警数量
- `alerts`：告警列表

每条告警包含：

- `ruleId`
- `level`
- `title`
- `summary`
- `value`
- `threshold`

## 后续阶段建议

在本地规则化预警稳定后，再继续接入：

- Elasticsearch
- Logstash
- Kibana
- 通知通道与告警中心
- 规则持久化与静默策略

## 相关说明

- 当前 README 反映的是模块最新真实状态
- 第一阶段、第二阶段都已经是实际代码，不再只是规划
- 第三阶段已经开始实现本地规则预警
- 后续若继续推进，应同步更新 README 与配置说明
