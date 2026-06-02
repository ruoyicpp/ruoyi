/**
 * @file OnnxCtrl.h
 * @brief ONNX 模型推理控制器 — 提供 ONNX Runtime 模型加载和推理功能
 * 
 * 功能概述：
 *   - 模型管理：加载、列表、卸载 ONNX 模型
 *   - 推理服务：执行模型推理，支持多输入多输出
 *   - 状态监控：查看 ONNX Runtime 服务状态
 * 
 * API 端点：
 *   GET    /ai/onnx/models    - 列出已加载的 ONNX 模型及其元数据
 *   POST   /ai/onnx/load      - 加载新的 ONNX 模型文件
 *   POST   /ai/onnx/infer     - 执行模型推理
 *   GET    /ai/onnx/status    - 获取 ONNX Runtime 服务状态
 * 
 * 依赖服务：
 *   - OnnxService: ONNX Runtime 推理引擎封装
 * 
 * @note ONNX Runtime 需要单独安装，支持 CPU 和 GPU 推理
 */

#pragma once
#include <drogon/drogon.h>
#include "OnnxService.h"
#include "../common/AjaxResult.h"

/**
 * @class OnnxCtrl
 * @brief ONNX 模型推理 HTTP 控制器
 * 
 * 提供 RESTful API 接口来管理和使用 ONNX 模型。
 * 支持动态加载模型、执行推理、查询模型信息等功能。
 */
class OnnxCtrl : public drogon::HttpController<OnnxCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(OnnxCtrl::listModels, "/ai/onnx/models",  drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(OnnxCtrl::loadModel,  "/ai/onnx/load",    drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(OnnxCtrl::infer,      "/ai/onnx/infer",   drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(OnnxCtrl::status,     "/ai/onnx/status",  drogon::Get);
    METHOD_LIST_END

    /**
     * @brief GET /ai/onnx/models — 列出所有已加载的 ONNX 模型
     * 
     * 返回当前系统中所有 ONNX 模型的元数据，包括模型名称、路径、
     * 输入输出节点名称等信息。
     * 
     * @param req HTTP 请求对象
     * @param cb 响应回调函数
     * 
     * @return JSON 响应，包含模型数组
     * @code
     * {
     *   "code": 200,
     *   "msg": "ok",
     *   "data": [
     *     {
     *       "name": "resnet50",
     *       "path": "/models/resnet50.onnx",
     *       "loaded": true,
     *       "description": "ResNet-50 image classification model",
     *       "inputs": ["input"],
     *       "outputs": ["output"]
     *     }
     *   ]
     * }
     * @endcode
     */
    void listModels(const drogon::HttpRequestPtr&,
                    std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        Json::Value arr(Json::arrayValue);
        for (auto &m : OnnxService::instance().listModels()) {
            Json::Value o;
            o["name"]        = m.name;
            o["path"]        = m.path;
            o["loaded"]      = m.loaded;
            o["description"] = m.description;
            Json::Value ins(Json::arrayValue);
            for (auto &n : m.inputNames) ins.append(n);
            Json::Value outs(Json::arrayValue);
            for (auto &n : m.outputNames) outs.append(n);
            o["inputs"]  = ins;
            o["outputs"] = outs;
            arr.append(o);
        }
        RESP_OK(cb, arr);
    }

    /**
     * @brief POST /ai/onnx/load — 加载新的 ONNX 模型文件
     * 
     * 从指定路径加载 ONNX 模型文件，并将其注册到模型管理器中。
     * 模型加载后可用于推理。
     * 
     * 需要 JWT 认证。
     * 
     * @param req HTTP 请求对象，包含 JSON 请求体
     * @param cb 响应回调函数
     * 
     * @param[in] name 模型名称（必需，用于后续推理时引用）
     * @param[in] path 模型文件路径（必需，支持相对路径和绝对路径）
     * 
     * @return JSON 响应
     * @code
     * {
     *   "code": 200,
     *   "msg": "模型加载成功"
     * }
     * @endcode
     * 
     * @note 
     *   - 模型文件必须是有效的 ONNX 格式
     *   - ONNX Runtime 必须已安装
     *   - 加载大型模型可能需要较长时间
     */
    void loadModel(const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body)["name"].isString() || !(*body)["path"].isString())
            { RESP_ERR(cb, "缺少 name / path"); return; }
        bool ok = OnnxService::instance().loadModel(
            (*body)["name"].asString(), (*body)["path"].asString());
        if (!ok) { RESP_ERR(cb, "模型加载失败（请检查路径或 ONNX Runtime 是否安装）"); return; }
        RESP_MSG(cb, "模型加载成功");
    }

    /**
     * @brief POST /ai/onnx/infer — 执行 ONNX 模型推理
     * 
     * 使用指定的模型对输入数据进行推理，返回推理结果。
     * 支持多输入多输出模型。
     * 
     * 需要 JWT 认证。
     * 
     * @param req HTTP 请求对象，包含 JSON 请求体
     * @param cb 响应回调函数
     * 
     * @param[in] model 模型名称（必需，必须是已加载的模型）
     * @param[in] inputs 输入数据（必需，JSON 对象，键为输入节点名，值为浮点数数组）
     * 
     * @return JSON 响应，包含推理结果
     * @code
     * {
     *   "code": 200,
     *   "msg": "ok",
     *   "data": {
     *     "output": [0.1, 0.2, 0.3, ...]
     *   }
     * }
     * @endcode
     * 
     * @note 
     *   - 输入数据格式必须与模型期望的格式匹配
     *   - 推理可能需要较长时间，取决于模型大小和硬件
     *   - 支持 CPU 和 GPU 推理（取决于 ONNX Runtime 配置）
     */
    void infer(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body)["model"].isString())
            { RESP_ERR(cb, "缺少 model 字段"); return; }
        try {
            auto result = OnnxService::instance().infer(
                (*body)["model"].asString(), (*body)["inputs"]);
            RESP_OK(cb, result);
        } catch (const std::exception &e) { RESP_ERR(cb, e.what()); }
    }

    /**
     * @brief GET /ai/onnx/status — 获取 ONNX Runtime 服务状态
     * 
     * 返回 ONNX Runtime 的可用性状态、模型目录、已加载模型数量等信息。
     * 无需认证，可用于健康检查。
     * 
     * @param req HTTP 请求对象
     * @param cb 响应回调函数
     * 
     * @return JSON 响应
     * @code
     * {
     *   "code": 200,
     *   "msg": "ok",
     *   "data": {
     *     "available": true,
     *     "mode": "runtime",
     *     "hint": "ONNX Runtime ready",
     *     "modelDir": "/models",
     *     "modelCount": 3
     *   }
     * }
     * @endcode
     * 
     * @note 
     *   - available=false 表示 ONNX Runtime 未安装或未编译支持
     *   - mode="stub" 表示使用占位符实现，无法执行真实推理
     */
    void status(const drogon::HttpRequestPtr&,
                std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        Json::Value r;
        bool avail = OnnxService::instance().available();
        r["available"] = avail;
        r["mode"]      = avail ? "runtime" : "stub";
        r["hint"]      = avail ? "ONNX Runtime ready" : "Compile with HAVE_ONNXRUNTIME=1 to enable inference";
        r["modelDir"]  = OnnxService::instance().modelDir();
        r["modelCount"] = (int)OnnxService::instance().listModels().size();
        RESP_OK(cb, r);
    }
};
