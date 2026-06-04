#pragma once
#include "IPlugin.h"
#include <map>
#include <mutex>

namespace ruoyi {
namespace plugin {

// ── 已加载插件实例 ───────────────────────────────────────────────
struct LoadedPlugin {
    std::string dllPath;
    std::string pluginDir;       // plugins/{name}/
    void* handle = nullptr;      // dlopen / LoadLibrary 句柄
    IPlugin* instance = nullptr;
    PluginDescriptor descriptor;
    std::function<void(IPlugin*)> destroyFn;
};

// ── 插件管理器 ───────────────────────────────────────────────────
// 热加载流程：
//   前端 import plugin.js → fetch /plugin/register/{name}
//   → PluginManager.load(dllPath) → dlopen → createPlugin()
//   → onLoad → registerRoutes() → 路由生效
class PluginManager {
public:
    static PluginManager& instance();

    // ── 动态加载插件（热激活）────────────────────────────────────
    // 路径格式: plugins/{name}/{name}.dll  (Windows)
    //            plugins/{name}/lib{name}.so (Linux)
    // 返回: 成功返回 PluginDescriptor，失败抛异常
    const PluginDescriptor& load(const std::string& pluginName);

    // 卸载插件
    void unload(const std::string& pluginName);

    // 查询
    bool isLoaded(const std::string& pluginName) const;
    const PluginDescriptor* getDescriptor(const std::string& pluginName) const;
    IPlugin* getPlugin(const std::string& pluginName) const;
    std::vector<std::string> listLoaded() const;
    size_t count() const;

    // 获取插件前端文件路径（供 PluginCtrl serve）
    bool getFrontendPath(const std::string& pluginName,
                        const std::string& relativePath,
                        std::string& outFilePath) const;

    // 获取插件在前端可见的列表（JSON）
    void listPluginsJson(nlohmann::json& out) const;

    // ── 插件发现（不加载，仅扫描目录）─────────────────────────────
    // 扫描 plugins/ 下所有含 plugin.json 或同名 .dll 的目录
    void discoverPlugins(nlohmann::json& out) const;

    // 发现单个插件（返回 plugin.json 内容，无 DLL）
    void discoverOne(const std::string& name, nlohmann::json& out) const;

    // 获取已发现但未加载的插件列表
    std::vector<std::string> listDiscovered() const;

private:
    PluginManager() = default;
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    std::string findDllPath(const std::string& pluginName) const;
    std::string dlError() const;

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<LoadedPlugin>> plugins_;
    mutable std::map<std::string, nlohmann::json> discovered_; // 已扫描到的插件（未加载）
};

}}
