#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"

// Swagger UI 托管（CDN 加载 swagger-ui，本地提供 OpenAPI 描述）
class SwaggerCtrl : public drogon::HttpController<SwaggerCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SwaggerCtrl::uiHtml,   "/swagger-ui/index.html", drogon::Get);
        ADD_METHOD_TO(SwaggerCtrl::uiHtml,   "/swagger-ui/",           drogon::Get);
        ADD_METHOD_TO(SwaggerCtrl::apiDocs,  "/v2/api-docs",           drogon::Get);
        ADD_METHOD_TO(SwaggerCtrl::apiDocs3, "/v3/api-docs",           drogon::Get);
    METHOD_LIST_END

    // 返回 Swagger UI HTML（从 CDN 加载 swagger-ui-dist）
    void uiHtml(const drogon::HttpRequestPtr&,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->setBody(R"html(<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>RuoYi-Cpp API 文档</title>
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/swagger-ui-dist@5/swagger-ui.css">
  <style>
    body { margin: 0; background: #fafafa; }
    .topbar { display: none !important; }
    #swagger-ui { max-width: 1460px; margin: 0 auto; padding: 10px 20px; }
  </style>
</head>
<body>
<div id="swagger-ui"></div>
<script src="https://cdn.jsdelivr.net/npm/swagger-ui-dist@5/swagger-ui-bundle.js"></script>
<script>
window.onload = function () {
  SwaggerUIBundle({
    url: "/prod-api/v3/api-docs",
    dom_id: '#swagger-ui',
    presets: [SwaggerUIBundle.presets.apis, SwaggerUIBundle.SwaggerUIStandalonePreset],
    layout: "BaseLayout",
    deepLinking: true,
    defaultModelsExpandDepth: 1,
    defaultModelExpandDepth: 1
  })
}
</script>
</body>
</html>)html");
        cb(resp);
    }

    // OpenAPI 2.0 描述（供老版 Swagger UI 使用，重定向到 v3）
    void apiDocs(const drogon::HttpRequestPtr&,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k302Found);
        resp->addHeader("Location", "/prod-api/v3/api-docs");
        cb(resp);
    }

    // OpenAPI 3.0 描述
    void apiDocs3(const drogon::HttpRequestPtr&,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        cb(drogon::HttpResponse::newHttpJsonResponse(buildSpec()));
    }

private:
    static Json::Value buildSpec() {
        Json::Value spec;
        spec["openapi"] = "3.0.3";

        Json::Value info;
        info["title"]       = "RuoYi-Cpp API";
        info["description"] = "基于 Drogon C++ 框架的 RuoYi 后台管理系统接口文档";
        info["version"]     = "1.0.0";
        spec["info"] = info;

        Json::Value server;
        server["url"]         = "/prod-api";
        server["description"] = "当前服务器";
        spec["servers"].append(server);

        // 全局安全方案
        Json::Value bearerScheme;
        bearerScheme["type"]         = "http";
        bearerScheme["scheme"]       = "bearer";
        bearerScheme["bearerFormat"] = "JWT";
        spec["components"]["securitySchemes"]["bearerAuth"] = bearerScheme;

        Json::Value globalSec;
        globalSec["bearerAuth"] = Json::Value(Json::arrayValue);
        spec["security"].append(globalSec);

        // 标签分组
        auto addTag = [&](const std::string& name, const std::string& desc) {
            Json::Value t; t["name"] = name; t["description"] = desc;
            spec["tags"].append(t);
        };
        addTag("认证",     "登录 / 注销 / 验证码");
        addTag("用户管理", "/system/user");
        addTag("角色管理", "/system/role");
        addTag("菜单管理", "/system/menu");
        addTag("部门管理", "/system/dept");
        addTag("参数配置", "/system/config");
        addTag("字典管理", "/system/dict");
        addTag("通知公告", "/system/notice");
        addTag("操作日志", "/monitor/operlog");
        addTag("登录日志", "/monitor/logininfor");
        addTag("在线用户", "/monitor/online");
        addTag("定时任务", "/monitor/job");
        addTag("IP封禁",   "/monitor/ip");
        addTag("视频",     "/api/video");
        addTag("工具",     "/tool");

        // 部分常用路径示例
        auto addPath = [&](const std::string& path, const std::string& method,
                           const std::string& tag, const std::string& summary) {
            Json::Value op;
            op["tags"].append(tag);
            op["summary"] = summary;
            op["responses"]["200"]["description"] = "成功";
            spec["paths"][path][method] = op;
        };
        addPath("/system/user/list",   "get",  "用户管理", "查询用户列表");
        addPath("/system/role/list",   "get",  "角色管理", "查询角色列表");
        addPath("/system/menu/list",   "get",  "菜单管理", "查询菜单列表");
        addPath("/system/dept/list",   "get",  "部门管理", "查询部门列表");
        addPath("/system/config/list", "get",  "参数配置", "查询参数列表");
        addPath("/monitor/job/list",   "get",  "定时任务", "查询任务列表");
        addPath("/monitor/ip/banned",  "get",  "IP封禁",   "查询速率封禁列表");
        addPath("/api/video/random",   "get",  "视频",     "获取随机视频 URL");
        addPath("/captchaImage",       "get",  "认证",     "获取验证码");
        addPath("/login",              "post", "认证",     "用户登录");
        addPath("/logout",             "delete","认证",    "用户注销");

        return spec;
    }
};
