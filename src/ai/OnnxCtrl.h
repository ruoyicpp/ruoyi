#pragma once
// GET  /ai/onnx/models          列出已加载模型
// POST /ai/onnx/load            加载模型 {name, path}
// POST /ai/onnx/infer           推理    {model, inputs:{name:[floats]}}
// GET  /ai/onnx/status          服务状态
#include <drogon/drogon.h>
#include "OnnxService.h"
#include "../common/AjaxResult.h"

class OnnxCtrl : public drogon::HttpController<OnnxCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(OnnxCtrl::listModels, "/ai/onnx/models",  drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(OnnxCtrl::loadModel,  "/ai/onnx/load",    drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(OnnxCtrl::infer,      "/ai/onnx/infer",   drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(OnnxCtrl::status,     "/ai/onnx/status",  drogon::Get);
    METHOD_LIST_END

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
