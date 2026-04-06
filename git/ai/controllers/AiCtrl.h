#pragma once
#include <drogon/HttpController.h>

class AiCtrl : public drogon::HttpController<AiCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(AiCtrl::chat,       "/ai/chat",       drogon::Post);
        ADD_METHOD_TO(AiCtrl::generate,   "/ai/generate",   drogon::Post);
        ADD_METHOD_TO(AiCtrl::transcribe, "/ai/transcribe", drogon::Post);
        ADD_METHOD_TO(AiCtrl::health,     "/ai/health",     drogon::Get);
    METHOD_LIST_END

    void chat(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void generate(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void transcribe(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void health(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};
