/**
 * @file ApiDocCtrl.h
 * @brief API 文档控制器 - Swagger/OpenAPI
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
