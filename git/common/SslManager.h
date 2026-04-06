#pragma once
#include <string>
#include <fstream>
#include <filesystem>
#include <json/json.h>
#include <trantor/utils/Logger.h>

// SSL 证书管理器
// 配置存储在 ./ssl/config.json，证书/私钥存储在 ./ssl/server.crt / server.key
// 启动时读取配置，若 enabled=true 且文件存在，则 Drogon 启动 HTTPS 监听器
class SslManager {
public:
    static constexpr const char* CERT_PATH   = "./ssl/server.crt";
    static constexpr const char* KEY_PATH    = "./ssl/server.key";
    static constexpr const char* CONFIG_PATH = "./ssl/config.json";

    struct Config {
        bool   enabled    = false;   // 是否启用 HTTPS
        int    httpsPort  = 18443;   // HTTPS 端口
        int    httpPort   = 18080;   // HTTP 端口（用于生成重定向 URL）
        bool   forceHttps = false;   // 强制 HTTP→HTTPS 跳转
    };

    static Config loadConfig() {
        Config cfg;
        std::ifstream f(CONFIG_PATH);
        if (!f.is_open()) return cfg;
        Json::Value root;
        Json::CharReaderBuilder rb;
        std::string errs;
        if (!Json::parseFromStream(rb, f, &root, &errs)) return cfg;
        cfg.enabled    = root.get("enabled",    false).asBool();
        cfg.httpsPort  = root.get("httpsPort",  18443).asInt();
        cfg.httpPort   = root.get("httpPort",   18080).asInt();
        cfg.forceHttps = root.get("forceHttps", false).asBool();
        return cfg;
    }

    static bool saveConfig(const Config& cfg) {
        std::error_code ec;
        std::filesystem::create_directories("./ssl", ec);
        Json::Value root;
        root["enabled"]    = cfg.enabled;
        root["httpsPort"]  = cfg.httpsPort;
        root["httpPort"]   = cfg.httpPort;
        root["forceHttps"] = cfg.forceHttps;
        Json::StreamWriterBuilder wb;
        wb["indentation"] = "  ";
        std::ofstream f(CONFIG_PATH);
        if (!f) return false;
        f << Json::writeString(wb, root);
        return true;
    }

    // 写入证书 PEM 到磁盘
    static bool writeCert(const std::string& pem) {
        std::error_code ec;
        std::filesystem::create_directories("./ssl", ec);
        std::ofstream f(CERT_PATH);
        if (!f) return false;
        f << pem;
        LOG_INFO << "[SSL] certificate written -> " << CERT_PATH;
        return true;
    }

    // 写入私钥 PEM 到磁盘
    static bool writeKey(const std::string& pem) {
        std::error_code ec;
        std::filesystem::create_directories("./ssl", ec);
        std::ofstream f(KEY_PATH);
        if (!f) return false;
        f << pem;
        LOG_INFO << "[SSL] private key written -> " << KEY_PATH;
        return true;
    }

    // 证书和私钥是否都存在
    static bool certExists() {
        return std::filesystem::exists(CERT_PATH) && std::filesystem::exists(KEY_PATH);
    }

    // 读取证书内容（用于前端预览，只读证书不读私钥）
    static std::string readCert() {
        std::ifstream f(CERT_PATH);
        if (!f) return "";
        return {std::istreambuf_iterator<char>(f), {}};
    }
};
