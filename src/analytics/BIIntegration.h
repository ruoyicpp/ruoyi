#pragma once

#include <json/json.h>

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Analytics {

using JsonValue = Json::Value;

struct BIToolConfig {
    std::string name;
    std::string type;        // tableau | power_bi | metabase | custom
    std::string baseUrl;
    std::string apiKey;
    bool enabled = false;
    std::map<std::string, std::string> options;
};

struct BIReportLink {
    std::string toolName;
    std::string externalId;
    std::string name;
    std::string url;
    std::string embedUrl;
    std::string lastSyncedAt;
    bool active = true;
};

class BIIntegration {
public:
    static BIIntegration& instance();

    void init(const JsonValue& config);
    void shutdown();

    bool registerTool(const BIToolConfig& tool);
    std::vector<std::string> listTools() const;
    BIToolConfig getTool(const std::string& name) const;
    bool removeTool(const std::string& name);

    bool publishDashboard(const std::string& toolName,
                          int64_t dashboardId,
                          const std::string& dashboardName);
    BIReportLink getDashboardLink(const std::string& toolName,
                                  int64_t dashboardId) const;
    std::vector<BIReportLink> listDashboardLinks(const std::string& toolName = "") const;

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
