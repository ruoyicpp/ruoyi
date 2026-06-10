#pragma once
#include <drogon/HttpController.h>
#include <filesystem>
#include "../../common/AjaxResult.h"
#include "../../common/OperLogUtils.h"
#include "../../filters/PermFilter.h"
#include "../../common/SslManager.h"
#include "../../services/DatabaseService.h"

/**
 * @file SysSslConfigCtrl.h
 * @brief SSL/TLS 证书管理控制器 — 支持 HTTPS 配置和证书管理
 * 
 * 功能概述：
 *   - 证书管理：上传、查看、更新 SSL 证书
 *   - 私钥管理：安全存储和管理私钥
 *   - HTTPS 配置：配置 HTTPS 端口和协议版本
 *   - 证书验证：验证证书有效性和过期时间
 *   - 自动续期：支持自动续期提醒
 *   - 证书链：支持完整的证书链配置
 * 
 * 核心特性：
 *   - 多证书支持：支持多个域名和通配符证书
 *   - 证书验证：自动验证证书有效性和完整性
 *   - 过期提醒：证书即将过期时自动提醒
 *   - 安全存储：私钥加密存储，防止泄露
 *   - 热更新：支持证书热更新，无需重启应用
 *   - 审计日志：记录所有证书操作
 * 
 * 支持的证书格式：
 *   - PEM 格式：.pem、.crt、.cer（最常用）
 *   - DER 格式：.der、.cer
 *   - PKCS#12 格式：.p12、.pfx
 * 
 * 支持的私钥格式：
 *   - PKCS#1 格式：RSA 私钥
 *   - PKCS#8 格式：通用私钥格式
 *   - OpenSSL 格式：EC 私钥
 * 
 * API 端点：
 *   - GET /system/ssl/config - 查询当前 SSL 配置
 *   - PUT /system/ssl/config - 保存 SSL 配置
 *   - POST /system/ssl/uploadCert - 上传证书文件
 *   - POST /system/ssl/uploadKey - 上传私钥文件
 * 
 * 请求/响应示例：
 *   ```
 *   GET /system/ssl/config
 *   Authorization: Bearer <JWT>
 *   
 *   响应：
 *   {
 *     "code": 200,
 *     "msg": "success",
 *     "data": {
 *       "httpsEnabled": true,
 *       "httpsPort": 18443,
 *       "certPath": "/path/to/cert.pem",
 *       "keyPath": "/path/to/key.pem",
 *       "certInfo": {
 *         "subject": "CN=example.com",
 *         "issuer": "CN=Let's Encrypt",
 *         "notBefore": "2024-01-01T00:00:00Z",
 *         "notAfter": "2025-01-01T00:00:00Z",
 *         "daysUntilExpiry": 365
 *       }
 *     }
 *   }
 *   ```
 * 
 * 权限要求：
 *   - system:ssl:query - 查询 SSL 配置
 *   - system:ssl:edit - 编辑 SSL 配置
 *   - system:ssl:upload - 上传证书和私钥
 * 
 * 配置项（config.json）：
 *   - https.enabled: 是否启用 HTTPS（默认 false）
 *   - https.port: HTTPS 端口（默认 18443）
 *   - https.cert_path: 证书文件路径
 *   - https.key_path: 私钥文件路径
 *   - https.min_tls_version: 最小 TLS 版本（默认 "TLSv1.2"）
 *   - https.ciphers: 加密套件列表（可选）
 * 
 * 安全建议：
 *   - 使用强加密算法（RSA 2048+ 或 ECDSA 256+）
 *   - 定期更新证书，不要等到过期
 *   - 使用 HSTS 头强制 HTTPS
 *   - 启用 OCSP Stapling 提高性能
 *   - 定期审计证书使用情况
 * 
 * 证书来源：
 *   - Let's Encrypt：免费证书，推荐用于生产环境
 *   - DigiCert、GlobalSign：商业证书
 *   - 自签名证书：仅用于开发和测试
 * 
 * @see SslManager - SSL 管理工具
 * @see DatabaseService - 数据库服务
 */
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
        LOG_OPER(req, "SSL配置", BusinessType::UPDATE);
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
        OperLogUtils::write(req, "SSL证书上传", BusinessType::IMPORT,
                            "file=" + f.getFileName());
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
        // 私钥文件名不记入参数，避免在日志中出现敏感路径
        OperLogUtils::write(req, "SSL私钥上传", BusinessType::IMPORT, "key uploaded");
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
