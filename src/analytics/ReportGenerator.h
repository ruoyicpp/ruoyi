/**
 * @file ReportGenerator.h
 * @brief 报表生成器 — 支持多种格式和模板的报表生成
 * 
 * 功能概述：
 *   - 报表模板管理：定义、注册、查询、删除报表模板
 *   - 多格式支持：HTML、CSV、JSON、PDF
 *   - 指标和图表：支持多种指标和图表类型
 *   - 定时生成：支持 Cron 表达式定时生成报表
 *   - 邮件发送：生成后自动发送给指定收件人
 * 
 * 报表模板结构：
 *   - Section（章节）：包含指标和图表
 *   - Metric（指标）：单个数值指标
 *   - ChartConfig（图表）：数据可视化图表
 * 
 * 支持的图表类型：
 *   - line：折线图
 *   - bar：柱状图
 *   - pie：饼图
 *   - scatter：散点图
 *   - table：数据表格
 * 
 * 支持的输出格式：
 *   - html：HTML 网页
 *   - csv：CSV 数据文件
 *   - json：JSON 数据
 *   - pdf：PDF 文档
 * 
 * @see DataWarehouse - 数据仓库
 * @see DataExport - 数据导出
 */

#pragma once

#include <json/json.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Analytics {

using JsonValue = Json::Value;                    ///< JSON 值类型

/**
 * @struct ReportTemplate
 * @brief 报表模板定义
 * 
 * 定义一个报表模板，包含多个章节、指标和图表。
 */
struct ReportTemplate {
    std::string name;                              ///< 模板名称
    std::string description;                       ///< 模板描述

    /**
     * @struct Metric
     * @brief 指标定义
     * 
     * 定义报表中的一个数值指标。
     */
    struct Metric {
        std::string name;                          ///< 指标名称
        std::string label;                         ///< 指标标签
        std::string format;                        ///< 数值格式
        std::string unit;                          ///< 单位
        std::string query;                         ///< 数据查询语句
    };

    /**
     * @struct ChartConfig
     * @brief 图表配置
     * 
     * 定义报表中的一个图表。
     */
    struct ChartConfig {
        std::string name;                          ///< 图表名称
        std::string title;                         ///< 图表标题
        std::string type;                          ///< 图表类型（line | bar | pie | scatter | table）
        std::string query;                         ///< 数据查询语句
        std::string xAxis;                         ///< X 轴字段
        std::vector<std::string> yAxes;            ///< Y 轴字段列表
    };

    /**
     * @struct Section
     * @brief 报表章节
     * 
     * 报表的一个章节，包含指标和图表。
     */
    struct Section {
        std::string name;                          ///< 章节名称
        std::string title;                         ///< 章节标题
        std::vector<Metric> metrics;               ///< 指标列表
        std::vector<ChartConfig> charts;           ///< 图表列表
    };

    std::vector<Section> sections;                 ///< 章节列表
};

/**
 * @struct ScheduleConfig
 * @brief 定时生成配置
 * 
 * 配置报表的定时生成和发送。
 */
struct ScheduleConfig {
    bool enabled = false;                          ///< 是否启用定时生成
    std::string cron;                              ///< Cron 表达式
    std::string format;                            ///< 输出格式（html | csv | json | pdf）
    std::string timezone;                          ///< 时区
    std::vector<std::string> recipients;           ///< 邮件收件人列表
};

/**
 * @struct ReportResult
 * @brief 报表生成结果
 * 
 * 记录报表生成的结果信息。
 */
struct ReportResult {
    int64_t reportId = 0;                          ///< 报表 ID
    std::string name;                              ///< 报表名称
    std::string format;                            ///< 输出格式
    std::string generatedAt;                       ///< 生成时间
    std::string filePath;                          ///< 文件路径
    int64_t fileSize = 0;                          ///< 文件大小
    bool success = false;                          ///< 是否生成成功
    std::string errorMessage;                      ///< 错误信息
    std::map<std::string, std::string> metadata;   ///< 元数据
};

/**
 * @class ReportGenerator
 * @brief 报表生成器
 * 
 * 单例模式，管理报表模板和生成报表。
 */
class ReportGenerator {
public:
    /**
     * @brief 获取单例实例
     * @return ReportGenerator 单例引用
     */
    static ReportGenerator& instance();

    /**
     * @brief 初始化报表生成器
     * @param config 配置（JSON 对象）
     */
    void init(const JsonValue& config);

    /**
     * @brief 关闭报表生成器
     */
    void shutdown();

    /**
     * @brief 注册报表模板
     * @param tmpl 报表模板
     * @return 是否注册成功
     */
    bool registerTemplate(const ReportTemplate& tmpl);

    /**
     * @brief 获取报表模板
     * @param name 模板名称
     * @return 报表模板
     */
    ReportTemplate getTemplate(const std::string& name);

    /**
     * @brief 列出所有模板名称
     * @return 模板名称列表
     */
    std::vector<std::string> listTemplates();

    /**
     * @brief 删除报表模板
     * @param name 模板名称
     * @return 是否删除成功
     */
    bool deleteTemplate(const std::string& name);

    /**
     * @brief 生成报表
     * 
     * 根据指定的模板生成报表。
     * 
     * @param templateName 模板名称
     * @param params 模板参数（默认空）
     * @param format 输出格式（默认 "html"）
     * @return 报表生成结果
     */
    ReportResult generate(const std::string& templateName,
                          const std::map<std::string, std::string>& params = {},
                          const std::string& format = "html");
    ReportResult generate(int64_t reportId,
                          const std::map<std::string, std::string>& params = {},
                          const std::string& format = "html");

    JsonValue previewData(const std::string& templateName,
                          const std::map<std::string, std::string>& params = {});

    void startScheduler();
    void stopScheduler();
    bool updateSchedule(int64_t reportId, const ScheduleConfig& schedule);

    std::vector<ReportResult> listGeneratedReports(int64_t reportId = 0, int page = 1, int pageSize = 20);
    bool deleteGeneratedReport(int64_t generatedId);

private:
    ReportGenerator() = default;
    ~ReportGenerator() = default;
    ReportGenerator(const ReportGenerator&) = delete;
    ReportGenerator& operator=(const ReportGenerator&) = delete;

    std::string getReportPath(int64_t reportId);

    std::string renderHtml(const ReportTemplate& tmpl,
                           const std::map<std::string, std::string>& params,
                           const JsonValue& data);
    std::string renderCsv(const ReportTemplate& tmpl,
                          const std::map<std::string, std::string>& params,
                          const JsonValue& data);
    std::string renderJson(const ReportTemplate& tmpl,
                           const std::map<std::string, std::string>& params,
                           const JsonValue& data);

    bool saveReport(const ReportResult& result);
    std::string expandQuery(const std::string& query,
                            const std::map<std::string, std::string>& params);
    JsonValue executeQuery(const std::string& query);

    mutable std::mutex mutex_;
    std::map<std::string, ReportTemplate> templates_;
    std::atomic<int64_t> nextGeneratedId_{1};
    bool schedulerRunning_ = false;

    std::string outputDir_ = "reports";
    std::string templateDir_ = "templates";
    std::string dbPath_;
};

} // namespace Analytics
