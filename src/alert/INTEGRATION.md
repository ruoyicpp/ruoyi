# 性能告警系统 - 集成指南

## 📋 集成步骤

### 1. 更新主 CMakeLists.txt

在 `G:\back\recovered\ruoyi-cpp\CMakeLists.txt` 中，找到源文件列表部分，添加告警系统源文件：

```cmake
# ── 源文件 ────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    src/main.cc
    src/common/*.cc
    src/services/*.cc
    src/system/services/*.cc
    src/system/controllers/*.cc
    src/ai/controllers/*.cc
    src/filters/*.cc
    src/libs/plugin/*.cc
    src/taskqueue/*.cc
    src/alert/*.cc          # 新增：告警系统
)
```

### 2. 更新 AppIncludes.h

在 `src/AppIncludes.h` 中添加告警系统的包含：

```cpp
// ── 性能告警系统 ────────────────────────────────────────────
#include "alert/AlertEngine.h"
#include "alert/AlertAggregator.h"
#include "alert/AlertNotifier.h"
#include "alert/AlertCtrl.h"
```

### 3. 在 main.cc 中初始化

在 `src/main.cc` 的初始化部分添加告警系统初始化代码：

```cpp
// ── 初始化告警系统 ──────────────────────────────────────────
{
    // 初始化告警引擎
    AlertEngine::instance().init(config["alert"]);
    
    // 初始化告警聚合器
    AlertAggregator::instance().init(300);  // 300 秒聚合窗口
    
    // 初始化告警通知器
    AlertNotifier::instance().init(config["alert"]);
    
    // 启动告警引擎
    AlertEngine::instance().start();
    
    LOG_INFO << "[Alert] Alert system initialized and started";
}
```

### 4. 配置 config.json

在 `config.json` 中添加告警系统配置：

```json
{
  "alert": {
    "enabled": true,
    "checkInterval": 5000,
    "maxAlerts": 10000,
    "alertRetention": 86400,
    "rules": [
      {
        "id": "cpu_high",
        "name": "CPU 使用率过高",
        "type": 0,
        "severity": 2,
        "metric": "cpu_usage",
        "operator": 2,
        "threshold": 80,
        "duration": 300,
        "enabled": true,
        "maxAlerts": 10,
        "silenceDuration": 300,
        "notifyChannels": ["email"],
        "notifyReceivers": ["admin@example.com"]
      },
      {
        "id": "memory_high",
        "name": "内存使用率过高",
        "type": 0,
        "severity": 2,
        "metric": "memory_usage",
        "operator": 2,
        "threshold": 85,
        "duration": 300,
        "enabled": true,
        "maxAlerts": 10,
        "silenceDuration": 300,
        "notifyChannels": ["email"],
        "notifyReceivers": ["admin@example.com"]
      }
    ],
    "smtp": {
      "host": "smtp.qq.com",
      "port": 465,
      "user": "your-email@qq.com",
      "password": "your-auth-code"
    },
    "webhooks": {
      "dingtalk": "https://oapi.dingtalk.com/robot/send?access_token=xxx",
      "wecom": "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=xxx"
    }
  }
}
```

## 🚀 使用示例

### 1. 报告指标

在业务代码中报告监控指标：

```cpp
#include "alert/AlertEngine.h"

// 报告 CPU 使用率
double cpuUsage = getSystemCpuUsage();
AlertEngine::instance().reportMetric("cpu_usage", cpuUsage);

// 报告内存使用率
double memUsage = getSystemMemoryUsage();
AlertEngine::instance().reportMetric("memory_usage", memUsage);

// 报告磁盘使用率
double diskUsage = getSystemDiskUsage();
AlertEngine::instance().reportMetric("disk_usage", diskUsage);
```

### 2. 创建告警规则

通过 API 创建告警规则：

```bash
curl -X POST http://localhost:8080/monitor/alert/rules \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <token>" \
  -d '{
    "id": "request_latency_high",
    "name": "请求延迟过高",
    "type": 0,
    "severity": 1,
    "metric": "request_latency",
    "operator": 2,
    "threshold": 1000,
    "duration": 60,
    "enabled": true,
    "maxAlerts": 10,
    "silenceDuration": 300,
    "notifyChannels": ["email", "dingtalk"],
    "notifyReceivers": ["admin@example.com"]
  }'
```

### 3. 查询告警

```bash
# 获取所有告警规则
curl http://localhost:8080/monitor/alert/rules \
  -H "Authorization: Bearer <token>"

# 获取所有告警
curl http://localhost:8080/monitor/alert/alerts \
  -H "Authorization: Bearer <token>"

# 获取聚合告警
curl http://localhost:8080/monitor/alert/aggregated \
  -H "Authorization: Bearer <token>"

# 获取告警统计
curl http://localhost:8080/monitor/alert/stats \
  -H "Authorization: Bearer <token>"
```

### 4. 确认告警

```bash
curl -X POST http://localhost:8080/monitor/alert/alerts/{alertId}/ack \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <token>" \
  -d '{
    "reason": "已处理，问题已解决"
  }'
```

## 📊 API 端点详解

### 规则管理

#### GET /monitor/alert/rules
获取所有告警规则

**响应示例：**
```json
[
  {
    "id": "cpu_high",
    "name": "CPU 使用率过高",
    "type": 0,
    "severity": 2,
    "metric": "cpu_usage",
    "operator": 2,
    "threshold": 80,
    "duration": 300,
    "enabled": true,
    "maxAlerts": 10,
    "silenceDuration": 300,
    "notifyChannels": ["email"],
    "notifyReceivers": ["admin@example.com"],
    "createTime": 1704067200,
    "updateTime": 1704067200
  }
]
```

#### POST /monitor/alert/rules
创建告警规则

**请求体：**
```json
{
  "id": "new_rule",
  "name": "新规则",
  "type": 0,
  "severity": 1,
  "metric": "metric_name",
  "operator": 2,
  "threshold": 100,
  "duration": 300,
  "enabled": true,
  "maxAlerts": 10,
  "silenceDuration": 300,
  "notifyChannels": ["email"],
  "notifyReceivers": ["user@example.com"]
}
```

#### PUT /monitor/alert/rules/{id}
更新告警规则

#### DELETE /monitor/alert/rules/{id}
删除告警规则

### 告警管理

#### GET /monitor/alert/alerts
获取所有告警

**查询参数：**
- `status`: 告警状态（triggered, acknowledged, resolved）

**响应示例：**
```json
[
  {
    "id": "alert_1704067200_0",
    "ruleId": "cpu_high",
    "ruleName": "CPU 使用率过高",
    "severity": 2,
    "metric": "cpu_usage",
    "value": 85.5,
    "threshold": 80,
    "message": "CPU 使用率过高: 85.5",
    "status": "triggered",
    "triggerTime": 1704067200,
    "acknowledgeTime": 0,
    "resolveTime": 0,
    "acknowledgedBy": "",
    "acknowledgeReason": ""
  }
]
```

#### GET /monitor/alert/alerts/{id}
获取告警详情

#### POST /monitor/alert/alerts/{id}/ack
确认告警

**请求体：**
```json
{
  "reason": "已处理"
}
```

### 聚合告警

#### GET /monitor/alert/aggregated
获取聚合告警

**响应示例：**
```json
[
  {
    "id": "cpu_high_2",
    "ruleId": "cpu_high",
    "ruleName": "CPU 使用率过高",
    "severity": 2,
    "count": 5,
    "lastMessage": "CPU 使用率过高: 85.5",
    "firstTriggerTime": 1704067200,
    "lastTriggerTime": 1704067500,
    "alertIds": ["alert_1704067200_0", "alert_1704067300_1", ...]
  }
]
```

### 统计信息

#### GET /monitor/alert/stats
获取告警统计

**响应示例：**
```json
{
  "engine": {
    "totalRules": 5,
    "enabledRules": 4,
    "totalAlerts": 25,
    "triggeredAlerts": 5,
    "acknowledgedAlerts": 10,
    "resolvedAlerts": 10
  },
  "aggregator": {
    "totalAlerts": 25,
    "aggregatedAlerts": 5,
    "duplicateAlerts": 20,
    "averageAlertsPerGroup": 5
  }
}
```

## 🔧 配置说明

### 告警规则类型

| 值 | 类型 | 说明 |
|----|------|------|
| 0 | THRESHOLD | 阈值告警 |
| 1 | ANOMALY | 异常告警 |
| 2 | TREND | 趋势告警 |
| 3 | COMPOSITE | 组合告警 |

### 告警级别

| 值 | 级别 | 说明 |
|----|------|------|
| 0 | INFO | 信息 |
| 1 | WARNING | 警告 |
| 2 | ERROR | 错误 |
| 3 | CRITICAL | 严重 |

### 比较操作符

| 值 | 操作符 | 说明 |
|----|--------|------|
| 0 | EQUAL | == |
| 1 | NOT_EQUAL | != |
| 2 | GREATER | > |
| 3 | GREATER_EQUAL | >= |
| 4 | LESS | < |
| 5 | LESS_EQUAL | <= |
| 6 | CONTAINS | 包含 |
| 7 | NOT_CONTAINS | 不包含 |

## 💡 最佳实践

1. **规则设计**
   - 避免过于敏感的规则（导致告警风暴）
   - 设置合理的告警持续时间（duration）
   - 定期审查和优化规则

2. **告警聚合**
   - 相同类型的告警应该聚合
   - 避免重复通知
   - 设置合理的聚合窗口（300-600 秒）

3. **通知管理**
   - 根据告警级别选择通知方式
   - 设置合理的沉默时间（silenceDuration）
   - 定期检查通知送达情况

4. **告警处理**
   - 及时确认告警
   - 记录处理过程
   - 定期分析告警根因

## 🔗 相关文件

- [AlertEngine.h](AlertEngine.h) - 告警规则引擎
- [AlertAggregator.h](AlertAggregator.h) - 告警聚合器
- [AlertNotifier.h](AlertNotifier.h) - 告警通知器
- [AlertRule.h](AlertRule.h) - 告警规则定义
- [AlertCtrl.h](AlertCtrl.h) - 告警管理 API
- [README.md](README.md) - 功能文档

