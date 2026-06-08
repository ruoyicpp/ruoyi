#include "analytics/BIIntegration.h"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace Analytics {
namespace {

std::string nowString() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace

BIIntegration& BIIntegration::instance() {
    static BIIntegration integration;
    return integration;
}

void BIIntegration::init(const JsonValue& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    tools_.clear();

    const auto& tools = config["tools"];
    if (!tools.isArray()) {
        return;
    }

    for (const auto& node : tools) {
        BIToolConfig tool;
        tool.name = node.get("name", "").asString();
        tool.type = node.get("type", "custom").asString();
        tool.baseUrl = node.get("baseUrl", node.get("server", "")).asString();
        tool.apiKey = node.get("apiKey", "").asString();
        tool.enabled = node.get("enabled", false).asBool();
        if (node["options"].isObject()) {
            for (const auto& key : node["options"].getMemberNames()) {
                tool.options[key] = node["options"][key].asString();
            }
        }
        if (!tool.name.empty()) {
            tools_[tool.name] = tool;
        }
    }
}

void BIIntegration::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    dashboardLinks_.clear();
}

bool BIIntegration::registerTool(const BIToolConfig& tool) {
    if (tool.name.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    tools_[tool.name] = tool;
    return true;
}

std::vector<std::string> BIIntegration::listTools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (const auto& [name, tool] : tools_) {
        (void)tool;
        names.push_back(name);
    }
    return names;
}

BIToolConfig BIIntegration::getTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    return it == tools_.end() ? BIToolConfig{} : it->second;
}

bool BIIntegration::removeTool(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.erase(name) > 0;
}

bool BIIntegration::publishDashboard(const std::string& toolName,
                                     int64_t dashboardId,
                                     const std::string& dashboardName) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(toolName);
    if (it == tools_.end() || !it->second.enabled) {
        return false;
    }

    BIReportLink link;
    link.toolName = toolName;
    link.externalId = toolName + "-dashboard-" + std::to_string(dashboardId);
    link.name = dashboardName;
    link.url = it->second.baseUrl + "/dashboards/" + std::to_string(dashboardId);
    link.embedUrl = link.url + "?embed=true";
    link.lastSyncedAt = nowString();
    dashboardLinks_[makeDashboardKey(toolName, dashboardId)] = link;
    return true;
}

BIReportLink BIIntegration::getDashboardLink(const std::string& toolName, int64_t dashboardId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = dashboardLinks_.find(makeDashboardKey(toolName, dashboardId));
    return it == dashboardLinks_.end() ? BIReportLink{} : it->second;
}

std::vector<BIReportLink> BIIntegration::listDashboardLinks(const std::string& toolName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<BIReportLink> links;
    for (const auto& [key, link] : dashboardLinks_) {
        (void)key;
        if (!toolName.empty() && link.toolName != toolName) {
            continue;
        }
        links.push_back(link);
    }
    return links;
}

JsonValue BIIntegration::health() const {
    std::lock_guard<std::mutex> lock(mutex_);
    JsonValue health(Json::objectValue);
    health["toolCount"] = static_cast<Json::UInt64>(tools_.size());
    health["publishedDashboardCount"] = static_cast<Json::UInt64>(dashboardLinks_.size());

    JsonValue tools(Json::arrayValue);
    for (const auto& [name, tool] : tools_) {
        JsonValue node(Json::objectValue);
        node["name"] = name;
        node["type"] = tool.type;
        node["baseUrl"] = tool.baseUrl;
        node["enabled"] = tool.enabled;
        tools.append(node);
    }
    health["tools"] = tools;
    return health;
}

std::string BIIntegration::makeDashboardKey(const std::string& toolName, int64_t dashboardId) const {
    return toolName + ":" + std::to_string(dashboardId);
}

} // namespace Analytics
