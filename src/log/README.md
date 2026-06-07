# 日志聚合分析系统 (Log Aggregation & Analysis)

## 📋 模块概述

日志聚合分析系统提供企业级的日志收集、存储、搜索和分析能力，支持 ELK Stack 集成，可以处理海量日志数据。

## 🎯 核心功能

### 1. 日志收集
- 实时日志收集
- 多源日志汇聚
- 日志格式标准化
- 日志缓冲和批处理

### 2. 日志存储
- Elasticsearch 集成
- 日志索引管理
- 日志轮转和归档
- 日志压缩和优化

### 3. 日志搜索
- 全文搜索
- 字段搜索
- 范围查询
- 复杂查询支持

### 4. 日志分析
- 日志统计分析
- 日志趋势分析
- 日志异常检测
- 日志聚合分析

### 5. 日志可视化
- Kibana 集成
- 自定义仪表盘
- 日志图表展示
- 实时日志流

### 6. 日志告警
- 基于日志的告警规则
- 日志异常告警
- 日志错误告警
- 日志性能告警

## 📁 文件结构

```
src/log/
├── LogCollector.h             - 日志收集器
├── LogCollector.cc            - 实现代码
├── LogAnalyzer.h              - 日志分析器
├── LogAnalyzer.cc             - 实现代码
├── LogSearchEngine.h          - 日志搜索引擎
├── LogSearchEngine.cc         - 实现代码
├── LogQuery.h                 - 日志查询接口
├── LogCtrl.h                  - 日志管理 API
├── CMakeLists.txt             - 编译配置
└── README.md                  - 本文件
```

## 🚀 快速开始

### 1. 配置日志系统

```json
{
  "log": {
    "enabled": true,
    "elasticsearch": {
      "host": "localhost",
      "port": 9200,
      "index_prefix": "ruoyi-logs"
    },
    "logstash": {
      "enabled": true,
      "host": "localhost",
      "port": 5000
    },
    "kibana": {
      "enabled": true,
      "host": "localhost",
      "port": 5601
    }
  }
}
```

### 2. 初始化日志系统

```cpp
#include "log/LogCollector.h"

// 初始化
LogCollector::instance().init(config["log"]);

// 启动日志收集
LogCollector::instance().start();
```

### 3. 搜索日志

```cpp
#include "log/LogSearchEngine.h"

// 创建查询
LogQuery query;
query.keyword = "error";
query.timeRange = {startTime, endTime};
query.level = "ERROR";

// 执行搜索
auto results = LogSearchEngine::instance().search(query);
```

## 📊 API 端点

```
GET  /monitor/logs/search              - 搜索日志
GET  /monitor/logs/stats               - 日志统计
GET  /monitor/logs/trends              - 日志趋势
GET  /monitor/logs/analysis            - 日志分析

GET  /monitor/logs/errors              - 获取错误日志
GET  /monitor/logs/warnings            - 获取警告日志
GET  /monitor/logs/performance         - 获取性能日志

POST /monitor/logs/alert/rules         - 创建日志告警规则
GET  /monitor/logs/alert/rules         - 获取日志告警规则
DELETE /monitor/logs/alert/rules/{id}  - 删除日志告警规则
```

## 🔧 ELK Stack 集成

### Elasticsearch 配置

```bash
# 创建索引模板
PUT _index_template/ruoyi-logs
{
  "index_patterns": ["ruoyi-logs-*"],
  "settings": {
    "number_of_shards": 3,
    "number_of_replicas": 1
  },
  "mappings": {
    "properties": {
      "timestamp": {"type": "date"},
      "level": {"type": "keyword"},
      "message": {"type": "text"},
      "traceId": {"type": "keyword"},
      "userId": {"type": "keyword"}
    }
  }
}
```

### Logstash 配置

```logstash
input {
  tcp {
    port => 5000
    codec => json
  }
}

filter {
  mutate {
    add_field => { "[@metadata][index_name]" => "ruoyi-logs-%{+YYYY.MM.dd}" }
  }
}

output {
  elasticsearch {
    hosts => ["localhost:9200"]
    index => "%{[@metadata][index_name]}"
  }
}
```

### Kibana 配置

1. 创建 Index Pattern：`ruoyi-logs-*`
2. 创建自定义仪表盘
3. 配置日志告警规则

## 💡 最佳实践

1. **日志收集**
   - 统一日志格式
   - 添加必要的上下文信息
   - 避免收集敏感信息

2. **日志存储**
   - 合理设置索引保留期
   - 定期归档和清理
   - 优化存储空间

3. **日志搜索**
   - 使用合适的查询条件
   - 避免过于宽泛的搜索
   - 利用缓存提高性能

4. **日志分析**
   - 定期分析日志趋势
   - 及时发现异常
   - 持续优化系统

## 🔗 相关模块

- [Alert](../alert/) - 性能告警系统（基于日志的告警）
- [TaskQueue](../taskqueue/) - 异步任务队列（日志处理任务）
- [Cache](../cache/) - 分布式缓存（日志缓存）

## 📚 参考资源

- [ELK Stack 官方文档](https://www.elastic.co/guide/index.html)
- [日志搜索语法](docs/log-search-syntax.md)
- [日志分析指南](docs/log-analysis-guide.md)

