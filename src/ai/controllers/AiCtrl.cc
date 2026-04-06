#include "AiCtrl.h"
#include "services/KoboldCppService.h"
#include "services/WhisperService.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>

using namespace drogon;

void AiCtrl::chat(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body) {
        Json::Value e; e["msg"] = "Invalid JSON";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k400BadRequest);
        return cb(r);
    }
    std::string message      = (*body).get("message", "").asString();
    std::string systemPrompt = (*body).get("system",  "").asString();
    float  temperature = (float)(*body).get("temperature", -1.0).asDouble();
    int    maxTokens   = (*body).get("max_tokens", -1).asInt();

    if (message.empty()) {
        Json::Value e; e["msg"] = "message is required";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k400BadRequest);
        return cb(r);
    }

    auto& svc = KoboldCppService::instance();
    if (!svc.isReady()) {
        Json::Value e; e["code"] = 503; e["msg"] = "KoboldCpp subprocess not running";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k503ServiceUnavailable);
        return cb(r);
    }

    std::string result = svc.chat(message, systemPrompt, temperature, maxTokens);
    Json::Value resp;
    resp["code"] = 200;
    resp["data"]["reply"]   = result;
    resp["data"]["model"]   = "koboldcpp";
    cb(HttpResponse::newHttpJsonResponse(resp));
}

void AiCtrl::generate(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body) {
        Json::Value e; e["msg"] = "Invalid JSON";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k400BadRequest);
        return cb(r);
    }
    std::string prompt     = (*body).get("prompt", "").asString();
    float  temperature = (float)(*body).get("temperature", -1.0).asDouble();
    int    maxTokens   = (*body).get("max_tokens", -1).asInt();

    if (prompt.empty()) {
        Json::Value e; e["msg"] = "prompt is required";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k400BadRequest);
        return cb(r);
    }

    auto& svc = KoboldCppService::instance();
    if (!svc.isReady()) {
        Json::Value e; e["code"] = 503; e["msg"] = "KoboldCpp subprocess not running";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k503ServiceUnavailable);
        return cb(r);
    }

    std::string result = svc.generate(prompt, temperature, maxTokens);
    Json::Value resp;
    resp["code"] = 200;
    resp["data"]["text"] = result;
    cb(HttpResponse::newHttpJsonResponse(resp));
}

void AiCtrl::transcribe(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& cb) {
    auto& svc = WhisperService::instance();
    if (!svc.isReady()) {
        Json::Value e; e["code"] = 503; e["msg"] = "KoboldCpp subprocess not running";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k503ServiceUnavailable);
        return cb(r);
    }

    // 接受 raw PCM float32 body 或 JSON {audio: [float...]}
    std::vector<float> audioData;
    auto ct = req->getHeader("content-type");
    if (ct.find("application/json") != std::string::npos) {
        auto body = req->getJsonObject();
        if (body && (*body).isMember("audio")) {
            for (auto& v : (*body)["audio"])
                audioData.push_back(v.asFloat());
        }
    } else {
        // raw bytes → float32 LE
        const auto& raw = req->getBody();
        size_t n = raw.size() / sizeof(float);
        audioData.resize(n);
        memcpy(audioData.data(), raw.data(), n * sizeof(float));
    }

    if (audioData.empty()) {
        Json::Value e; e["msg"] = "No audio data";
        auto r = HttpResponse::newHttpJsonResponse(e);
        r->setStatusCode(k400BadRequest);
        return cb(r);
    }

    std::string text = svc.transcribe(audioData);
    Json::Value resp;
    resp["code"] = 200;
    resp["data"]["text"] = text;
    cb(HttpResponse::newHttpJsonResponse(resp));
}

void AiCtrl::health(const HttpRequestPtr&,
                     std::function<void(const HttpResponsePtr&)>&& cb) {
    Json::Value resp;
    resp["code"] = 200;
    resp["data"]["koboldcpp"] = KoboldCppService::instance().isReady();
    resp["data"]["whisper"]   = WhisperService::instance().isReady();
    cb(HttpResponse::newHttpJsonResponse(resp));
}
