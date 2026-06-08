#pragma once
#include "PluginManager.h"
#include <drogon/HttpController.h>
#include <filesystem>

class PluginCtrl : public drogon::HttpController<PluginCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(PluginCtrl::list,           "/plugin/list",                    drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(PluginCtrl::discover,       "/plugin/discover",                drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(PluginCtrl::registerPlugin, "/plugin/register/{name}",        drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(PluginCtrl::unloadPlugin,   "/plugin/{name}",                 drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(PluginCtrl::getPlugin,      "/plugin/{name}",                 drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(PluginCtrl::serveFrontend,  "/plugin/{name}/frontend/{*path}", drogon::Get);
        ADD_METHOD_TO(PluginCtrl::getMenus,       "/plugin/{name}/menus",           drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(PluginCtrl::listAllMenus,   "/plugin/menus",                  drogon::Get,    "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr&,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        nlohmann::json out;
        ruoyi::plugin::PluginManager::instance().listPluginsJson(out);
        auto r = drogon::HttpResponse::newHttpResponse();
        r->setStatusCode(drogon::k200OK);
        r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        r->setBody(out.dump());
        cb(r);
    }

    void discover(const drogon::HttpRequestPtr&,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        nlohmann::json out;
        ruoyi::plugin::PluginManager::instance().discoverPlugins(out);
        auto r = drogon::HttpResponse::newHttpResponse();
        r->setStatusCode(drogon::k200OK);
        r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        r->setBody(out.dump());
        cb(r);
    }

    void registerPlugin(const drogon::HttpRequestPtr&,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                        const std::string& name) {
        try {
            const auto& desc = ruoyi::plugin::PluginManager::instance().load(name);
            nlohmann::json resp;
            resp["code"] = 200;
            resp["msg"] = "plugin loaded: " + name;
            resp["data"]["name"] = desc.name;
            resp["data"]["version"] = desc.version;
            resp["data"]["type"] = static_cast<int>(desc.type);
            resp["data"]["frontendEntry"] = desc.frontendEntry;
            auto r = drogon::HttpResponse::newHttpResponse();
            r->setStatusCode(drogon::k200OK);
            r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            r->setBody(resp.dump());
            cb(r);
        } catch (const std::exception& e) {
            nlohmann::json err;
            err["code"] = 500;
            err["msg"] = e.what();
            auto r = drogon::HttpResponse::newHttpResponse();
            r->setStatusCode(drogon::k500InternalServerError);
            r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            r->setBody(err.dump());
            cb(r);
        }
    }

    void unloadPlugin(const drogon::HttpRequestPtr&,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                      const std::string& name) {
        ruoyi::plugin::PluginManager::instance().unload(name);
        nlohmann::json resp;
        resp["code"] = 200;
        resp["msg"] = "plugin unloaded: " + name;
        auto r = drogon::HttpResponse::newHttpResponse();
        r->setStatusCode(drogon::k200OK);
        r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        r->setBody(resp.dump());
        cb(r);
    }

    void getPlugin(const drogon::HttpRequestPtr&,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                   const std::string& name) {
        auto* p = ruoyi::plugin::PluginManager::instance().getPlugin(name);
        nlohmann::json info;
        if (!p) {
            info["code"] = 404;
            info["msg"] = "plugin not loaded: " + name;
            auto r = drogon::HttpResponse::newHttpResponse();
            r->setStatusCode(drogon::k404NotFound);
            r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            r->setBody(info.dump());
            return cb(r);
        }
        info["name"] = p->name();
        info["version"] = p->version();
        info["description"] = p->description();
        info["type"] = static_cast<int>(p->type());
        info["frontendEntry"] = p->frontendEntry();
        nlohmann::json menus;
        ruoyi::plugin::PluginManager::instance().getPluginMenusJson(name, menus);
        info["menus"] = std::move(menus);
        auto r = drogon::HttpResponse::newHttpResponse();
        r->setStatusCode(drogon::k200OK);
        r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        r->setBody(info.dump());
        cb(r);
    }

    void serveFrontend(const drogon::HttpRequestPtr&,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                       const std::string& name,
                       const std::string& path) {
        std::string filePath;
        bool found = ruoyi::plugin::PluginManager::instance().getFrontendPath(name, path, filePath);
        if (!found) {
            auto r = drogon::HttpResponse::newHttpResponse();
            r->setStatusCode(drogon::k404NotFound);
            return cb(r);
        }

        static const std::map<std::string, drogon::ContentType> mimeMap = {
            {".js", drogon::CT_TEXT_JAVASCRIPT},
            {".mjs", drogon::CT_TEXT_JAVASCRIPT},
            {".css", drogon::CT_TEXT_CSS},
            {".json", drogon::CT_APPLICATION_JSON},
            {".html", drogon::CT_TEXT_HTML},
            {".htm", drogon::CT_TEXT_HTML},
            {".png", drogon::CT_IMAGE_PNG},
            {".jpg", drogon::CT_IMAGE_JPG},
            {".jpeg", drogon::CT_IMAGE_JPG},
            {".svg", drogon::CT_IMAGE_SVG_XML},
            {".ico", drogon::CT_IMAGE_XICON},
            {".woff2", drogon::CT_APPLICATION_FONT_WOFF2},
            {".woff", drogon::CT_APPLICATION_FONT_WOFF},
            {".ttf", drogon::CT_APPLICATION_OCTET_STREAM},
            {".gif", drogon::CT_IMAGE_GIF},
            {".webp", drogon::CT_IMAGE_WEBP},
        };
        auto ext = std::filesystem::path(filePath).extension().string();
        auto ctIt = mimeMap.find(ext);
        drogon::ContentType ct = drogon::CT_TEXT_PLAIN;
        if (ctIt != mimeMap.end()) ct = ctIt->second;

        auto r = drogon::HttpResponse::newFileResponse(filePath);
        r->setContentTypeCode(ct);
        r->addHeader("Cache-Control", "public, max-age=3600");
        cb(r);
    }

    void getMenus(const drogon::HttpRequestPtr&,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                  const std::string& name) {
        nlohmann::json jmenus;
        if (!ruoyi::plugin::PluginManager::instance().getPluginMenusJson(name, jmenus)) {
            auto r = drogon::HttpResponse::newHttpResponse();
            r->setStatusCode(drogon::k404NotFound);
            r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            nlohmann::json e;
            e["code"] = 404;
            e["msg"] = "plugin not loaded: " + name;
            r->setBody(e.dump());
            return cb(r);
        }
        auto r = drogon::HttpResponse::newHttpResponse();
        r->setStatusCode(drogon::k200OK);
        r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        r->setBody(jmenus.dump());
        cb(r);
    }

    void listAllMenus(const drogon::HttpRequestPtr&,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        nlohmann::json out;
        ruoyi::plugin::PluginManager::instance().listAllMenusJson(out);
        auto r = drogon::HttpResponse::newHttpResponse();
        r->setStatusCode(drogon::k200OK);
        r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        r->setBody(out.dump());
        cb(r);
    }
};
