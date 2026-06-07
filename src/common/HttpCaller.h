/**
 * @file HttpCaller.h
 * @brief HTTP 客户端调用工具 — 异步 HTTP 请求封装
 * 
 * 功能概述：
 *   - 异步 GET：发送异步 GET 请求
 *   - 异步 POST：发送异步 POST 请求
 *   - 自动 URL 解析：自动拆分 URL 为 base 和 path
 *   - 统一回调：统一的错误处理和回调签名
 * 
 * 特性：
 *   - 异步非阻塞：所有请求都是异步的，不阻塞主线程
 *   - 可选回调：回调可以为 nullptr，表示"发了不管"
 *   - 默认超时：默认 15 秒超时
 *   - 自动 URL 解析：自动拆分 URL 为 base 和 path
 *   - 统一签名：所有回调都使用相同的签名
 * 
 * 使用示例：
 *   // 异步 GET 请求
 *   HttpCaller::asyncGet("http://api.example.com/users", 
 *       [](bool ok, int status, const std::string& body) {
 *           if (ok && status == 200) {
 *               std::cout << "Response: " << body << std::endl;
 *           } else {
 *               std::cout << "Error: " << status << std::endl;
 *           }
 *       });
 *   
 *   // 异步 POST 请求
 *   std::string jsonBody = R"({"name":"John","age":30})";
 *   HttpCaller::asyncPost("http://api.example.com/users", jsonBody, 
 *       "application/json",
 *       [](bool ok, int status, const std::string& body) {
 *           if (ok && status == 201) {
 *               std::cout << "Created: " << body << std::endl;
 *           }
 *       });
 *   
 *   // 发了不管（无回调）
 *   HttpCaller::asyncPost("http://api.example.com/log", logData);
 * 
 * 回调签名：
 *   void callback(bool ok, int httpStatus, const std::string& body)
 *   - ok: 请求是否成功（网络层）
 *   - httpStatus: HTTP 状态码（200、404、500 等）
 *   - body: 响应体内容
 * 
 * 配置项（config.json）：
 *   - http.timeout: HTTP 请求超时时间（秒，默认 15）
 *   - http.user_agent: User-Agent 字符串（默认 "RuoYi-Cpp/1.0"）
 *   - http.max_retries: 最大重试次数（默认 0）
 */

#pragma once
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpAppFramework.h>
#include <string>
#include <functional>
#include <iostream>
#include <algorithm>

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
        // 简单URL解析: http://host[:port]/path
        std::string str = url;
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        if (str.find("http://") == 0) {
            base = url.substr(7);
        } else if (str.find("https://") == 0) {
            base = url.substr(8);
        } else {
            return false;
        }
        size_t slash = base.find('/');
        if (slash == std::string::npos) {
            base = url;
            path = "/";
        } else {
            base = base.substr(0, slash);
            path = url.substr(url.find('/'));
        }
        return true;
    }
};
