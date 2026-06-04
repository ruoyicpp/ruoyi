#include "plugin.h"
#include <ctime>

using Json = nlohmann::json;

// ── 路由处理函数 ─────────────────────────────────────────────────
void HelloPluginCtrl::greet(const drogon::HttpRequestPtr&,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    Json r;
    r["msg"]     = "Hello from hello_plugin!";
    r["time"]    = static_cast<long>(std::time(nullptr));
    r["greeting"] = greeting_;
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(r.dump());
    cb(resp);
}

void HelloPluginCtrl::echo(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    Json r;
    r["msg"]      = "echo from hello_plugin";
    r["greeting"] = greeting_;
    if (body) r["received"] = *body;
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(r.dump());
    cb(resp);
}

// ── 插件主类实现 ─────────────────────────────────────────────────
void HelloPlugin::onLoad(const Json& config) {
    if (config.is_object() && config.contains("greeting"))
        greeting_ = config["greeting"].get<std::string>();
}

void HelloPlugin::registerRoutes() {
    // drogon 在 DLL 加载时通过 DrObjectFactory 自动发现并注册 HelloPluginCtrl。
    // 此处注册一个无过滤器的健康检查路由（用于调试）：

    drogon::app().registerHandler(
        "/plugin/hello/health",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            Json r;
            r["plugin"] = "hello_plugin";
            r["status"] = "ok";
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(r.dump());
            cb(resp);
        },
        {drogon::Get});
}

std::vector<ruoyi::plugin::MenuItem> HelloPlugin::getMenus() {
    return {{
        "/hello",
        "插件示例",
        "plugin/hello/index",
        "smile",
        0,   // menuType: 菜单
        0,   // parentId: 顶级
        R"({"icon":"plugin","isCache":true})"
    }};
}

// ── DLL 工厂函数 ─────────────────────────────────────────────────
extern "C" {
    RUOYI_PLUGIN_API ruoyi::plugin::IPlugin* createPlugin() {
        return new HelloPlugin();
    }
    RUOYI_PLUGIN_API void destroyPlugin(ruoyi::plugin::IPlugin* p) {
        delete p;
    }
}
