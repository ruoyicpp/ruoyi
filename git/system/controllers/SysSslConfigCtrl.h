#pragma once
#include <drogon/HttpController.h>
#include <filesystem>
#include "../../common/AjaxResult.h"
#include "../../filters/PermFilter.h"
#include "../../common/SslManager.h"
#include "../../services/DatabaseService.h"

// SSL 证书与 HTTPS 配置管理
// GET  /system/ssl/config      查询当前配置
// PUT  /system/ssl/config      保存配置（重启生效）
// POST /system/ssl/uploadCert  上传证书文件 (.pem/.crt/.cer)
// POST /system/ssl/uploadKey   上传私钥文件 (.pem/.key)
class SysSslConfigCtrl : public drogon::HttpController<SysSslConfigCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SysSslConfigCtrl::getConfig,  "/system/ssl/config",     drogon::Get,  "JwtAuthFilter");
        ADD_METHOD_TO(SysSslConfigCtrl::saveConfig, "/system/ssl/config",     drogon::Put,  "JwtAuthFilter");
        ADD_METHOD_TO(SysSslConfigCtrl::uploadCert, "/system/ssl/uploadCert", drogon::Post, "JwtAuthFilter");
        ADD_METHOD_TO(SysSslConfigCtrl::uploadKey,  "/system/ssl/uploadKey",  drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    // ── 查询当前 SSL 配置 ───────────────────────────────────────────────────
    void getConfig(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:ssl:query");
        auto cfg = SslManager::loadConfig();
        auto& db = DatabaseService::instance();
        ensureTable(db);
        // 从 DB 读取证书存在状态（比磁盘更可靠，磁盘文件可能被误删）
        bool certInDb = dbHasKey(db, "cert_pem");
        bool keyInDb  = dbHasKey(db, "key_pem");
        Json::Value data;
        data["enabled"]     = cfg.enabled;
        data["httpsPort"]   = cfg.httpsPort;
        data["httpPort"]    = cfg.httpPort;
        data["forceHttps"]  = cfg.forceHttps;
        data["certExists"]  = SslManager::certExists() || certInDb;
        data["certInDb"]    = certInDb;
        data["keyInDb"]     = keyInDb;
        // 前端预览：证书前两行（不含私钥）
        if (SslManager::certExists()) {
            std::string cert = SslManager::readCert();
            auto nl = cert.find('\n');
            nl = (nl != std::string::npos) ? cert.find('\n', nl + 1) : nl;
            data["certPreview"] = cert.substr(0, nl == std::string::npos ? 80 : nl);
        } else {
            data["certPreview"] = "";
        }
        RESP_OK(cb, data);
    }

    // ── 保存 SSL 配置（重启后端后生效）─────────────────────────────────────
    void saveConfig(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:ssl:edit");
        auto b = req->getJsonObject();
        if (!b) { RESP_ERR(cb, "参数错误"); return; }

        SslManager::Config cfg;
        cfg.enabled    = (*b).get("enabled",    false).asBool();
        cfg.httpsPort  = (*b).get("httpsPort",  18443).asInt();
        cfg.httpPort   = (*b).get("httpPort",   18080).asInt();
        cfg.forceHttps = (*b).get("forceHttps", false).asBool();

        if (cfg.httpsPort < 1 || cfg.httpsPort > 65535 ||
            cfg.httpPort  < 1 || cfg.httpPort  > 65535 ||
            cfg.httpsPort == cfg.httpPort) {
            RESP_ERR(cb, "端口号无效（1-65535，HTTP 与 HTTPS 端口不能相同）");
            return;
        }
        if (cfg.enabled && !SslManager::certExists()) {
            RESP_ERR(cb, "请先上传证书和私钥文件，再启用 HTTPS");
            return;
        }
        if (!SslManager::saveConfig(cfg)) {
            RESP_ERR(cb, "配置写入失败，请检查 ssl/ 目录权限");
            return;
        }
        // 同步保存到 DB（用于多实例/磁盘丢失后恢复）
        auto& db = DatabaseService::instance();
        ensureTable(db);
        dbUpsert(db, "ssl_enabled",    cfg.enabled    ? "1" : "0");
        dbUpsert(db, "ssl_https_port", std::to_string(cfg.httpsPort));
        dbUpsert(db, "ssl_http_port",  std::to_string(cfg.httpPort));
        dbUpsert(db, "ssl_force_https",cfg.forceHttps ? "1" : "0");
        std::string msg = cfg.enabled
            ? "HTTPS 配置已保存，重启后端后将在端口 "
              + std::to_string(cfg.httpsPort) + " 启用 HTTPS"
              + (cfg.forceHttps ? "，并强制重定向 HTTP→HTTPS" : "")
            : "配置已保存（HTTPS 未启用）";
        RESP_MSG(cb, msg);
    }

    // ── 上传 SSL 证书文件（PEM 格式 .pem / .crt / .cer）─────────────────────
    void uploadCert(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:ssl:edit");
        drogon::MultiPartParser parser;
        if (parser.parse(req) != 0) { RESP_ERR(cb, "解析上传内容失败"); return; }
        auto& files = parser.getFiles();
        if (files.empty()) { RESP_ERR(cb, "未上传文件"); return; }

        auto& f = files[0];
        std::string ext = std::filesystem::path(f.getFileName()).extension().string();
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext != ".pem" && ext != ".crt" && ext != ".cer") {
            RESP_ERR(cb, "仅支持 .pem / .crt / .cer 格式的证书文件"); return;
        }
        if (f.fileLength() > 64 * 1024) {
            RESP_ERR(cb, "证书文件不应超过 64KB"); return;
        }
        std::string pem(f.fileContent());
        if (pem.find("-----BEGIN") == std::string::npos) {
            RESP_ERR(cb, "文件不是有效的 PEM 格式，请检查证书内容"); return;
        }
        if (pem.find("PRIVATE KEY") != std::string::npos) {
            RESP_ERR(cb, "此接口用于上传证书，请勿上传私钥文件"); return;
        }
        if (!SslManager::writeCert(pem)) {
            RESP_ERR(cb, "证书写入失败，请检查磁盘权限"); return;
        }
        // 同步存入 DB
        auto& db = DatabaseService::instance();
        ensureTable(db);
        dbUpsert(db, "cert_pem", pem);
        RESP_MSG(cb, "证书上传成功（" + f.getFileName() + "）");
    }

    // ── 上传 SSL 私钥文件（PEM 格式 .pem / .key）─────────────────────────────
    void uploadKey(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        CHECK_PERM(req, cb, "system:ssl:edit");
        drogon::MultiPartParser parser;
        if (parser.parse(req) != 0) { RESP_ERR(cb, "解析上传内容失败"); return; }
        auto& files = parser.getFiles();
        if (files.empty()) { RESP_ERR(cb, "未上传文件"); return; }

        auto& f = files[0];
        std::string ext = std::filesystem::path(f.getFileName()).extension().string();
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext != ".pem" && ext != ".key") {
            RESP_ERR(cb, "仅支持 .pem / .key 格式的私钥文件"); return;
        }
        if (f.fileLength() > 64 * 1024) {
            RESP_ERR(cb, "私钥文件不应超过 64KB"); return;
        }
        std::string pem(f.fileContent());
        if (pem.find("-----BEGIN") == std::string::npos) {
            RESP_ERR(cb, "文件不是有效的 PEM 格式，请检查私钥内容"); return;
        }
        if (pem.find("PRIVATE KEY") == std::string::npos) {
            RESP_ERR(cb, "文件不包含私钥（PRIVATE KEY）块，请检查文件内容"); return;
        }
        if (!SslManager::writeKey(pem)) {
            RESP_ERR(cb, "私钥写入失败，请检查磁盘权限"); return;
        }
        // 同步存入 DB（存储加密后的私钥，仅服务器读取）
        auto& db = DatabaseService::instance();
        ensureTable(db);
        dbUpsert(db, "key_pem", pem);
        RESP_MSG(cb, "私钥上传成功");
    }

private:
    // ── sys_ssl_cert 表操作（懒建表，cert_key PRIMARY KEY）────────────────
    static void ensureTable(DatabaseService& db) {
        db.exec(
            "CREATE TABLE IF NOT EXISTS sys_ssl_cert ("
            "  cert_key   VARCHAR(50) PRIMARY KEY,"
            "  cert_val   TEXT        NOT NULL DEFAULT '',"
            "  update_time TIMESTAMP  DEFAULT NOW()"
            ")");
    }

    // upsert：先删后插（兼容 PG 和 SQLite）
    static void dbUpsert(DatabaseService& db,
                         const std::string& key, const std::string& val) {
        db.execParams("DELETE FROM sys_ssl_cert WHERE cert_key=$1", {key});
        db.execParams(
            "INSERT INTO sys_ssl_cert(cert_key,cert_val,update_time) VALUES($1,$2,NOW())",
            {key, val});
    }

    // 检查 key 是否存在且非空
    static bool dbHasKey(DatabaseService& db, const std::string& key) {
        auto res = db.queryParams(
            "SELECT 1 FROM sys_ssl_cert WHERE cert_key=$1 AND cert_val<>''",
            {key});
        return res.ok() && res.rows() > 0;
    }

    // 读取值
    static std::string dbGet(DatabaseService& db, const std::string& key) {
        auto res = db.queryParams(
            "SELECT cert_val FROM sys_ssl_cert WHERE cert_key=$1", {key});
        if (res.ok() && res.rows() > 0) return res.str(0, 0);
        return {};
    }
};
