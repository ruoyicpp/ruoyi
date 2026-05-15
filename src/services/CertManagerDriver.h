// =============================================================
// CertManagerDriver.h — certmanager.dll / certmanager.so 动态库封装
//
// 通过运行时加载（LoadLibrary / dlopen）调用 certmanager 导出的 C 函数，
// 实现 ACME 证书申请、续期、查询，以及可选 Web UI 服务启动。
//
// 导出符号（certlib.go build -buildmode=c-shared）：
//   CertManager_Init(basePath, caServer)      -> JSON
//   CertManager_Free(p)                       -> void
//   CertManager_RegisterAccount(email, kt)    -> JSON
//   CertManager_ListAccounts()                -> JSON
//   CertManager_ObtainCert(email,domains,     -> JSON
//                          provider,envJson,
//                          keyType,bundle)
//   CertManager_RenewCert(certId, bundle)     -> JSON
//   CertManager_RevokeCert(certId)            -> JSON
//   CertManager_ListCerts()                   -> JSON
//   CertManager_GetCertInfo(certId)           -> JSON
//   CertManager_ReadCertFile(certId,type)     -> JSON
//   CertManager_ListDNSProviders()            -> JSON
//   CertManager_StartServer(addr)             -> JSON
//   CertManager_StopServer()                  -> JSON
//   CertManager_Version()                     -> char*
//
// 所有 JSON 返回格式：
//   成功: {"success":true,"data":{...}}
//   失败: {"success":false,"error":"..."}
//
// 使用示例（config.json -> "acme"）：
//   {
//     "enabled": true,
//     "certmanager_lib": "certmanager.dll",
//     "lego_path": ".lego",
//     "ca_server": "",
//     "email": "admin@example.com",
//     "domains": ["example.com","*.example.com"],
//     "dns_provider": "alidns",
//     "env_vars": {"ALICLOUD_ACCESS_KEY":"xxx","ALICLOUD_SECRET_KEY":"yyy"},
//     "renew_days_before": 30,
//     "check_interval_hours": 24,
//     "cert_out": "nginx/ssl/server.crt",
//     "key_out":  "nginx/ssl/server.key",
//     "reload_nginx_after": true,
//     "web_ui_addr": ":19090"
//   }
// =============================================================
#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include "../common/NginxEmbedded.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  define CM_DLOPEN(p)    (void*)LoadLibraryA(p)
#  define CM_DLSYM(h,n)   (void*)GetProcAddress((HMODULE)(h),(n))
#  define CM_DLCLOSE(h)   FreeLibrary((HMODULE)(h))
#else
#  include <dlfcn.h>
#  define CM_DLOPEN(p)    dlopen((p), RTLD_LAZY | RTLD_LOCAL)
#  define CM_DLSYM(h,n)   dlsym((h),(n))
#  define CM_DLCLOSE(h)   dlclose(h)
#endif

// ── DLL 函数指针表 ─────────────────────────────────────────────────────────
struct CertManagerDriver {
    using fp_Init        = char* (*)(const char*, const char*);
    using fp_Free        = void  (*)(char*);
    using fp_RegAccount  = char* (*)(const char*, const char*);
    using fp_ListAcct    = char* (*)();
    using fp_Obtain      = char* (*)(const char*, const char*, const char*,
                                     const char*, const char*, int);
    using fp_Renew       = char* (*)(const char*, int);
    using fp_Revoke      = char* (*)(const char*);
    using fp_ListCerts   = char* (*)();
    using fp_GetInfo     = char* (*)(const char*);
    using fp_ReadFile    = char* (*)(const char*, const char*);
    using fp_ListDNS     = char* (*)();
    using fp_StartSrv    = char* (*)(const char*);
    using fp_StopSrv     = char* (*)();
    using fp_Version     = char* (*)();

    void* hLib = nullptr;

    fp_Init       Init          = nullptr;
    fp_Free       Free          = nullptr;
    fp_RegAccount RegisterAccount = nullptr;
    fp_ListAcct   ListAccounts  = nullptr;
    fp_Obtain     ObtainCert    = nullptr;
    fp_Renew      RenewCert     = nullptr;
    fp_Revoke     RevokeCert    = nullptr;
    fp_ListCerts  ListCerts     = nullptr;
    fp_GetInfo    GetCertInfo   = nullptr;
    fp_ReadFile   ReadCertFile  = nullptr;
    fp_ListDNS    ListDNSProviders = nullptr;
    fp_StartSrv   StartServer   = nullptr;
    fp_StopSrv    StopServer    = nullptr;
    fp_Version    Version       = nullptr;

    bool load(const std::string& libPath) {
        hLib = CM_DLOPEN(libPath.c_str());
        if (!hLib) return false;
        auto sym = [&](const char* n) { return CM_DLSYM(hLib, n); };
        Init           = (fp_Init)      sym("CertManager_Init");
        Free           = (fp_Free)      sym("CertManager_Free");
        RegisterAccount= (fp_RegAccount)sym("CertManager_RegisterAccount");
        ListAccounts   = (fp_ListAcct)  sym("CertManager_ListAccounts");
        ObtainCert     = (fp_Obtain)    sym("CertManager_ObtainCert");
        RenewCert      = (fp_Renew)     sym("CertManager_RenewCert");
        RevokeCert     = (fp_Revoke)    sym("CertManager_RevokeCert");
        ListCerts      = (fp_ListCerts) sym("CertManager_ListCerts");
        GetCertInfo    = (fp_GetInfo)   sym("CertManager_GetCertInfo");
        ReadCertFile   = (fp_ReadFile)  sym("CertManager_ReadCertFile");
        ListDNSProviders=(fp_ListDNS)   sym("CertManager_ListDNSProviders");
        StartServer    = (fp_StartSrv)  sym("CertManager_StartServer");
        StopServer     = (fp_StopSrv)   sym("CertManager_StopServer");
        Version        = (fp_Version)   sym("CertManager_Version");
        return Init && Free && ObtainCert && RenewCert && ListCerts && ReadCertFile;
    }
    void unload() { if (hLib) { CM_DLCLOSE(hLib); hLib = nullptr; } }
    bool loaded() const { return hLib != nullptr; }

    // ── RAII 释放 DLL 返回的字符串 ───────────────────────────────────────
    struct AutoStr {
        char* p = nullptr;
        CertManagerDriver* drv = nullptr;
        ~AutoStr() { if (p && drv && drv->Free) drv->Free(p); }
        const char* c_str() const { return p ? p : ""; }
    };

    // ── 解析统一 JSON 包装 {"success":bool,"data":...,"error":"..."} ─────
    static bool parseResult(const std::string& json,
                            Json::Value& dataOut, std::string& errOut) {
        Json::Value root; Json::CharReaderBuilder rb; std::string e;
        std::istringstream ss(json);
        if (!Json::parseFromStream(rb, ss, &root, &e)) {
            errOut = "JSON parse error: " + e; return false;
        }
        if (!root.get("success", false).asBool()) {
            errOut = root.get("error", "unknown error").asString(); return false;
        }
        dataOut = root["data"];
        return true;
    }
};

// ── CertManagerAcme：使用 certmanager DLL 的完整 ACME 管理器 ──────────────
class CertManagerAcme {
public:
    struct Config {
        bool        enabled            = false;
        std::string certmanagerLib     = "certmanager.dll"; // Windows; Linux 改 .so
        std::string legoPath           = ".lego";
        std::string caServer;           // 空 = Let's Encrypt production
        std::vector<std::string> domains;
        std::string email;
        std::string dnsProvider;        // 如 "alidns", "cloudflare", "tencentcloud"
        std::string envVarsJson;        // DNS 凭证 JSON: {"KEY":"val",...}
        std::string keyType            = "EC256";
        bool        staging            = false;
        int         renewDaysBefore    = 30;
        int         checkIntervalHours = 24;
        std::string certOut            = "nginx/ssl/server.crt";
        std::string keyOut             = "nginx/ssl/server.key";
        bool        reloadNginxAfter   = true;
        std::string webUiAddr;          // 空 = 不启动 Web UI，如 ":19090"

        static Config fromJson(const Json::Value& c) {
            Config r;
            r.enabled          = c.get("enabled", false).asBool();
            r.certmanagerLib   = c.get("certmanager_lib",
#ifdef _WIN32
                                       "certmanager.dll"
#elif defined(__APPLE__)
                                       "libcertmanager.dylib"
#else
                                       "libcertmanager.so"
#endif
                                 ).asString();
            r.legoPath         = c.get("lego_path",    ".lego").asString();
            r.caServer         = c.get("ca_server",    "").asString();
            if (c.isMember("domains"))
                for (auto& d : c["domains"]) r.domains.push_back(d.asString());
            r.email            = c.get("email",        "").asString();
            r.dnsProvider      = c.get("dns_provider", "").asString();
            // env_vars: JSON 对象 → 序列化为字符串传给 DLL
            if (c.isMember("env_vars") && c["env_vars"].isObject()) {
                Json::StreamWriterBuilder wb; wb["indentation"] = "";
                r.envVarsJson  = Json::writeString(wb, c["env_vars"]);
            }
            r.keyType          = c.get("key_type",     "EC256").asString();
            r.staging          = c.get("staging",      false).asBool();
            r.renewDaysBefore  = c.get("renew_days_before",    30).asInt();
            r.checkIntervalHours=c.get("check_interval_hours", 24).asInt();
            r.certOut          = c.get("cert_out", "nginx/ssl/server.crt").asString();
            r.keyOut           = c.get("key_out",  "nginx/ssl/server.key").asString();
            r.reloadNginxAfter = c.get("reload_nginx_after", true).asBool();
            r.webUiAddr        = c.get("web_ui_addr", "").asString();
            return r;
        }
    };

    static CertManagerAcme& instance() {
        static CertManagerAcme inst; return inst;
    }

    bool start(const Config& cfg) {
        std::lock_guard<std::mutex> lk(mu_);
        if (running_.load()) return true;
        cfg_ = cfg;
        if (!cfg_.enabled) { LOG_INFO << "[ACME] certmanager disabled"; return false; }
        if (cfg_.domains.empty() || cfg_.email.empty() || cfg_.dnsProvider.empty()) {
            LOG_WARN << "[ACME] certmanager: domains/email/dns_provider required";
            return false;
        }

        // 加载 DLL / SO，文件不存在时静默跳过
        if (!std::filesystem::exists(cfg_.certmanagerLib)) {
            LOG_INFO << "[ACME] certmanager lib not found, skipping: " << cfg_.certmanagerLib;
            return false;
        }
        if (!drv_.load(cfg_.certmanagerLib)) {
            LOG_WARN << "[ACME] certmanager lib load failed: " << cfg_.certmanagerLib;
            return false;
        }

        // 打印版本
        if (drv_.Version) {
            char* ver = drv_.Version();
            LOG_INFO << "[ACME] certmanager lib version: " << (ver ? ver : "?");
            // Version 返回的是普通 C.CString，不走 Free
            if (ver) { /* Go runtime freed or static — 不调用 Free */ }
        }

        // 初始化
        std::string caServer = cfg_.staging
            ? "https://acme-staging-v02.api.letsencrypt.org/directory"
            : cfg_.caServer;
        {
            CertManagerDriver::AutoStr r; r.drv = &drv_;
            r.p = drv_.Init(cfg_.legoPath.c_str(), caServer.c_str());
            std::string err; Json::Value data;
            if (!CertManagerDriver::parseResult(r.c_str(), data, err))
                LOG_WARN << "[ACME] certmanager init warn: " << err;
            else
                LOG_INFO << "[ACME] certmanager init ok, path=" << cfg_.legoPath;
        }

        // 可选：启动 Web UI
        if (!cfg_.webUiAddr.empty() && drv_.StartServer) {
            CertManagerDriver::AutoStr r; r.drv = &drv_;
            r.p = drv_.StartServer(cfg_.webUiAddr.c_str());
            std::string err; Json::Value data;
            if (CertManagerDriver::parseResult(r.c_str(), data, err))
                LOG_INFO << "[ACME] certmanager Web UI started at http://localhost" << cfg_.webUiAddr;
            else
                LOG_WARN << "[ACME] certmanager Web UI start failed: " << err;
        }

        stopRequested_.store(false);
        running_.store(true);
        worker_ = std::thread([this]() { schedulerLoop(); });
        LOG_INFO << "[ACME] certmanager scheduler started"
                 << " domains=" << cfg_.domains.size()
                 << " provider=" << cfg_.dnsProvider
                 << " renew_before=" << cfg_.renewDaysBefore << "d";
        return true;
    }

    void stop() {
        stopRequested_.store(true);
        if (worker_.joinable()) worker_.join();
        if (drv_.loaded() && !cfg_.webUiAddr.empty() && drv_.StopServer) {
            CertManagerDriver::AutoStr r; r.drv = &drv_;
            r.p = drv_.StopServer();
        }
        drv_.unload();
        running_.store(false);
    }

    bool checkAndRenewOnce() {
        std::lock_guard<std::mutex> lk(mu_);
        const std::string primaryDomain = cfg_.domains.front();

        // ── 查询现有证书 ────────────────────────────────────────────────
        int daysLeft = -1;
        std::string existingCertId;
        {
            CertManagerDriver::AutoStr r; r.drv = &drv_;
            r.p = drv_.ListCerts();
            std::string err; Json::Value data;
            if (CertManagerDriver::parseResult(r.c_str(), data, err) && data.isArray()) {
                for (auto& c : data) {
                    // 匹配主域名
                    bool match = false;
                    for (auto& d : c["domains"])
                        if (d.asString() == primaryDomain) { match = true; break; }
                    if (match) {
                        daysLeft = c.get("daysLeft", -1).asInt();
                        existingCertId = c.get("id", "").asString();
                        break;
                    }
                }
            }
        }

        bool needRenew = (daysLeft < 0) || (daysLeft <= cfg_.renewDaysBefore);
        if (!needRenew) {
            LOG_INFO << "[ACME] cert valid, " << daysLeft << " days left (threshold="
                     << cfg_.renewDaysBefore << ")";
            return true;
        }

        if (daysLeft >= 0)
            LOG_WARN << "[ACME] cert expires in " << daysLeft << " days, renewing";
        else
            LOG_INFO << "[ACME] no cert found for " << primaryDomain << ", obtaining";

        // ── 申请或续期 ──────────────────────────────────────────────────
        std::string newCertId;
        {
            CertManagerDriver::AutoStr r; r.drv = &drv_;
            if (!existingCertId.empty() && daysLeft >= 0) {
                r.p = drv_.RenewCert(existingCertId.c_str(), 0);
            } else {
                // 构建逗号分隔的域名列表
                std::string domainList;
                for (size_t i = 0; i < cfg_.domains.size(); ++i) {
                    if (i) domainList += ",";
                    domainList += cfg_.domains[i];
                }
                r.p = drv_.ObtainCert(
                    cfg_.email.c_str(),
                    domainList.c_str(),
                    cfg_.dnsProvider.c_str(),
                    cfg_.envVarsJson.c_str(),
                    cfg_.keyType.c_str(),
                    0
                );
            }
            std::string err; Json::Value data;
            if (!CertManagerDriver::parseResult(r.c_str(), data, err)) {
                LOG_ERROR << "[ACME] cert operation failed: " << err;
                return false;
            }
            newCertId = data.get("id", existingCertId).asString();
            LOG_INFO << "[ACME] cert ok, id=" << newCertId;
        }

        // ── 写出证书文件 ────────────────────────────────────────────────
        return writeCertFiles(newCertId.empty() ? primaryDomain : newCertId);
    }

    bool isRunning() const { return running_.load(); }

    // ── 读取 DLL 内证书信息（供 SslCertCtrl 等调用）─────────────────────
    Json::Value getCertInfoJson(const std::string& certId) {
        if (!drv_.loaded() || !drv_.GetCertInfo) return Json::Value();
        std::lock_guard<std::mutex> lk(mu_);
        CertManagerDriver::AutoStr r; r.drv = &drv_;
        r.p = drv_.GetCertInfo(certId.c_str());
        std::string err; Json::Value data;
        if (!CertManagerDriver::parseResult(r.c_str(), data, err)) return Json::Value();
        return data;
    }

    Json::Value listCertsJson() {
        if (!drv_.loaded() || !drv_.ListCerts) return Json::Value(Json::arrayValue);
        std::lock_guard<std::mutex> lk(mu_);
        CertManagerDriver::AutoStr r; r.drv = &drv_;
        r.p = drv_.ListCerts();
        std::string err; Json::Value data;
        if (!CertManagerDriver::parseResult(r.c_str(), data, err)) return Json::Value(Json::arrayValue);
        return data;
    }

    // 供控制器调用：直接申请（不经过调度线程）
    Json::Value obtainCertJson(const std::string& email, const std::string& domainList,
                               const std::string& dnsProvider, const std::string& envVarsJson,
                               const std::string& keyType, int bundle) {
        if (!drv_.loaded() || !drv_.ObtainCert) return Json::Value();
        std::lock_guard<std::mutex> lk(mu_);
        CertManagerDriver::AutoStr r; r.drv = &drv_;
        r.p = drv_.ObtainCert(email.c_str(), domainList.c_str(), dnsProvider.c_str(),
                               envVarsJson.c_str(), keyType.c_str(), bundle);
        std::string err; Json::Value data;
        Json::Value root;
        Json::CharReaderBuilder rb; std::istringstream ss(r.c_str()); std::string e;
        if (Json::parseFromStream(rb, ss, &root, &e)) return root;
        return Json::Value();
    }

    // 供控制器调用：直接续期
    Json::Value renewCertJson(const std::string& certId, int bundle) {
        if (!drv_.loaded() || !drv_.RenewCert) return Json::Value();
        std::lock_guard<std::mutex> lk(mu_);
        CertManagerDriver::AutoStr r; r.drv = &drv_;
        r.p = drv_.RenewCert(certId.c_str(), bundle);
        Json::Value root; Json::CharReaderBuilder rb;
        std::istringstream ss(r.c_str()); std::string e;
        if (Json::parseFromStream(rb, ss, &root, &e)) return root;
        return Json::Value();
    }

    // 供控制器调用：吊销
    Json::Value revokeCertJson(const std::string& certId) {
        if (!drv_.loaded() || !drv_.RevokeCert) return Json::Value();
        std::lock_guard<std::mutex> lk(mu_);
        CertManagerDriver::AutoStr r; r.drv = &drv_;
        r.p = drv_.RevokeCert(certId.c_str());
        Json::Value root; Json::CharReaderBuilder rb;
        std::istringstream ss(r.c_str()); std::string e;
        if (Json::parseFromStream(rb, ss, &root, &e)) return root;
        return Json::Value();
    }

    // 供控制器调用：列出 DNS 提供商
    Json::Value listDNSProvidersJson() {
        if (!drv_.loaded() || !drv_.ListDNSProviders) return Json::Value(Json::arrayValue);
        std::lock_guard<std::mutex> lk(mu_);
        CertManagerDriver::AutoStr r; r.drv = &drv_;
        r.p = drv_.ListDNSProviders();
        std::string err; Json::Value data;
        if (!CertManagerDriver::parseResult(r.c_str(), data, err)) return Json::Value(Json::arrayValue);
        return data;
    }

    // 供控制器调用：版本字符串
    std::string version() {
        if (!drv_.loaded() || !drv_.Version) return "";
        char* v = drv_.Version();
        return v ? std::string(v) : "";
    }

private:
    CertManagerAcme()  = default;
    ~CertManagerAcme() { stop(); }
    CertManagerAcme(const CertManagerAcme&) = delete;
    CertManagerAcme& operator=(const CertManagerAcme&) = delete;

    void schedulerLoop() {
        try { checkAndRenewOnce(); } catch (const std::exception& e) {
            LOG_WARN << "[ACME] initial check error: " << e.what();
        }
        while (!stopRequested_.load()) {
            for (int i = 0; i < 60 * cfg_.checkIntervalHours && !stopRequested_.load(); ++i)
                std::this_thread::sleep_for(std::chrono::minutes(1));
            if (stopRequested_.load()) break;
            try { checkAndRenewOnce(); } catch (const std::exception& e) {
                LOG_WARN << "[ACME] renew check error: " << e.what();
            }
        }
        LOG_INFO << "[ACME] certmanager scheduler stopped";
    }

    bool writeCertFiles(const std::string& certId) {
        // 读取证书内容
        auto readFile = [&](const char* type, const std::string& outPath) -> bool {
            CertManagerDriver::AutoStr r; r.drv = &drv_;
            r.p = drv_.ReadCertFile(certId.c_str(), type);
            std::string err; Json::Value data;
            if (!CertManagerDriver::parseResult(r.c_str(), data, err)) {
                LOG_ERROR << "[ACME] read " << type << " failed: " << err;
                return false;
            }
            std::string content = data.isString() ? data.asString() : data.toStyledString();
            // 确保目录存在
            std::error_code ec;
            std::filesystem::create_directories(
                std::filesystem::path(outPath).parent_path(), ec);
            std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
            if (!ofs) { LOG_ERROR << "[ACME] write failed: " << outPath; return false; }
            ofs.write(content.data(), (std::streamsize)content.size());
            LOG_INFO << "[ACME] wrote " << outPath << " (" << content.size() << " bytes)";
            return true;
        };

        if (!readFile("cert", cfg_.certOut)) return false;
        if (!readFile("key",  cfg_.keyOut))  return false;

        // 触发 Nginx reload
        if (cfg_.reloadNginxAfter && NginxEmbedded::instance().isRunning()) {
            LOG_INFO << "[ACME] triggering nginx reload";
            NginxEmbedded::instance().reload();
        }
        return true;
    }

    Config             cfg_;
    CertManagerDriver  drv_;
    std::atomic<bool>  running_{false};
    std::atomic<bool>  stopRequested_{false};
    std::thread        worker_;
    std::mutex         mu_;
};
