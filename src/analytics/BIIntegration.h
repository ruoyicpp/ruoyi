/**
 * @file BIIntegration.h
 * @brief BI 工具集成 — 支持多种商业智能工具的集成
 * 
 * 功能概述：
 *   - 多工具支持：Tableau、Power BI、Metabase、自定义工具
 *   - 工具管理：注册、列表、获取、删除 BI 工具
 *   - 仪表板发布：将内部仪表板发布到外部 BI 工具
 *   - 链接管理：管理仪表板的外部链接和嵌入 URL
 *   - 健康检查：检查 BI 工具的连接状态
 * 
 * 支持的 BI 工具：
 *   - tableau：Tableau 商业智能平台
 *   - power_bi：Microsoft Power BI
 *   - metabase：开源 BI 工具 Metabase
 *   - custom：自定义 BI 工具
 * 
 * 使用示例：
 *   // 注册 BI 工具
 *   BIToolConfig config;
 *   config.name = "tableau";
 *   config.type = "tableau";
 *   config.baseUrl = "https://tableau.example.com";
 *   config.apiKey = "xxx";
 *   BIIntegration::instance().registerTool(config);
 *   
 *   // 发布仪表板
 *   BIIntegration::instance().publishDashboard(
 *       "tableau", dashboardId, "Sales Dashboard");
 *   
 *   // 获取仪表板链接
 *   auto link = BIIntegration::instance().getDashboardLink("tableau", dashboardId);
 * 
 * @see ReportGenerator - 报表生成器
 * @see DataWarehouse - 数据仓库
 */

#pragma once

#include <json/json.h>

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Analytics {

using JsonValue = Json::Value;                    ///< JSON 值类型

/**
 * @struct BIToolConfig
 * @brief BI 工具配置
 * 
 * 定义一个 BI 工具的连接信息和配置。
 */
struct BIToolConfig {
    std::string name;                              ///< 工具名称
    std::string type;                              ///< 工具类型（tableau | power_bi | metabase | custom）
    std::string baseUrl;                           ///< 基础 URL
    std::string apiKey;                            ///< API 密钥
    bool enabled = false;                          ///< 是否启用
    std::map<std::string, std::string> options;    ///< 额外选项
};

/**
 * @struct BIReportLink
 * @brief BI 报表链接
 * 
 * 记录内部仪表板与外部 BI 工具的关联。
 */
struct BIReportLink {
    std::string toolName;                          ///< BI 工具名称
    std::string externalId;                        ///< 外部 ID
    std::string name;                              ///< 报表名称
    std::string url;                               ///< 访问 URL
    std::string embedUrl;                          ///< 嵌入 URL
    std::string lastSyncedAt;                      ///< 最后同步时间
    bool active = true;                            ///< 是否活跃
};

/**
 * @class BIIntegration
 * @brief BI 工具集成管理器
 * 
 * 单例模式，管理 BI 工具的集成和仪表板发布。
 */
class BIIntegration {
public:
    /**
     * @brief 获取单例实例
     * @return BIIntegration 单例引用
     */
    static BIIntegration& instance();

    /**
     * @brief 初始化 BI 集成
     * @param config 配置（JSON 对象）
     */
    void init(const JsonValue& config);

    /**
     * @brief 关闭 BI 集成
     */
    void shutdown();

    /**
     * @brief 注册 BI 工具
     * @param tool BI 工具配置
     * @return 是否注册成功
     */
    bool registerTool(const BIToolConfig& tool);

    /**
     * @brief 列出所有 BI 工具名称
     * @return 工具名称列表
     */
    std::vector<std::string> listTools() const;

    /**
     * @brief 获取 BI 工具配置
     * @param name 工具名称
     * @return BI 工具配置
     */
    BIToolConfig getTool(const std::string& name) const;

    /**
     * @brief 删除 BI 工具
     * @param name 工具名称
     * @return 是否删除成功
     */
    bool removeTool(const std::string& name);

    /**
     * @brief 发布仪表板到 BI 工具
     * 
     * @param toolName BI 工具名称
     * @param dashboardId 仪表板 ID
     * @param dashboardName 仪表板名称
     * @return 是否发布成功
     */
    bool publishDashboard(const std::string& toolName,
                          int64_t dashboardId,
                          const std::string& dashboardName);

    /**
     * @brief 获取仪表板的 BI 工具链接
     * 
     * @param toolName BI 工具名称
     * @param dashboardId 仪表板 ID
     * @return BI 报表链接
     */
    BIReportLink getDashboardLink(const std::string& toolName,
                                  int64_t dashboardId) const;

    /**
     * @brief 列出所有仪表板链接
     * 
     * @param toolName BI 工具名称（可选，为空表示所有工具）
     * @return 报表链接列表
     */
    std::vector<BIReportLink> listDashboardLinks(const std::string& toolName = "") const;

    /**
     * @brief 检查 BI 工具的健康状态
     * @return 健康状态信息（JSON）
     */
    JsonValue health() const;

private:
    BIIntegration() = default;
    ~BIIntegration() = default;
    BIIntegration(const BIIntegration&) = delete;
    BIIntegration& operator=(const BIIntegration&) = delete;

    std::string makeDashboardKey(const std::string& toolName, int64_t dashboardId) const;

    mutable std::mutex mutex_;
    std::map<std::string, BIToolConfig> tools_;
    std::map<std::string, BIReportLink> dashboardLinks_;
};

} // namespace Analytics
