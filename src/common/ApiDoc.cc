/**
 * @file ApiDoc.cc
 * @brief Swagger/OpenAPI 文档生成器实现
 */

#include "ApiDoc.h"
#include "StringUtils.h"
#include <sstream>
#include <iomanip>

namespace ApiDoc {

std::string ApiDocGenerator::generateOpenApiJson() const {
    std::ostringstream ss;

    ss << "{\n";
    ss << "  \"openapi\": \"3.0.3\",\n";
    ss << "  \"info\": {\n";
    ss << "    \"title\": \"RuoYi-CPP API\",\n";
    ss << "    \"description\": \"若依 C++ 后端管理系统 API 文档\",\n";
    ss << "    \"version\": \"1.0.0\",\n";
    ss << "    \"contact\": { \"name\": \"API Support\", \"email\": \"support@ruoyi.com\" }\n";
    ss << "  },\n";
    ss << "  \"servers\": [\n";
    ss << "    { \"url\": \"http://localhost:8080\", \"description\": \"本地开发服务器\" },\n";
    ss << "    { \"url\": \"https://api.ruoyi.com\", \"description\": \"生产服务器\" }\n";
    ss << "  ],\n";

    // 安全方案
    ss << "  \"components\": {\n";
    ss << "    \"securitySchemes\": {\n";
    ss << "      \"BearerAuth\": {\n";
    ss << "        \"type\": \"http\",\n";
    ss << "        \"scheme\": \"bearer\",\n";
    ss << "        \"bearerFormat\": \"JWT\",\n";
    ss << "        \"description\": \"JWT 认证令牌\"\n";
    ss << "      }\n";
    ss << "    },\n";

    // 通用响应
    ss << "    \"responses\": {\n";
    ss << "      \"200\": { \"description\": \"成功\" },\n";
    ss << "      \"400\": { \"description\": \"请求参数错误\" },\n";
    ss << "      \"401\": { \"description\": \"未授权\" },\n";
    ss << "      \"403\": { \"description\": \"权限不足\" },\n";
    ss << "      \"404\": { \"description\": \"资源不存在\" },\n";
    ss << "      \"500\": { \"description\": \"服务器内部错误\" }\n";
    ss << "    },\n";

    // Schema 定义
    ss << "    \"schemas\": {\n";
    ss << "      \"AjaxResult\": {\n";
    ss << "        \"type\": \"object\",\n";
    ss << "        \"properties\": {\n";
    ss << "          \"code\": { \"type\": \"integer\", \"example\": 200 },\n";
    ss << "          \"msg\": { \"type\": \"string\", \"example\": \"操作成功\" },\n";
    ss << "          \"data\": { \"type\": \"object\", \"nullable\": true }\n";
    ss << "        }\n";
    ss << "      },\n";
    ss << "      \"Dept\": {\n";
    ss << "        \"type\": \"object\",\n";
    ss << "        \"properties\": {\n";
    ss << "          \"deptId\": { \"type\": \"integer\", \"format\": \"int64\" },\n";
    ss << "          \"parentId\": { \"type\": \"integer\", \"format\": \"int64\" },\n";
    ss << "          \"deptName\": { \"type\": \"string\" },\n";
    ss << "          \"orderNum\": { \"type\": \"integer\" },\n";
    ss << "          \"leader\": { \"type\": \"string\" },\n";
    ss << "          \"phone\": { \"type\": \"string\" },\n";
    ss << "          \"email\": { \"type\": \"string\", \"format\": \"email\" },\n";
    ss << "          \"status\": { \"type\": \"string\", \"enum\": [\"0\", \"1\"] }\n";
    ss << "        }\n";
    ss << "      },\n";
    ss << "      \"User\": {\n";
    ss << "        \"type\": \"object\",\n";
    ss << "        \"properties\": {\n";
    ss << "          \"userId\": { \"type\": \"integer\", \"format\": \"int64\" },\n";
    ss << "          \"userName\": { \"type\": \"string\" },\n";
    ss << "          \"nickName\": { \"type\": \"string\" },\n";
    ss << "          \"email\": { \"type\": \"string\", \"format\": \"email\" },\n";
    ss << "          \"phonenumber\": { \"type\": \"string\" },\n";
    ss << "          \"sex\": { \"type\": \"string\", \"enum\": [\"0\", \"1\", \"2\"] },\n";
    ss << "          \"avatar\": { \"type\": \"string\" },\n";
    ss << "          \"status\": { \"type\": \"string\", \"enum\": [\"0\", \"1\"] }\n";
    ss << "        }\n";
    ss << "      }\n";
    ss << "    }\n";
    ss << "  },\n";

    // 路径
    ss << "  \"paths\": {\n";

    // 部门管理 API
    ss << "    \"/system/dept/list\": {\n";
    ss << "      \"get\": {\n";
    ss << "        \"tags\": [\"部门管理\"],\n";
    ss << "        \"summary\": \"获取部门列表\",\n";
    ss << "        \"description\": \"查询部门列表，支持树形结构和条件筛选\",\n";
    ss << "        \"operationId\": \"listDept\",\n";
    ss << "        \"parameters\": [\n";
    ss << "          { \"name\": \"deptName\", \"in\": \"query\", \"schema\": { \"type\": \"string\" }, \"description\": \"部门名称\" },\n";
    ss << "          { \"name\": \"status\", \"in\": \"query\", \"schema\": { \"type\": \"string\" }, \"description\": \"部门状态 (0正常 1停用)\" }\n";
    ss << "        ],\n";
    ss << "        \"responses\": {\n";
    ss << "          \"200\": {\n";
    ss << "            \"description\": \"成功\",\n";
    ss << "            \"content\": {\n";
    ss << "              \"application/json\": {\n";
    ss << "                \"schema\": { \"type\": \"array\", \"items\": { \"$ref\": \"#/components/schemas/Dept\" } }\n";
    ss << "              }\n";
    ss << "            }\n";
    ss << "          }\n";
    ss << "        },\n";
    ss << "        \"security\": [{ \"BearerAuth\": [] }]\n";
    ss << "      }\n";
    ss << "    },\n";

    ss << "    \"/system/dept/{deptId}\": {\n";
    ss << "      \"get\": {\n";
    ss << "        \"tags\": [\"部门管理\"],\n";
    ss << "        \"summary\": \"获取部门详情\",\n";
    ss << "        \"operationId\": \"getDept\",\n";
    ss << "        \"parameters\": [\n";
    ss << "          { \"name\": \"deptId\", \"in\": \"path\", \"required\": true, \"schema\": { \"type\": \"integer\" } }\n";
    ss << "        ],\n";
    ss << "        \"responses\": {\n";
    ss << "          \"200\": { \"description\": \"成功\", \"content\": { \"application/json\": { \"schema\": { \"$ref\": \"#/components/schemas/Dept\" } } } }\n";
    ss << "        },\n";
    ss << "        \"security\": [{ \"BearerAuth\": [] }]\n";
    ss << "      },\n";
    ss << "      \"put\": {\n";
    ss << "        \"tags\": [\"部门管理\"],\n";
    ss << "        \"summary\": \"修改部门\",\n";
    ss << "        \"operationId\": \"updateDept\",\n";
    ss << "        \"requestBody\": {\n";
    ss << "          \"required\": true,\n";
    ss << "          \"content\": { \"application/json\": { \"schema\": { \"$ref\": \"#/components/schemas/Dept\" } } }\n";
    ss << "        },\n";
    ss << "        \"responses\": { \"200\": { \"description\": \"成功\" } },\n";
    ss << "        \"security\": [{ \"BearerAuth\": [] }]\n";
    ss << "      },\n";
    ss << "      \"delete\": {\n";
    ss << "        \"tags\": [\"部门管理\"],\n";
    ss << "        \"summary\": \"删除部门\",\n";
    ss << "        \"operationId\": \"deleteDept\",\n";
    ss << "        \"parameters\": [\n";
    ss << "          { \"name\": \"deptId\", \"in\": \"path\", \"required\": true, \"schema\": { \"type\": \"integer\" } }\n";
    ss << "        ],\n";
    ss << "        \"responses\": { \"200\": { \"description\": \"成功\" } },\n";
    ss << "        \"security\": [{ \"BearerAuth\": [] }]\n";
    ss << "      }\n";
    ss << "    },\n";

    // 部门新增
    ss << "    \"/system/dept\": {\n";
    ss << "      \"post\": {\n";
    ss << "        \"tags\": [\"部门管理\"],\n";
    ss << "        \"summary\": \"新增部门\",\n";
    ss << "        \"operationId\": \"addDept\",\n";
    ss << "        \"requestBody\": {\n";
    ss << "          \"required\": true,\n";
    ss << "          \"content\": { \"application/json\": { \"schema\": { \"$ref\": \"#/components/schemas/Dept\" } } }\n";
    ss << "        },\n";
    ss << "        \"responses\": { \"200\": { \"description\": \"成功\" } },\n";
    ss << "        \"security\": [{ \"BearerAuth\": [] }]\n";
    ss << "      }\n";
    ss << "    },\n";

    // 用户管理 API
    ss << "    \"/system/user/list\": {\n";
    ss << "      \"get\": {\n";
    ss << "        \"tags\": [\"用户管理\"],\n";
    ss << "        \"summary\": \"获取用户列表\",\n";
    ss << "        \"operationId\": \"listUser\",\n";
    ss << "        \"parameters\": [\n";
    ss << "          { \"name\": \"userName\", \"in\": \"query\", \"schema\": { \"type\": \"string\" } },\n";
    ss << "          { \"name\": \"phonenumber\", \"in\": \"query\", \"schema\": { \"type\": \"string\" } },\n";
    ss << "          { \"name\": \"status\", \"in\": \"query\", \"schema\": { \"type\": \"string\" } },\n";
    ss << "          { \"name\": \"deptId\", \"in\": \"query\", \"schema\": { \"type\": \"integer\" } }\n";
    ss << "        ],\n";
    ss << "        \"responses\": {\n";
    ss << "          \"200\": { \"description\": \"成功\", \"content\": { \"application/json\": { \"schema\": { \"type\": \"array\", \"items\": { \"$ref\": \"#/components/schemas/User\" } } } } }\n";
    ss << "        },\n";
    ss << "        \"security\": [{ \"BearerAuth\": [] }]\n";
    ss << "      }\n";
    ss << "    },\n";

    // 登录 API
    ss << "    \"/login\": {\n";
    ss << "      \"post\": {\n";
    ss << "        \"tags\": [\"认证\"],\n";
    ss << "        \"summary\": \"用户登录\",\n";
    ss << "        \"operationId\": \"login\",\n";
    ss << "        \"requestBody\": {\n";
    ss << "          \"required\": true,\n";
    ss << "          \"content\": {\n";
    ss << "            \"application/json\": {\n";
    ss << "              \"schema\": {\n";
    ss << "                \"type\": \"object\",\n";
    ss << "                \"properties\": {\n";
    ss << "                  \"username\": { \"type\": \"string\", \"example\": \"admin\" },\n";
    ss << "                  \"password\": { \"type\": \"string\", \"example\": \"admin123\" },\n";
    ss << "                  \"code\": { \"type\": \"string\", \"example\": \"1234\" },\n";
    ss << "                  \"uuid\": { \"type\": \"string\" }\n";
    ss << "                }\n";
    ss << "              }\n";
    ss << "            }\n";
    ss << "          }\n";
    ss << "        },\n";
    ss << "        \"responses\": {\n";
    ss << "          \"200\": { \"description\": \"登录成功，返回 token\" }\n";
    ss << "        }\n";
    ss << "      }\n";
    ss << "    },\n";

    ss << "    \"/logout\": {\n";
    ss << "      \"post\": {\n";
    ss << "        \"tags\": [\"认证\"],\n";
    ss << "        \"summary\": \"用户登出\",\n";
    ss << "        \"operationId\": \"logout\",\n";
    ss << "        \"responses\": { \"200\": { \"description\": \"成功\" } },\n";
    ss << "        \"security\": [{ \"BearerAuth\": [] }]\n";
    ss << "      }\n";
    ss << "    }\n";

    ss << "  }\n";
    ss << "}\n";

    return ss.str();
}

std::string ApiDocGenerator::generateSwaggerUI() const {
    return R"(
<!DOCTYPE html>
<html>
<head>
    <title>RuoYi-CPP API 文档</title>
    <link rel="stylesheet" type="text/css" href="https://unpkg.com/swagger-ui-dist@5/swagger-ui.css">
    <style>
        body { margin: 0; padding: 0; }
        .topbar { display: none; }
    </style>
</head>
<body>
    <div id="swagger-ui"></div>
    <script src="https://unpkg.com/swagger-ui-dist@5/swagger-ui-bundle.js"></script>
    <script>
        window.onload = function() {
            SwaggerUIBundle({
                url: "/api-docs/openapi.json",
                dom_id: '#swagger-ui',
                presets: [SwaggerUIBundle.presets.apis, SwaggerUIBundle.SwaggerUIStandalonePreset],
                layout: "StandaloneLayout",
                deepLinking: true
            });
        };
    </script>
</body>
</html>
)";
}

void ApiDocGenerator::exportToFile(const std::string& path) const {
    // 实现文件导出逻辑
    // std::ofstream ofs(path);
    // ofs << generateOpenApiJson();
}

} // namespace ApiDoc
