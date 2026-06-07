# 数据分析和 BI (Data Analytics & Business Intelligence)

## 📋 模块概述

数据分析和 BI 模块提供企业级的数据分析和商业智能解决方案，支持数据仓库、报表生成、数据可视化和 BI 工具集成。

## 🎯 核心功能

### 1. 数据仓库
- 数据采集和清洗
- 数据转换和加载（ETL）
- 数据聚合和分析
- 数据质量管理

### 2. 报表生成
- 动态报表生成
- 报表模板引擎
- 报表调度和分发
- 报表版本管理

### 3. 数据可视化
- 图表展示（柱状图、折线图、饼图等）
- 仪表盘展示
- 实时数据更新
- 交互式分析

### 4. 数据导出
- CSV 导出
- Excel 导出
- JSON 导出
- PDF 导出

### 5. BI 工具集成
- Tableau 集成
- Power BI 集成
- Metabase 集成
- 自定义 BI 工具

### 6. 数据分析
- 趋势分析
- 对比分析
- 预测分析
- 异常检测

## 📁 文件结构

```
src/analytics/
├── DataWarehouse.h            - 数据仓库
├── DataWarehouse.cc           - 实现代码
├── ReportGenerator.h          - 报表生成器
├── ReportGenerator.cc         - 实现代码
├── BIIntegration.h            - BI 工具集成
├── BIIntegration.cc           - 实现代码
├── DataExport.h               - 数据导出
├── DataExport.cc              - 实现代码
├── AnalyticsCtrl.h            - 分析管理 API
├── CMakeLists.txt             - 编译配置
└── README.md                  - 本文件
```

## 🚀 快速开始

### 1. 配置数据分析系统

```json
{
  "analytics": {
    "enabled": true,
    "data_warehouse": {
      "enabled": true,
      "database": "ruoyi_dw",
      "etl_interval": 3600
    },
    "report": {
      "enabled": true,
      "template_dir": "./templates",
      "output_dir": "./reports"
    },
    "bi_tools": {
      "tableau": {
        "enabled": false,
        "server": "http://localhost:8000"
      },
      "power_bi": {
        "enabled": false,
        "workspace": ""
      },
      "metabase": {
        "enabled": true,
        "server": "http://localhost:3000"
      }
    }
  }
}
```

### 2. 初始化数据分析系统

```cpp
#include "analytics/DataWarehouse.h"
#include "analytics/ReportGenerator.h"

// 初始化数据仓库
DataWarehouse::instance().init(config["analytics"]["data_warehouse"]);

// 初始化报表生成器
ReportGenerator::instance().init(config["analytics"]["report"]);

// 启动 ETL 流程
DataWarehouse::instance().startETL();
```

### 3. 生成报表

```cpp
// 创建报表
Report report;
report.name = "用户统计报表";
report.template = "user_stats.tpl";
report.parameters["start_date"] = "2024-01-01";
report.parameters["end_date"] = "2024-12-31";

// 生成报表
auto result = ReportGenerator::instance().generate(report);
```

### 4. 导出数据

```cpp
#include "analytics/DataExport.h"

// 导出为 CSV
DataExport::instance().exportToCSV(
    "SELECT * FROM sys_user",
    "users.csv"
);

// 导出为 Excel
DataExport::instance().exportToExcel(
    "SELECT * FROM sys_user",
    "users.xlsx"
);

// 导出为 JSON
DataExport::instance().exportToJSON(
    "SELECT * FROM sys_user",
    "users.json"
);
```

## 📊 API 端点

```
GET  /analytics/reports                - 获取报表列表
POST /analytics/reports                - 创建报表
GET  /analytics/reports/{id}           - 获取报表详情
PUT  /analytics/reports/{id}           - 更新报表
DELETE /analytics/reports/{id}         - 删除报表

POST /analytics/reports/{id}/generate  - 生成报表
GET  /analytics/reports/{id}/download  - 下载报表

GET  /analytics/data/export            - 导出数据
POST /analytics/data/export            - 创建导出任务

GET  /analytics/dashboard              - 获取仪表盘
POST /analytics/dashboard              - 创建仪表盘
PUT  /analytics/dashboard/{id}         - 更新仪表盘

GET  /analytics/charts                 - 获取图表列表
POST /analytics/charts                 - 创建图表
```

## 🔧 ETL 流程

### 数据采集

```cpp
// 从源系统采集数据
auto data = DataWarehouse::instance().collect({
    "source": "sys_user",
    "columns": ["user_id", "user_name", "email"],
    "filter": "status = 1"
});
```

### 数据清洗

```cpp
// 清洗数据
auto cleaned = DataWarehouse::instance().clean(data, {
    "remove_duplicates": true,
    "handle_missing": "fill_default",
    "normalize": true
});
```

### 数据转换

```cpp
// 转换数据
auto transformed = DataWarehouse::instance().transform(cleaned, {
    "aggregations": [
        {"field": "user_id", "function": "count", "alias": "user_count"}
    ],
    "groupby": ["create_date"]
});
```

### 数据加载

```cpp
// 加载到数据仓库
DataWarehouse::instance().load(transformed, {
    "target_table": "dw_user_daily",
    "mode": "replace"
});
```

## 📊 报表模板

### 报表模板示例

```xml
<?xml version="1.0" encoding="UTF-8"?>
<report>
  <name>用户统计报表</name>
  <description>用户数据统计分析</description>
  
  <parameters>
    <parameter name="start_date" type="date" required="true"/>
    <parameter name="end_date" type="date" required="true"/>
  </parameters>
  
  <sections>
    <section name="摘要">
      <metric name="总用户数" query="SELECT COUNT(*) FROM sys_user"/>
      <metric name="活跃用户数" query="SELECT COUNT(*) FROM sys_user WHERE status=1"/>
    </section>
    
    <section name="趋势分析">
      <chart type="line" title="用户增长趋势">
        <query>SELECT DATE(create_time) as date, COUNT(*) as count FROM sys_user WHERE create_time BETWEEN ? AND ? GROUP BY DATE(create_time)</query>
        <parameters>start_date, end_date</parameters>
      </chart>
    </section>
  </sections>
</report>
```

## 💡 最佳实践

1. **数据仓库设计**
   - 合理设计数据模型
   - 建立事实表和维度表
   - 优化查询性能

2. **报表生成**
   - 使用模板提高效率
   - 支持参数化报表
   - 定期更新报表数据

3. **数据可视化**
   - 选择合适的图表类型
   - 突出关键指标
   - 支持交互式分析

4. **数据导出**
   - 支持多种格式
   - 异步导出大数据
   - 安全性和隐私保护

5. **BI 工具集成**
   - 选择合适的 BI 工具
   - 建立数据连接
   - 创建自定义仪表盘

## 🔗 相关模块

- [TaskQueue](../taskqueue/) - 异步任务队列（报表生成、数据导出）
- [Alert](../alert/) - 性能告警系统（数据告警）
- [Log](../log/) - 日志聚合分析（日志分析）
- [Cache](../cache/) - 分布式缓存（缓存分析数据）

## 📚 参考资源

- [数据仓库设计指南](docs/data-warehouse-design.md)
- [报表生成指南](docs/report-generation.md)
- [BI 工具集成指南](docs/bi-integration.md)
- [数据分析最佳实践](docs/analytics-best-practices.md)

