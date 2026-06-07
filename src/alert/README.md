# 性能告警系统 (Alert System)

## 📋 模块概述

性能告警系统是一个企业级的实时告警平台，支持多种告警规则、灵活的聚合策略、多渠道通知和完整的告警历史追踪。

## 🎯 核心功能

### 1. 实时告警规则引擎
- 支持多种告警规则类型（阈值、异常、趋势等）
- 灵活的规则配置和动态更新
- 支持自定义告警条件和表达式

### 2. 告警聚合和去重
- 告警聚合（相同告警合并）
- 告警去重（避免重复告警）
- 告警分组（按类型、源、级别分组）

### 3. 告警通知
- 邮件通知
- 钉钉通知
- 企业微信通知
- Webhook 通知
- 短信通知（可选）

### 4. 告警历史和分析
- 完整的告警历史记录
- 告警统计和分析
- 告警趋势分析
- 告警根因分析

## 📁 文件结构

```
src/alert/
├── AlertEngine.h              - 告警规则引擎
├── AlertEngine.cc             - 实现代码
├── AlertAggregator.h          - 告警聚合器
├── AlertAggregator.cc         - 实现代码
├── AlertNotifier.h            - 告警通知器
├── AlertNotifier.cc           - 实现代码
├── AlertRule.h                - 告警规则定义
├── AlertCtrl.h                - 告警管理 API
├── CMakeLists.txt             - 编译配置
└── README.md                  - 本文件
```

## 🚀 快速开始

### 1. 配置告警规则

```json
{
  "alert": {
    "enabled": true,
    "rules": [
      {
        "id": "cpu_high",
        "name": "CPU 使用率过高",
        "type": "threshold",
        "metric": "cpu_usage",
        "operator": ">",
        "threshold": 80,
        "duration": 300,
        "severity": "warning",
        "enabled": true
      }
    ]
  }
}
```

### 2. 初始化告警引擎

```cpp
#include "alert/AlertEngine.h"

// 初始化
AlertEngine::instance().init(config["alert"]);

// 启动告警引擎
AlertEngine::instance().start();
```

### 3. 触发告警

```cpp
// 报告指标
AlertEngine::instance().reportMetric("cpu_usage", 85.5);

// 报告事件
AlertEngine::instance().reportEvent("system_crash", "Application crashed");
```

## 📊 API 端点

```
GET  /monitor/alert/rules              - 获取告警规则列表
POST /monitor/alert/rules              - 创建告警规则
PUT  /monitor/alert/rules/{id}         - 更新告警规则
DELETE /monitor/alert/rules/{id}       - 删除告警规则

GET  /monitor/alert/history            - 获取告警历史
GET  /monitor/alert/history/{id}       - 获取告警详情
POST /monitor/alert/history/{id}/ack   - 确认告警

GET  /monitor/alert/stats              - 获取告警统计
GET  /monitor/alert/trends             - 获取告警趋势
```

## 🔧 配置说明

### 告警规则类型

| 类型 | 说明 | 示例 |
|------|------|------|
| threshold | 阈值告警 | CPU > 80% |
| anomaly | 异常告警 | 请求延迟异常 |
| trend | 趋势告警 | 内存持续上升 |
| composite | 组合告警 | 多个条件组合 |

### 告警级别

| 级别 | 说明 | 通知方式 |
|------|------|---------|
| info | 信息 | 日志 |
| warning | 警告 | 邮件 |
| error | 错误 | 邮件 + 钉钉 |
| critical | 严重 | 邮件 + 钉钉 + 企业微信 |

## 💡 最佳实践

1. **规则设计**
   - 避免过于敏感的规则（导致告警风暴）
   - 设置合理的告警持续时间
   - 定期审查和优化规则

2. **告警聚合**
   - 相同类型的告警应该聚合
   - 避免重复通知
   - 设置合理的聚合时间窗口

3. **通知管理**
   - 根据告警级别选择通知方式
   - 设置通知接收人和群组
   - 定期检查通知送达情况

4. **告警处理**
   - 及时确认告警
   - 记录处理过程
   - 定期分析告警根因

## 🔗 相关模块

- [TaskQueue](../taskqueue/) - 异步任务队列（用于异步发送通知）
- [Log](../log/) - 日志聚合分析（用于记录告警历史）
- [Monitor](../monitor/) - 系统监控（提供监控数据）

## 📚 参考资源

- [告警规则引擎设计](docs/alert-engine-design.md)
- [告警聚合策略](docs/aggregation-strategy.md)
- [通知集成指南](docs/notification-integration.md)

