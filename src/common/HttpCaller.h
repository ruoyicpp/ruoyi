// HttpCaller — 异步 HTTP 客户端调用封装（移植自 wepay-cpp）
//
// 用法：
//   HttpCaller::asyncGet(url, [](bool ok, int status, const std::string& body){ ... });
//   HttpCaller::asyncPost(url, body, "application/json", cb);
//
// 与现有 drogon::HttpClient 直接调用方式可共存。
// 优点：自动 URL 拆 base+path、统一错误回调签名、默认 15s 超时、
//       回调可选（nullptr = 发了不管）。
#pragma once
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpAppFramework.h>
#include <string>
#include <functional>
#include <regex>
#include <iostream>

class HttpCaller {
public:
    using Cb = std::function<void(bool ok, int httpStatus, const std::string &body)>;

    // 异步 GET，回调可选（为空时即"发了不管"）
    static void asyncGet(const std::string &url, Cb cb = nullptr) {
        std::string base, path;
        if (!splitUrl(url, base, path)) {
            if (cb) cb(false, 0, "invalid url");
            return;
        }
        auto client = drogon::HttpClient::newHttpClient(base);
        client->setUserAgent("RuoYi-Cpp/1.0");
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath(path);
        client->sendRequest(req,
            [cb](drogon::ReqResult result, const drogon::HttpResponsePtr &resp) {
                if (cb) {
                    if (result != drogon::ReqResult::Ok || !resp) {
                        cb(false, 0, "request failed");
                    } else {
                        cb(true, (int)resp->statusCode(),
                           std::string(resp->getBody().data(), resp->getBody().size()));
                    }
                }
            }, /*timeout*/ 15.0);
    }

    // 异步 POST，body + Content-Type
    static void asyncPost(const std::string &url, const std::string &body,
                          const std::string &contentType = "application/json",
                          Cb cb = nullptr) {
        std::string base, path;
        if (!splitUrl(url, base, path)) {
            if (cb) cb(false, 0, "invalid url");
            return;
        }
        auto client = drogon::HttpClient::newHttpClient(base);
        client->setUserAgent("RuoYi-Cpp/1.0");
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Post);
        req->setPath(path);
        req->setBody(body);
        req->addHeader("Content-Type", contentType);
        client->sendRequest(req,
            [cb](drogon::ReqResult result, const drogon::HttpResponsePtr &resp) {
                if (cb) {
                    if (result != drogon::ReqResult::Ok || !resp) {
                        cb(false, 0, "request failed");
                    } else {
                        cb(true, (int)resp->statusCode(),
                           std::string(resp->getBody().data(), resp->getBody().size()));
                    }
                }
            }, /*timeout*/ 15.0);
    }

private:
    static bool splitUrl(const std::string &url, std::string &base, std::string &path) {
        std::regex re(R"(^(https?://[^/]+)(/.*)?$)", std::regex::icase);
        std::smatch m;
        if (!std::regex_match(url, m, re)) return false;
        base = m[1].str();
        path = m[2].matched ? m[2].str() : "/";
        return true;
    }
};
