/**
 * @file DataWarehouse.h
 * @brief 数据仓库 — 支持 ETL、数据转换、报表生成的分析引擎
 * 
 * 功能概述：
 *   - ETL 流程：提取（Extract）、转换（Transform）、加载（Load）
 *   - 多数据源支持：内存、JSON 文件、数据库
 *   - 数据转换：分组、聚合、排序、限制
 *   - 报表生成：支持多种图表类型（表格、折线、柱状、饼图等）
 *   - 仪表板管理：组织和展示多个报表
 * 
 * ETL 流程：
 *   - Extract：从多个数据源提取数据
 *   - Transform：数据清洗、转换、聚合
 *   - Load：将数据加载到目标表
 * 
 * 数据源类型：
 *   - memory：内存数据
 *   - json_file：JSON 文件
 *   - database：数据库
 * 
 * 聚合函数：
 *   - count：计数
 *   - sum：求和
 *   - avg：平均值
 *   - min：最小值
 *   - max：最大值
 * 
 * 图表类型：
 *   - table：数据表格
 *   - line：折线图
 *   - bar：柱状图
 *   - pie：饼图
 *   - scatter：散点图
 * 
 * @see ReportGenerator - 报表生成器
 * @see DataExport - 数据导出
 * @see BIIntegration - BI 集成
 */

/**
 * @file DataWarehouse.h
 * @brief 数据仓库和数据分析 — 支持多维分析和数据挖掘
 * 
 * 功能概述：
 *   - 数据采集：从业务系统采集原始数据
 *   - 数据清洗：数据验证、去重、格式转换
 *   - 数据转换：维度建模、聚合计算、衍生指标
 *   - 数据存储：支持列式存储和行式存储
 *   - 多维分析：支持 OLAP 分析、钻取、切片、旋转
 *   - 数据挖掘：关联规则、聚类、分类等算法
 * 
 * 核心特性：
 *   - 星型模型：事实表 + 维度表的经典数据仓库设计
 *   - 增量更新：支持增量数据加载，提高效率
 *   - 分区存储：按时间、地域等维度分区，加速查询
 *   - 物化视图：预计算常用聚合，加速报表生成
 *   - 数据质量：完整性检查、一致性验证、异常检测
 * 
 * 数据模型：
 *   - 事实表：记录业务事件（订单、销售、访问等）
 *   - 维度表：描述事实的属性（时间、地点、产品等）
 *   - 聚合表：预计算的汇总数据（日销售额、月活跃用户等）
 * 
 * 配置项（config.json）：
 *   - datawarehouse.enabled: 是否启用数据仓库（默认 false）
 *   - datawarehouse.storage_type: "columnar" | "row"（默认 "columnar"）
 *   - datawarehouse.retention_days: 数据保留天数（默认 365）
 *   - datawarehouse.partition_by: "day" | "month" | "year"（默认 "day"）
 */

#pragma once

#include <json/json.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/**
 * @namespace Analytics
 * @brief 数据分析命名空间
 */
namespace Analytics {

// ── 类型别名 ──────────────────────────────────────────────────────────
using JsonValue = Json::Value;                    ///< JSON 值类型
using RowData = std::map<std::string, std::string>;  ///< 行数据（列名 → 值）
using TableData = std::vector<RowData>;           ///< 表数据（行列表）

/**
 * @struct DataSourceConfig
 * @brief 数据源配置
 * 
 * 定义数据源的连接信息和选项。
 */
struct DataSourceConfig {
    std::string name;                              ///< 数据源名称
    std::string type;                              ///< 数据源类型（memory | json_file | database）
    std::string connection;                        ///< 连接字符串
    std::map<std::string, std::string> options;    ///< 额外选项
};

/**
 * @struct ETLConfig
 * @brief ETL 流程配置
 * 
 * 配置 ETL（提取、转换、加载）流程的参数。
 */
struct ETLConfig {
    bool enabled = true;                           ///< 是否启用 ETL
    int intervalSeconds = 3600;                    ///< ETL 执行间隔（秒）
    int batchSize = 1000;                          ///< 批处理大小
    bool removeDuplicates = true;                  ///< 是否删除重复数据
    std::string missingValueStrategy;              ///< 缺失值处理策略（null | drop | fill_default）
    bool normalize = true;                         ///< 是否进行数据规范化
};

/**
 * @struct TransformConfig
 * @brief 数据转换配置
 * 
 * 定义数据转换的规则，包括分组、聚合、排序等。
 */
struct TransformConfig {
    std::vector<std::string> groupBy;              ///< 分组字段列表
    
    /**
     * @struct Aggregation
     * @brief 聚合配置
     */
    struct Aggregation {
        std::string field;                         ///< 聚合字段
        std::string function;                      ///< 聚合函数（count | sum | avg | min | max）
        std::string alias;                         ///< 结果别名
    };
    
    std::vector<Aggregation> aggregations;         ///< 聚合配置列表
    std::vector<std::string> orderBy;              ///< 排序字段列表
    std::string orderDirection = "asc";            ///< 排序方向（asc | desc）
    int limit = 0;                                 ///< 结果数量限制（0 表示无限制）
};

/**
 * @struct LoadConfig
 * @brief 数据加载配置
 * 
 * 定义数据加载到目标表的方式。
 */
struct LoadConfig {
    std::string targetTable;                       ///< 目标表名
    std::string mode;                              ///< 加载模式（append | replace | upsert）
    std::string uniqueKey;                         ///< 唯一键（用于 upsert 模式）
};

/**
 * @struct Report
 * @brief 报表定义
 * 
 * 定义一个报表的基本信息和参数。
 */
struct Report {
    int64_t id = 0;                                ///< 报表 ID
    std::string name;                              ///< 报表名称
    std::string description;                       ///< 报表描述
    std::string category;                          ///< 报表分类
    std::string templateName;                      ///< 模板名称
    std::string createdBy;                         ///< 创建者
    std::string createdAt;                         ///< 创建时间
    std::string updatedAt;                         ///< 更新时间
    int status = 0;                                ///< 报表状态
    std::map<std::string, std::string> parameters; ///< 报表参数
};

/**
 * @struct ChartDefinition
 * @brief 图表定义
 * 
 * 定义报表中的一个图表的配置。
 */
struct ChartDefinition {
    int64_t id = 0;                                ///< 图表 ID
    std::string reportId;                          ///< 所属报表 ID
    std::string name;                              ///< 图表名称
    std::string type;                              ///< 图表类型（table | line | bar | pie | scatter）
    std::string query;                             ///< 数据查询语句
    std::string xAxis;                             ///< X 轴字段
    std::string yAxis;                             ///< Y 轴字段
    std::vector<std::string> series;               ///< 数据序列列表
    std::map<std::string, std::string> options;    ///< 图表选项
};

struct Dashboard {
    int64_t id = 0;
    std::string name;
    std::string description;
    std::string layout;
    std::vector<int64_t> chartIds;
};

class DataWarehouse {
public:
    static DataWarehouse& instance();

    void init(const JsonValue& config);
    void shutdown();

    void startETL();
    void stopETL();

    TableData collect(const DataSourceConfig& source,
                      const std::vector<std::string>& columns = {},
                      const std::string& filter = "");
    TableData clean(const TableData& raw, const ETLConfig& config);
    TableData transform(const TableData& cleaned, const TransformConfig& config);
    bool load(const TableData& data, const LoadConfig& config);

    TableData query(const std::string& sql);
    TableData query(const std::string& table,
                    const std::map<std::string, std::string>& conditions = {},
                    const std::vector<std::string>& orderBy = {},
                    int limit = 0,
                    int offset = 0);

    std::vector<Report> listReports(const std::string& category = "", int page = 1, int pageSize = 20);
    Report getReport(int64_t id);
    int64_t createReport(const Report& report);
    bool updateReport(int64_t id, const Report& report);
    bool deleteReport(int64_t id);

    std::vector<ChartDefinition> listCharts(int64_t reportId = 0);
    ChartDefinition getChart(int64_t id);
    int64_t createChart(const ChartDefinition& chart);
    bool updateChart(int64_t id, const ChartDefinition& chart);
    bool deleteChart(int64_t id);

    std::vector<Dashboard> listDashboards();
    Dashboard getDashboard(int64_t id);
    int64_t createDashboard(const Dashboard& dashboard);
    bool updateDashboard(int64_t id, const Dashboard& dashboard);
    bool deleteDashboard(int64_t id);

    bool isETLRunning() const { return etlRunning_.load(); }
    JsonValue getStats();

private:
    DataWarehouse();
    ~DataWarehouse();
    DataWarehouse(const DataWarehouse&) = delete;
    DataWarehouse& operator=(const DataWarehouse&) = delete;

    void etlLoop();
    bool runETLStep(const std::string& step);
    void scheduleETL();
    int64_t nextId();

    mutable std::mutex mutex_;
    std::thread etlThread_;
    std::condition_variable etlCv_;
    std::atomic<bool> etlRunning_{false};
    std::atomic<int64_t> nextId_{1};

    ETLConfig etlConfig_;
    int etlIntervalSeconds_ = 3600;
    std::map<std::string, DataSourceConfig> dataSources_;
    std::string dbPath_;
};

} // namespace Analytics
