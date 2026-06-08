#include "plugin.h"
#include <ctime>

using Json = nlohmann::json;

void HelloPlugin::greet(const drogon::HttpRequestPtr&,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    Json r;
    r["msg"] = "Hello from hello_plugin!";
    r["time"] = static_cast<long>(std::time(nullptr));
    r["greeting"] = greeting_;
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(r.dump());
    cb(resp);
}

void HelloPlugin::echo(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    Json r;
    r["msg"] = "echo from hello_plugin";
    r["greeting"] = greeting_;
    if (body) r["received"] = *body;
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(r.dump());
    cb(resp);
}

void HelloPlugin::onLoad(const Json& config) {
    if (config.is_object() && config.contains("greeting"))
        greeting_ = config["greeting"].get<std::string>();
}

std::vector<ruoyi::plugin::RouteDescriptor> HelloPlugin::routes() {
    using ruoyi::plugin::RouteDescriptor;
    return {
        RouteDescriptor{
            "/plugin/hello/greet",
            {drogon::Get},
            [this](const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                greet(req, std::move(cb));
            }
        },
        RouteDescriptor{
            "/plugin/hello/echo",
            {drogon::Post},
            [this](const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                echo(req, std::move(cb));
            }
        },
        RouteDescriptor{
            "/plugin/hello/health",
            {drogon::Get},
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
            }
        }
    };
}

std::vector<ruoyi::plugin::MenuItem> HelloPlugin::getMenus() {
    return {{
        "/hello",
        "插件示例",
        "plugin/hello/index",
        "smile",
        0,
        0,
        R"({"icon":"plugin","isCache":true})"
    }};
}

extern "C" {
    RUOYI_PLUGIN_API ruoyi::plugin::IPlugin* createPlugin() {
        return new HelloPlugin();
    }
    RUOYI_PLUGIN_API void destroyPlugin(ruoyi::plugin::IPlugin* p) {
        delete p;
    }
}
