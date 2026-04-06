#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#endif
#include "KoboldCppService.h"
#include "KoboldCppManager.h"
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include <sstream>
#include <string>

KoboldCppService& KoboldCppService::instance() {
    static KoboldCppService inst;
    return inst;
}

// ── 共用：POSIX TCP 连接 + HTTP GET 探测 ─────────────────────────────
#ifndef _WIN32
static bool posixProbeHttp(int port) {
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return false;
    struct timeval tv{2, 0};
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(s, (sockaddr*)&addr, sizeof(addr)) != 0) { ::close(s); return false; }
    std::string req = "GET /api/extra/version HTTP/1.1\r\nHost: 127.0.0.1:\r\nConnection: close\r\n\r\n";
    ::send(s, req.c_str(), req.size(), 0);
    char buf[256]{};
    int n = ::recv(s, buf, sizeof(buf)-1, 0);
    ::close(s);
    return n > 0 && std::string(buf).find("200") != std::string::npos;
}

static std::string posixHttpPost(int port, const std::string& path, const std::string& body) {
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return "";
    struct timeval tv{30, 0};
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(s, (sockaddr*)&addr, sizeof(addr)) != 0) { ::close(s); return ""; }
    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n"
        << "Host: 127.0.0.1:" << port << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    std::string reqStr = req.str();
    ::send(s, reqStr.c_str(), reqStr.size(), 0);
    std::string resp;
    char buf[4096];
    int n;
    while ((n = ::recv(s, buf, sizeof(buf), 0)) > 0) resp.append(buf, n);
    ::close(s);
    auto pos = resp.find("\r\n\r\n");
    return (pos == std::string::npos) ? "" : resp.substr(pos + 4);
}
#endif

bool KoboldCppService::isReady() const {
    // 优先检测 HTTP 端点是否可达（无论进程是否由我们启动）
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    DWORD timeout = 2000; // 2s 快速探测
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u_short)port_);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(s);
        return false;
    }

    std::string req = "GET /api/extra/version HTTP/1.1\r\nHost: 127.0.0.1:"
                    + std::to_string(port_) + "\r\nConnection: close\r\n\r\n";
    send(s, req.c_str(), (int)req.size(), 0);

    char buf[256] = {};
    int  n = recv(s, buf, sizeof(buf)-1, 0);
    closesocket(s);
    // HTTP 200 即就绪
    return n > 0 && std::string(buf).find("200") != std::string::npos;
#else
    return posixProbeHttp(port_);
#endif
}

// Simple blocking HTTP/1.1 POST to localhost using Winsock
std::string KoboldCppService::httpPost(const std::string& path,
                                        const std::string& body) const {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return "";

    // 设置超时 30s
    DWORD timeout = 30000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u_short)port_);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(s);
        return "";
    }

    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n"
        << "Host: 127.0.0.1:" << port_ << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    std::string reqStr = req.str();
    send(s, reqStr.c_str(), (int)reqStr.size(), 0);

    // 读取响应
    std::string resp;
    char buf[4096];
    int  n;
    while ((n = recv(s, buf, sizeof(buf), 0)) > 0)
        resp.append(buf, n);
    closesocket(s);

    // 跳过 HTTP 头
    auto pos = resp.find("\r\n\r\n");
    if (pos == std::string::npos) return "";
    return resp.substr(pos + 4);
#else
    return posixHttpPost(port_, path, body);
#endif
}

std::string KoboldCppService::chat(const std::string& message,
                                    const std::string& systemPrompt,
                                    float temperature, int maxTokens) {
    // 构造 OpenAI messages 数组
    Json::Value msgs(Json::arrayValue);
    if (!systemPrompt.empty()) {
        Json::Value sys;
        sys["role"]    = "system";
        sys["content"] = systemPrompt;
        msgs.append(sys);
    }
    Json::Value usr;
    usr["role"]    = "user";
    usr["content"] = message;
    msgs.append(usr);

    Json::Value req;
    req["model"]       = "koboldcpp";
    req["messages"]    = msgs;
    req["temperature"] = temperature;
    req["max_tokens"]  = maxTokens;

    Json::StreamWriterBuilder wb;
    wb["indentation"] = "";
    std::string body = Json::writeString(wb, req);

    std::string resp = httpPost("/v1/chat/completions", body);
    if (resp.empty()) {
        lastError_ = "No response from koboldcpp";
        return "";
    }

    // 解析响应
    Json::Value root;
    Json::CharReaderBuilder rb;
    std::istringstream ss(resp);
    std::string errs;
    if (!Json::parseFromStream(rb, ss, &root, &errs)) {
        lastError_ = "JSON parse error: " + errs;
        return resp; // 返回原始文本
    }

    try {
        return root["choices"][0]["message"]["content"].asString();
    } catch (...) {
        return resp;
    }
}

std::string KoboldCppService::generate(const std::string& prompt,
                                        float temperature, int maxTokens) {
    Json::Value req;
    req["prompt"]      = prompt;
    req["temperature"] = temperature;
    req["max_length"]  = maxTokens;

    Json::StreamWriterBuilder wb;
    wb["indentation"] = "";
    std::string body = Json::writeString(wb, req);

    std::string resp = httpPost("/api/v1/generate", body);
    if (resp.empty()) {
        lastError_ = "No response from koboldcpp";
        return "";
    }

    Json::Value root;
    Json::CharReaderBuilder rb;
    std::istringstream ss(resp);
    std::string errs;
    if (!Json::parseFromStream(rb, ss, &root, &errs))
        return resp;

    try {
        return root["results"][0]["text"].asString();
    } catch (...) {
        return resp;
    }
}
