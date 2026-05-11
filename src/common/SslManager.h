#pragma once
#include <string>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/bio.h>
#include <openssl/asn1.h>

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

    // 证书信息（启动时检查 + 前端展示）
    struct CertInfo {
        bool        valid       = false;
        long long   daysLeft    = 0;          // 距过期天数（负数=已过期）
        std::string subject;                  // CN 等主题
        std::string issuer;                   // 颁发者
        std::string notBefore;                // 生效时间 YYYY-MM-DD HH:MM:SS
        std::string notAfter;                 // 过期时间
    };

    static CertInfo certInfo(const std::string& path = CERT_PATH) {
        CertInfo info;
        BIO* bio = BIO_new_file(path.c_str(), "r");
        if (!bio) return info;
        X509* x = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
        BIO_free(bio);
        if (!x) return info;

        // Subject / Issuer
        char buf[256] = {0};
        if (X509_NAME_oneline(X509_get_subject_name(x), buf, sizeof(buf)))
            info.subject = buf;
        if (X509_NAME_oneline(X509_get_issuer_name(x), buf, sizeof(buf)))
            info.issuer = buf;

        // NotBefore / NotAfter
        auto fmtAsn1 = [](const ASN1_TIME* t) -> std::string {
            if (!t) return "";
            BIO* mem = BIO_new(BIO_s_mem());
            ASN1_TIME_print(mem, t);
            char* p = nullptr;
            long n = BIO_get_mem_data(mem, &p);
            std::string s(p, p + n);
            BIO_free(mem);
            return s;
        };
        info.notBefore = fmtAsn1(X509_get0_notBefore(x));
        info.notAfter  = fmtAsn1(X509_get0_notAfter(x));

        // 计算剩余天数
        int days = 0, secs = 0;
        time_t now = std::time(nullptr);
        if (ASN1_TIME_diff(&days, &secs, nullptr, X509_get0_notAfter(x)) == 1) {
            info.daysLeft = (long long)days;
            (void)now; (void)secs;
        }
        info.valid = true;
        X509_free(x);
        return info;
    }

    // 启动时调用：检查证书有效性 / 即将到期；返回剩余天数（-1 表示无证书或解析失败）
    static long long checkCertOnStartup(int warnDays = 30, int criticalDays = 7) {
        if (!certExists()) return -1;
        auto info = certInfo();
        if (!info.valid) {
            LOG_WARN << "[SSL] 证书解析失败: " << CERT_PATH;
            return -1;
        }
        LOG_INFO << "[SSL] cert subject=" << info.subject
                 << " notAfter=" << info.notAfter
                 << " daysLeft=" << info.daysLeft;
        if (info.daysLeft < 0) {
            LOG_ERROR << "[SSL] ⚠ 证书已过期 " << (-info.daysLeft)
                      << " 天，请尽快更换！notAfter=" << info.notAfter;
        } else if (info.daysLeft <= criticalDays) {
            LOG_ERROR << "[SSL] ⚠ 证书 " << info.daysLeft
                      << " 天后过期，紧急！notAfter=" << info.notAfter;
        } else if (info.daysLeft <= warnDays) {
            LOG_WARN  << "[SSL] 证书 " << info.daysLeft
                      << " 天后过期，请及时更换。notAfter=" << info.notAfter;
        }
        return info.daysLeft;
    }
};
