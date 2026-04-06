#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#endif
#include "WhisperService.h"
#include "KoboldCppManager.h"
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include <sstream>

WhisperService& WhisperService::instance() {
    static WhisperService inst;
    return inst;
}

bool WhisperService::isReady() const {
#ifdef _WIN32
    return KoboldCppManager::instance().isRunning();
#else
    // Linux: 探测 TCP 端口可达性（更准确）
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return false;
    struct timeval tv{2, 0};
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port_);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    bool ok = (::connect(s, (sockaddr*)&addr, sizeof(addr)) == 0);
    ::close(s);
    return ok;
#endif
}

std::string WhisperService::httpPost(const std::string& path,
                                      const std::string& body) const {
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return "";
    DWORD timeout = 30000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((u_short)port_);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) != 0) { closesocket(s); return ""; }
    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n"
        << "Host: 127.0.0.1:" << port_ << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n" << body;
    std::string reqStr = req.str();
    send(s, reqStr.c_str(), (int)reqStr.size(), 0);
    std::string resp; char buf[4096]; int n;
    while ((n = recv(s, buf, sizeof(buf), 0)) > 0) resp.append(buf, n);
    closesocket(s);
    auto pos = resp.find("\r\n\r\n");
    return (pos == std::string::npos) ? "" : resp.substr(pos + 4);
#else
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return "";
    struct timeval tv{30, 0};
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port_);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(s, (sockaddr*)&addr, sizeof(addr)) != 0) { ::close(s); return ""; }
    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n"
        << "Host: 127.0.0.1:" << port_ << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n" << body;
    std::string reqStr = req.str();
    ::send(s, reqStr.c_str(), reqStr.size(), 0);
    std::string resp; char buf[4096]; int n;
    while ((n = ::recv(s, buf, sizeof(buf), 0)) > 0) resp.append(buf, n);
    ::close(s);
    auto pos = resp.find("\r\n\r\n");
    return (pos == std::string::npos) ? "" : resp.substr(pos + 4);
#endif
}

std::string WhisperService::transcribe(const std::vector<float>& audioData,
                                        const std::string& language) {
    if (audioData.empty()) { lastError_ = "Empty audio"; return ""; }
    if (!isReady())        { lastError_ = "KoboldCpp not running"; return ""; }

    // koboldcpp /api/extra/transcribe accepts base64 WAV or raw PCM JSON array
    // Use JSON float array for simplicity (small clips only)
    Json::Value req;
    Json::Value arr(Json::arrayValue);
    for (float v : audioData) arr.append(v);
    req["audio"]    = arr;
    req["language"] = language;

    Json::StreamWriterBuilder wb; wb["indentation"] = "";
    std::string body = Json::writeString(wb, req);

    std::string resp = httpPost("/api/extra/transcribe", body);
    if (resp.empty()) { lastError_ = "No response"; return ""; }

    Json::Value root; Json::CharReaderBuilder rb;
    std::istringstream ss(resp); std::string errs;
    if (!Json::parseFromStream(rb, ss, &root, &errs)) return resp;
    try { return root["text"].asString(); } catch (...) { return resp; }
}