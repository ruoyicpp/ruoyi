#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

namespace ruoyi {
namespace plugin {

// 插件类型
enum class PluginType {
    Route,           // 自定义 API 路由
    NotifyChannel,   // 通知渠道（钉钉/飞书/自定义 webhook 等）
    AiProvider,      // AI 模型供应商
    Menu,            // 前端菜单项
};

// 菜单项结构
struct MenuItem {
    std::string path;
    std::string name;
    std::string component;
    std::string icon;
    int menuType = 0;
    int parentId = 0;
    std::string meta;
};

// AI 请求/响应结构
struct AiRequest {
    std::string message;
    std::string systemPrompt;
    float temperature = 0.7f;
    int maxTokens = -1;
};

struct AiResponse {
    bool success = false;
    std::string answer;
    int tokens = 0;
    std::string error;
    std::string providerName;
};

// DLL 导出宏
#ifdef _WIN32
#  ifdef RUOYI_PLUGIN_EXPORTS
#    define RUOYI_PLUGIN_API __declspec(dllexport)
#  else
#    define RUOYI_PLUGIN_API __declspec(dllimport)
#  endif
#else
#  define RUOYI_PLUGIN_API __attribute__((visibility("default")))
#endif

// 抽象插件基类
class IPlugin {
public:
    virtual ~IPlugin() = default;

    virtual std::string name()        const = 0;
    virtual std::string version()      const = 0;
    virtual std::string description()  const = 0;
    virtual PluginType type()         const = 0;

    virtual void onLoad(const nlohmann::json& /*config*/) {}
    virtual void onUnload() {}

    // 注册 drogon 路由
    virtual void registerRoutes() {}

    // 通知渠道
    virtual bool buildRequest(
        const std::string& webhookUrl,
        const std::string& secret,
        const std::string& title,
        const std::string& content,
        std::string& outBody,
        std::vector<std::pair<std::string,std::string>>& outHeaders
    ) { return false; }

    // AI Provider
    virtual void chat(const AiRequest& req,
                      std::function<void(AiResponse)> cb) {}

    // 前端菜单
    virtual std::vector<MenuItem> getMenus() { return {}; }

    // 前端资源入口
    virtual std::string frontendEntry() const { return ""; }
};

// DLL 工厂函数
extern "C" {
    RUOYI_PLUGIN_API IPlugin* createPlugin();
    RUOYI_PLUGIN_API void destroyPlugin(IPlugin* p);
}

// 插件描述
struct PluginDescriptor {
    std::string name;
    std::string version;
    std::string description;
    PluginType type;
    std::string frontendEntry;  // 前端 JS 入口，如 "index.js"
};

}}
