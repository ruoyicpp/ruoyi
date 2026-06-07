/**
 * @file ApiDoc.h
 * @brief Swagger/OpenAPI 3.0 文档生成器
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace ApiDoc {

// ── 数据结构 ────────────────────────────────────────────────────────────────

struct ApiParameter {
    std::string name;
    std::string in;         // query, path, header, cookie
    bool required = false;
    std::string type;       // string, number, integer, boolean, array, object
    std::string description;
    std::string example;
};

struct ApiSchema {
    std::string type;       // string, object, array, integer, boolean
    std::string format;     // int64, date-time, email, etc.
    std::string description;
    std::map<std::string, std::string> properties;  // name -> type
    std::map<std::string, std::string> exampleValues;
};

struct ApiResponse {
    int statusCode;
    std::string description;
    std::string schemaType;
    std::string example;
};

struct ApiEndpoint {
    std::string path;
    std::string method;      // GET, POST, PUT, DELETE, PATCH
    std::string summary;
    std::string description;
    std::string tag;         // grouping tag
    std::vector<ApiParameter> parameters;
    std::vector<ApiResponse> responses;
    std::string requestBodySchema;
    bool authRequired = true;
};

struct SecurityScheme {
    std::string type;        // http, apiKey, oauth2
    std::string scheme;      // bearer, basic
    std::string name;        // for apiKey
    std::string in;          // header, query, cookie
    std::string description;
};

// ── API 文档生成器 ─────────────────────────────────────────────────────────

class ApiDocGenerator {
public:
    static ApiDocGenerator& instance() {
        static ApiDocGenerator inst;
        return inst;
    }

    // 注册端点
    void registerEndpoint(const ApiEndpoint& endpoint) {
        endpoints_.push_back(endpoint);
    }

    // 注册安全方案
    void addSecurityScheme(const SecurityScheme& scheme) {
        securitySchemes_[scheme.name] = scheme;
    }

    // 生成 OpenAPI 3.0 JSON
    std::string generateOpenApiJson() const;

    // 生成 Swagger UI HTML 页面
    std::string generateSwaggerUI() const;

    // 输出文件
    void exportToFile(const std::string& path) const;

private:
    std::vector<ApiEndpoint> endpoints_;
    std::map<std::string, SecurityScheme> securitySchemes_;
};

// ── 便捷宏 ─────────────────────────────────────────────────────────────────

#define REGISTER_API(path, method, summary, tag) \
    static struct _ApiReg_##__LINE__ { \
        _ApiReg_##__LINE__() { \
            ApiDoc::ApiEndpoint ep; \
            ep.path = path; \
            ep.method = method; \
            ep.summary = summary; \
            ep.tag = tag; \
            ApiDoc::ApiDocGenerator::instance().registerEndpoint(ep); \
        } \
    } _api_reg_##__LINE__

} // namespace ApiDoc
