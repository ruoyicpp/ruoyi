#pragma once
#include "IPlugin.h"
#include <map>
#include <mutex>
#include <json/value.h>
#include <atomic>
#include <condition_variable>
#include <chrono>

namespace ruoyi {
namespace plugin {

struct RouteRuntimeState {
    std::atomic_bool accepting{true};
    std::atomic_uint32_t activeRequests{0};
    std::mutex drainMutex;
    std::condition_variable drainCv;
};

// ── 已加载插件实例 ───────────────────────────────────────────────
struct LoadedPlugin {
    std::string dllPath;
    std::string pluginDir;       // plugins/{name}/
    void* handle = nullptr;      // dlopen / LoadLibrary 句柄
    IPlugin* instance = nullptr;
    PluginDescriptor descriptor;
    std::function<void(IPlugin*)> destroyFn;
    std::shared_ptr<RouteRuntimeState> runtimeState;
    std::vector<MenuItem> menus;
};

// ── 插件管理器 ───────────────────────────────────────────────────
class PluginManager {
public:
    static PluginManager& instance();

    const PluginDescriptor& load(const std::string& pluginName);
    std::vector<std::string> autoLoadFromConfig(const Json::Value& root);
    void unload(const std::string& pluginName);

    bool isLoaded(const std::string& pluginName) const;
    const PluginDescriptor* getDescriptor(const std::string& pluginName) const;
    IPlugin* getPlugin(const std::string& pluginName) const;
    std::vector<std::string> listLoaded() const;
    size_t count() const;

    bool getFrontendPath(const std::string& pluginName,
                        const std::string& relativePath,
                        std::string& outFilePath) const;

    void listPluginsJson(nlohmann::json& out) const;
    void listAllMenusJson(nlohmann::json& out) const;
    bool getPluginMenusJson(const std::string& pluginName, nlohmann::json& out) const;

    void discoverPlugins(nlohmann::json& out) const;
    void discoverOne(const std::string& name, nlohmann::json& out) const;
    std::vector<std::string> listDiscovered() const;

private:
    PluginManager() = default;
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    std::string findDllPath(const std::string& pluginName) const;
    std::string dlError() const;
    void registerManagedRoutes(const std::string& pluginName, const std::shared_ptr<LoadedPlugin>& loaded);
    static nlohmann::json menuToJson(const MenuItem& menu);

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<LoadedPlugin>> plugins_;
    mutable std::map<std::string, nlohmann::json> discovered_;
};

}}