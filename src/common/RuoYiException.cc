#include "RuoYiException.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <sstream>

std::string RuoYiException::toJson() const {
    std::string json = "{";
    json += "\"code\":" + std::to_string(httpCode_) + ",";
    json += "\"msg\":\"" + std::string(what()) + "\",";
    json += "\"type\":\"" + typeName() + "\"";
    if (!detail_.empty()) {
        json += ",\"detail\":\"" + detail_ + "\"";
    }
    json += "}";
    return json;
}

std::shared_ptr<drogon::HttpResponse> ExceptionHandler::toResponse(
    const RuoYiException& e) const {
    Json::Value json;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream iss(toAjaxResultJson(e));
    if (!Json::parseFromStream(builder, iss, &json, &errs)) {
        json["code"] = e.httpCode();
        json["msg"] = e.what();
        json["type"] = e.typeName();
    }
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(static_cast<drogon::HttpStatusCode>(e.httpCode()));
    resp->addHeader("X-Error-Type", e.typeName());
    return resp;
}
