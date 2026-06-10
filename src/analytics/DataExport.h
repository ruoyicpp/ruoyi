/**
 * @file DataExport.h
 * @brief 数据导出 — 支持多种格式的数据导出和异步任务管理
 * 
 * 功能概述：
 *   - 多格式导出：CSV、Excel、JSON、PDF
 *   - 异步导出：支持后台异步导出任务
 *   - 任务管理：提交、查询、取消、删除导出任务
 *   - 列配置：支持列的格式、宽度、隐藏等配置
 *   - 进度追踪：实时追踪导出进度
 * 
 * 支持的导出格式：
 *   - csv：逗号分隔值文件
 *   - excel：Excel 工作簿
 *   - json：JSON 数据格式
 *   - pdf：PDF 文档
 * 
 * 任务状态：
 *   - 0：待执行（pending）
 *   - 1：执行中（running）
 *   - 2：已完成（completed）
 *   - 3：失败（failed）
 * 
 * 使用示例：
 *   // 同步导出
 *   DataExport::instance().exportToCSV(
 *       "SELECT * FROM users",
 *       "/tmp/users.csv");
 *   
 *   // 异步导出
 *   int64_t taskId = DataExport::instance().submitExportTask(
 *       "Export Users",
 *       "SELECT * FROM users",
 *       "csv",
 *       "admin");
 *   
 *   // 查询任务状态
 *   auto task = DataExport::instance().getTaskStatus(taskId);
 * 
 * @see ReportGenerator - 报表生成器
 * @see DataWarehouse - 数据仓库
 */

#pragma once

#include <json/json.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace Analytics {

using JsonValue = Json::Value;                    ///< JSON 值类型

/**
 * @struct ExportTask
 * @brief 导出任务
 * 
 * 表示一个数据导出任务的信息。
 */
struct ExportTask {
    int64_t id = 0;                                ///< 任务 ID
    std::string name;                              ///< 任务名称
    std::string query;                             ///< 数据查询语句
    std::string format;                            ///< 导出格式（csv | excel | json | pdf）
    std::string filePath;                          ///< 输出文件路径
    int64_t totalRows = 0;                         ///< 总行数
    int64_t exportedRows = 0;                      ///< 已导出行数
    int status = 0;                                ///< 任务状态（0=pending, 1=running, 2=completed, 3=failed）
    std::string errorMessage;                      ///< 错误信息
    std::string createdBy;                         ///< 创建者
    std::string createdAt;                         ///< 创建时间
    std::string completedAt;                       ///< 完成时间
    int64_t fileSize = 0;                          ///< 文件大小
};

/**
 * @struct ExportColumn
 * @brief 导出列配置
 * 
 * 定义导出时的列配置，包括格式、宽度等。
 */
struct ExportColumn {
    std::string field;                             ///< 字段名
    std::string label;                             ///< 列标签
    std::string format;                            ///< 数据格式（string | number | date | bool）
    int width = 0;                                 ///< 列宽度
    bool hidden = false;                           ///< 是否隐藏
};

/**
 * @class DataExport
 * @brief 数据导出管理器
 * 
 * 单例模式，提供数据导出功能。
 * 支持同步导出和异步导出任务。
 */
class DataExport {
public:
    /**
     * @brief 获取单例实例
     * @return DataExport 单例引用
     */
    static DataExport& instance();

    /**
     * @brief 初始化导出管理器
     * @param config 配置（JSON 对象）
     */
    void init(const JsonValue& config);

    /**
     * @brief 关闭导出管理器
     */
    void shutdown();

    /**
     * @brief 导出为 CSV 文件
     * 
     * @param query 数据查询语句
     * @param outputPath 输出文件路径
     * @param columns 列配置（可选）
     * @param delimiter 分隔符（默认逗号）
     * @return 是否导出成功
     */
    bool exportToCSV(const std::string& query, const std::string& outputPath,
                     const std::vector<ExportColumn>& columns = {},
                     const std::string& delimiter = ",");

    /**
     * @brief 导出为 Excel 文件
     * 
     * @param query 数据查询语句
     * @param outputPath 输出文件路径
     * @param columns 列配置（可选）
     * @return 是否导出成功
     */
    bool exportToExcel(const std::string& query, const std::string& outputPath,
                       const std::vector<ExportColumn>& columns = {});

    /**
     * @brief 导出为 JSON 文件
     * 
     * @param query 数据查询语句
     * @param outputPath 输出文件路径
     * @return 是否导出成功
     */
    bool exportToJSON(const std::string& query, const std::string& outputPath);

    /**
     * @brief 导出为 PDF 文件
     * 
     * @param query 数据查询语句
     * @param outputPath 输出文件路径
     * @param title 文档标题（可选）
     * @return 是否导出成功
     */
    bool exportToPdf(const std::string& query, const std::string& outputPath,
                     const std::string& title = "");

    /**
     * @brief 提交异步导出任务
     * 
     * @param name 任务名称
     * @param query 数据查询语句
     * @param format 导出格式
     * @param createdBy 创建者
     * @param columns 列配置（可选）
     * @return 任务 ID
     */
    int64_t submitExportTask(const std::string& name,
                             const std::string& query,
                             const std::string& format,
                             const std::string& createdBy,
                             const std::vector<ExportColumn>& columns = {});

    /**
     * @brief 获取任务状态
     * 
     * @param taskId 任务 ID
     * @return 导出任务信息
     */
    ExportTask getTaskStatus(int64_t taskId);

    /**
     * @brief 列出导出任务
     * 
     * @param createdBy 创建者（可选，为空表示所有）
     * @param page 页码（默认 1）
     * @param pageSize 每页数量（默认 20）
     * @return 导出任务列表
     */
    std::vector<ExportTask> listTasks(const std::string& createdBy = "",
                                      int page = 1, int pageSize = 20);

    /**
     * @brief 取消导出任务
     * 
     * @param taskId 任务 ID
     * @return 是否取消成功
     */
    bool cancelTask(int64_t taskId);

    /**
     * @brief 删除导出任务
     * 
     * @param taskId 任务 ID
     * @return 是否删除成功
     */
    bool deleteTask(int64_t taskId);

    /**
     * @brief 获取导出目录
     * @return 导出目录路径
     */
    std::string getExportDir() const { return exportDir_; }

private:
    DataExport();
    ~DataExport();
    DataExport(const DataExport&) = delete;
    DataExport& operator=(const DataExport&) = delete;

    void workerLoop();
    void processTask(const ExportTask& task);
    bool writeCSV(const ExportTask& task);
    bool writeJSON(const ExportTask& task);

    bool exportToCSVImpl(const std::string& query,
                         const std::string& outputPath,
                         const std::vector<ExportColumn>& columns,
                         const std::string& delimiter);
    bool exportToJSONImpl(const std::string& query,
                          const std::string& outputPath);

    mutable std::mutex mutex_;
    std::thread workerThread_;
    std::condition_variable cv_;
    std::queue<ExportTask> taskQueue_;
    std::atomic<bool> running_{false};
    std::atomic<int64_t> nextTaskId_{1};
    std::map<int64_t, ExportTask> tasks_;

    std::string exportDir_ = "exports";
    std::string dbPath_;
};

} // namespace Analytics
