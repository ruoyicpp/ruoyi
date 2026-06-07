/**
 * @file MockHttpClient.h
 * @brief HTTP 客户端 Mock 对象
 */

#pragma once
#include <string>
#include <map>
#include <functional>
#include <vector>

/**
 * @class MockHttpResponse
 * @brief Mock HTTP 响应
 */
class MockHttpResponse {
public:
    void setStatusCode(int code) { statusCode_ = code; }
    int statusCode() const { return statusCode_; }

    void setBody(const std::string& body) { body_ = body; }
    std::string body() const { return body_; }

    void setContentType(const std::string& ct) { contentType_ = ct; }
    std::string contentType() const { return contentType_; }

    void addHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }

    std::string getHeader(const std::string& key) const {
        if (auto it = headers_.find(key); it != headers_.end()) {
            return it->second;
        }
        return "";
    }

private:
    int statusCode_ = 200;
    std::string body_;
    std::string contentType_;
    std::map<std::string, std::string> headers_;
};

/**
 * @class MockHttpRequest
 * @brief Mock HTTP 请求
 */
class MockHttpRequest {
public:
    void setMethod(const std::string& method) { method_ = method; }
    std::string method() const { return method_; }

    void setPath(const std::string& path) { path_ = path; }
    std::string path() const { return path_; }

    void setBody(const std::string& body) { body_ = body; }
    std::string body() const { return body_; }

    void addHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }

    std::string getHeader(const std::string& key) const {
        if (auto it = headers_.find(key); it != headers_.end()) {
            return it->second;
        }
        return "";
    }

    void setParameter(const std::string& key, const std::string& value) {
        params_[key] = value;
    }

    std::string getParameter(const std::string& key) const {
        if (auto it = params_.find(key); it != params_.end()) {
            return it->second;
        }
        return "";
    }

private:
    std::string method_;
    std::string path_;
    std::string body_;
    std::map<std::string, std::string> headers_;
    std::map<std::string, std::string> params_;
};

/**
 * @class MockHttpClient
 * @brief Mock HTTP 客户端，用于模拟外部 API 调用
 */
class MockHttpClient {
public:
    using ResponseCallback = std::function<void(const MockHttpResponse&)>;

    static MockHttpClient& instance() {
        static MockHttpClient inst;
        return inst;
    }

    // 设置预设响应
    void setPredefinedResponse(const std::string& path, const MockHttpResponse& response) {
        predefinedResponses_[path] = response;
    }

    // 设置响应生成器
    void setResponseGenerator(const std::string& path, std::function<MockHttpResponse()> generator) {
        responseGenerators_[path] = std::move(generator);
    }

    // 发起请求
    void sendRequest(const MockHttpRequest& request, ResponseCallback callback) {
        sentRequests_.push_back(request);

        // 优先使用预设响应
        if (auto it = predefinedResponses_.find(request.path()); it != predefinedResponses_.end()) {
            callback(it->second);
            return;
        }

        // 使用响应生成器
        if (auto it = responseGenerators_.find(request.path()); it != responseGenerators_.end()) {
            callback(it->second());
            return;
        }

        // 默认响应
        MockHttpResponse defaultResponse;
        defaultResponse.setStatusCode(404);
        defaultResponse.setBody("{\"error\": \"Not found\"}");
        callback(defaultResponse);
    }

    // 获取发送的请求历史
    const std::vector<MockHttpRequest>& getSentRequests() const { return sentRequests_; }
    void clearHistory() { sentRequests_.clear(); }

    // 便捷方法：预设 JSON 成功响应
    void presetJsonSuccess(const std::string& path, const std::string& jsonBody) {
        MockHttpResponse resp;
        resp.setStatusCode(200);
        resp.setBody(jsonBody);
        resp.setContentType("application/json");
        predefinedResponses_[path] = resp;
    }

    // 便捷方法：预设错误响应
    void presetError(const std::string& path, int statusCode, const std::string& errorMsg) {
        MockHttpResponse resp;
        resp.setStatusCode(statusCode);
        resp.setBody("{\"error\": \"" + errorMsg + "\"}");
        resp.setContentType("application/json");
        predefinedResponses_[path] = resp;
    }

private:
    std::map<std::string, MockHttpResponse> predefinedResponses_;
    std::map<std::string, std::function<MockHttpResponse()>> responseGenerators_;
    std::vector<MockHttpRequest> sentRequests_;
};
