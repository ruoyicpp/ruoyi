/**
 * @file ApiDocCtrl.h
 * @brief API 文档控制器 — Swagger/OpenAPI 文档生成和展示
 * 
 * 功能概述：
 *   - OpenAPI 文档生成：自动生成 OpenAPI 3.0 规范文档
 *   - Swagger UI：提供 Swagger UI 界面查看 API 文档
 *   - API 文档导出：支持导出 OpenAPI JSON 格式
 *   - 在线测试：在 Swagger UI 中直接测试 API
 *   - 文档更新：自动同步最新的 API 定义
 * 
 * 核心特性：
 *   - 自动生成：从代码注释自动生成文档
 *   - OpenAPI 3.0：符合 OpenAPI 3.0 规范
 *   - Swagger UI：美观的 Web 界面
 *   - 实时更新：代码变更自动更新文档
 *   - 多语言支持：支持中英文文档
 *   - 权限标注：标注 API 所需权限
 * 
 * API 端点：
 *   - GET /api-docs/openapi.json - 获取 OpenAPI JSON
 *   - GET /api-docs - Swagger UI 重定向
 *   - GET /api-docs/index.html - Swagger UI 页面
 * 
 * 文档内容：
 *   - API 路由：所有 HTTP 端点
 *   - 请求参数：Query、Path、Body 参数
 *   - 响应格式：成功和错误响应
 *   - 权限要求：API 所需权限
 *   - 示例代码：请求和响应示例
 *   - 数据模型：请求和响应数据结构
 * 
 * 使用示例：
 *   ```
 *   访问 Swagger UI：http://localhost:18080/api-docs
 *   获取 OpenAPI JSON：http://localhost:18080/api-docs/openapi.json
 *   ```
 * 
 * 配置项（config.json）：
 *   - api_doc.enabled: 是否启用 API 文档（默认 true）
 *   - api_doc.title: API 文档标题
 *   - api_doc.version: API 版本
 *   - api_doc.description: API 描述
 *   - api_doc.contact: 联系方式
 *   - api_doc.license: 许可证信息
 * 
 * OpenAPI 规范：
 *   - 版本：3.0.0
 *   - 格式：JSON
 *   - 包含：Paths、Components、Security Schemes
 * 
 * @see ApiDoc - API 文档生成器
 */

#pragma once
#include <drogon/HttpController.h>
#include "ApiDoc.h"

class ApiDocCtrl : public drogon::HttpController<ApiDocCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ApiDocCtrl::getOpenApiJson,  "/api-docs/openapi.json", drogon::Get);
        ADD_METHOD_TO(ApiDocCtrl::getSwaggerUI,    "/api-docs",              drogon::Get);
        ADD_METHOD_TO(ApiDocCtrl::getSwaggerIndex,"/api-docs/index.html",   drogon::Get);
    METHOD_LIST_END

    void getOpenApiJson(const drogon::HttpRequestPtr &req,
                        std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setBody(ApiDoc::ApiDocGenerator::instance().generateOpenApiJson());
        cb(resp);
    }

    void getSwaggerUI(const drogon::HttpRequestPtr &req,
                      std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k302Found);
        resp->addHeader("Location", "/api-docs/index.html");
        cb(resp);
    }

    void getSwaggerIndex(const drogon::HttpRequestPtr &req,
                         std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
        resp->setBody(ApiDoc::ApiDocGenerator::instance().generateSwaggerUI());
        cb(resp);
    }
};
