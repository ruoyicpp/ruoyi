#pragma once
#include "../../src/libs/plugin/IPlugin.h"
#include <drogon/HttpController.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpRequest.h>

using Json = nlohmann::json;

// ── 插件控制器（Drogon HttpController）────────────────────────────
class HelloPluginCtrl : public drogon::HttpController<HelloPluginCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(HelloPluginCtrl::greet, "/plugin/hello/greet", drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(HelloPluginCtrl::echo,  "/plugin/hello/echo",  drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    void greet(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void echo(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};

// ── 插件主类 ───────────────────────────────────────────────────
class HelloPlugin : public ruoyi::plugin::IPlugin {
public:
    std::string name()     const override { return "hello_plugin"; }
    std::string version() const override { return "1.0.0"; }
    std::string description() const override {
        return "示例插件：演示路由 + 菜单 + 前端组件热加载";
    }
    PluginType type() const override { return PluginType::Route; }

    void onLoad(const Json& config) override;
    void registerRoutes() override;
    std::vector<ruoyi::plugin::MenuItem> getMenus() override;
    std::string frontendEntry() const override { return "index.js"; }

private:
    std::string greeting_ = "Hello";
};
