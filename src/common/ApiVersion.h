/**
 * @file ApiVersion.h
 * @brief API 版本控制、CSRF 防护、安全响应头、请求签名验证
 * 
 * 功能概述：
 *   - API 版本控制：支持多版本 API 并存，平滑升级
 *   - CSRF 防护：生成和验证 CSRF Token，防止跨站请求伪造
 *   - 安全响应头：添加安全相关的 HTTP 响应头
 *   - 请求签名验证：验证请求的合法性和完整性
 * 
 * 核心特性：
 *   - 版本管理：支持主版本和次版本，自动降级兼容
 *   - 废弃策略：支持版本生命周期管理（活跃、维护、废弃、移除）
 *   - 多种版本识别：URL 路径、HTTP Header、Content-Type
 *   - 完整的安全防护：CSRF、XSS、CORS、CSP 等
 *   - 请求签名：支持 HMAC-SHA256/SHA512 签名验证
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <drogon/drogon.h>

/**
 * @namespace api_version
 * @brief API 版本控制命名空间
 */
namespace api_version {

/**
 * @struct Version
 * @brief API 版本信息结构体
 * 
 * 支持主版本和次版本，如 v1、v2.1 等。
 */
struct Version {
    int major;                  ///< 主版本号
    int minor;                  ///< 次版本号
    std::string full;           ///< 完整版本字符串（如 "v1", "v2.1"）

    /// @brief 版本比较运算符
    bool operator<(const Version& o) const {
        if (major != o.major) return major < o.major;
        return minor < o.minor;
    }
    bool operator==(const Version& o) const { return major == o.major && minor == o.minor; }
    bool operator!=(const Version& o) const { return !(*this == o); }
    bool operator>(const Version& o) const { return o < *this; }
    bool operator>=(const Version& o) const { return !(*this < o); }
    bool operator<=(const Version& o) const { return !(o < *this); }
};

inline Version parseVersion(const std::string& s) {
    Version v{1, 0, s};
    std::string str = s;
    // 移除前导 'v'
    if (!str.empty() && (str[0] == 'v' || str[0] == 'V')) {
        str = str.substr(1);
    }
    // 查找第一个 '.' 或结束
    size_t dot = str.find('.');
    std::string majorStr, minorStr;
    if (dot == std::string::npos) {
        majorStr = str;
        minorStr = "";
    } else {
        majorStr = str.substr(0, dot);
        minorStr = str.substr(dot + 1);
    }
    // 安全转换为数字
    try {
        if (!majorStr.empty()) {
            v.major = std::stoi(majorStr);
        }
        if (!minorStr.empty()) {
            v.minor = std::stoi(minorStr);
        }
    } catch (...) {
        v.major = 1;
        v.minor = 0;
    }
    v.full = "v" + std::to_string(v.major) + (v.minor > 0 ? "." + std::to_string(v.minor) : "");
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// API 版本策略
// ─────────────────────────────────────────────────────────────────────────────
enum class DeprecationPolicy {
    ACTIVE,      // 当前活跃版本
    MAINTAINED,  // 维护中（只修bug）
    DEPRECATED,  // 已废弃（建议迁移）
    REMOVED      // 已移除
};

struct VersionInfo {
    Version version;
    DeprecationPolicy policy = DeprecationPolicy::ACTIVE;
    std::string sunsetDate;        // 废弃日期
    std::string migrationGuide;    // 迁移指南URL
    std::string since;            // 首次发布版本

    bool isActive() const { return policy == DeprecationPolicy::ACTIVE; }
    bool shouldWarn() const {
        return policy == DeprecationPolicy::MAINTAINED || policy == DeprecationPolicy::DEPRECATED;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// 版本管理器
// ─────────────────────────────────────────────────────────────────────────────
class VersionManager {
public:
    static VersionManager& instance() {
        static VersionManager mgr;
        return mgr;
    }

    // 注册API版本
    void registerVersion(const VersionInfo& info);

    // 获取所有版本（按优先级排序）
    std::vector<VersionInfo> allVersions() const;

    // 获取当前最新活跃版本
    Version latestVersion() const;

    // 检查版本是否存在
    bool hasVersion(const Version& v) const;

    // 获取版本信息
    const VersionInfo* getVersionInfo(const Version& v) const;

    // 获取客户端请求的版本（从URL或Header解析）
    Version parseRequestVersion(const drogon::HttpRequestPtr& req) const;

    // 获取降级后的版本（如果请求的版本不可用）
    Version getCompatibleVersion(const Version& requested) const;

    // 添加自定义版本路由处理器
    void addRouteHandler(const std::string& path, const std::string& method,
                        const std::function<void(const drogon::HttpRequestPtr&,
                                                std::function<void(const drogon::HttpResponsePtr&)>&,
                                                const Version&)>& handler);

private:
    VersionManager() = default;

    std::map<Version, VersionInfo> versions_;
    std::vector<std::pair<std::string, std::string>> routeHandlers_; // path, method -> (handler)
    Version latest_{1, 0, "v1"};
};

// ─────────────────────────────────────────────────────────────────────────────
// 版本控制中间件
// ─────────────────────────────────────────────────────────────────────────────
class ApiVersionFilter : public drogon::HttpFilter<ApiVersionFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr &req,
                  drogon::FilterCallback &&fcb,
                  drogon::FilterChainCallback &&fccb) override;

    // 设置允许的最小版本
    void setMinVersion(const Version& v) { minVersion_ = v; }

    // 设置默认版本（未指定版本时）
    void setDefaultVersion(const Version& v) { defaultVersion_ = v; }

private:
    Version minVersion_{1, 0, "v1"};
    Version defaultVersion_{1, 0, "v1"};
};

// ─────────────────────────────────────────────────────────────────────────────
// 便捷函数
// ─────────────────────────────────────────────────────────────────────────────
inline bool isLatestVersion(const std::string& v) {
    auto latest = VersionManager::instance().latestVersion();
    return parseVersion(v) == latest;
}

inline bool isDeprecated(const std::string& v) {
    auto info = VersionManager::instance().getVersionInfo(parseVersion(v));
    return info && (info->policy == DeprecationPolicy::DEPRECATED ||
                   info->policy == DeprecationPolicy::REMOVED);
}

// 添加版本相关响应头
inline void addVersionHeaders(drogon::HttpResponsePtr& resp, const Version& v) {
    resp->addHeader("API-Version", v.full);
    resp->addHeader("API-Version-Major", std::to_string(v.major));

    auto info = VersionManager::instance().getVersionInfo(v);
    if (info) {
        if (info->shouldWarn()) {
            resp->addHeader("Sunset", info->sunsetDate);
            resp->addHeader("Deprecation", "true");
        }
        if (!info->migrationGuide.empty()) {
            resp->addHeader("Migration-Guide", info->migrationGuide);
        }
    }

    // 建议使用最新版本
    auto latest = VersionManager::instance().latestVersion();
    if (v != latest) {
        resp->addHeader("X-API-Latest", latest.full);
    }
}

} // namespace api_version

/**
 * @class CsrfProtection
 * @brief CSRF（跨站请求伪造）防护
 * 
 * 功能概述：
 *   - Token 生成：为每个会话生成唯一的 CSRF Token
 *   - Token 验证：验证请求中的 CSRF Token 是否有效
 *   - 请求检查：自动识别需要 CSRF 验证的请求
 *   - 豁免管理：支持添加不需要 CSRF 验证的路径
 * 
 * 工作流程：
 *   1. 用户登录时，生成 CSRF Token 并通过 Cookie 返回
 *   2. 前端在发送 POST/PUT/DELETE 请求时，需要在 Header 或 Body 中包含 Token
 *   3. 服务器验证 Token 的有效性
 *   4. GET/HEAD/OPTIONS 请求不需要 CSRF 验证
 * 
 * Token 存储位置（优先级）：
 *   1. X-XSRF-TOKEN 请求头
 *   2. _csrf 表单字段
 *   3. Cookie 中的 XSRF-TOKEN
 */
class CsrfProtection {
public:
    /**
     * @brief 获取 CsrfProtection 单例
     * @return CsrfProtection 实例引用
     */
    static CsrfProtection& instance() {
        static CsrfProtection inst;
        return inst;
    }

    /**
     * @brief 为会话生成 CSRF Token
     * 
     * 生成一个加密的随机 Token，并与会话 ID 关联。
     * 
     * @param sessionId 会话 ID
     * @return 生成的 CSRF Token
     */
    std::string generateToken(const std::string& sessionId);

    /**
     * @brief 验证 CSRF Token
     * 
     * 检查给定的 Token 是否与会话 ID 匹配。
     * 
     * @param sessionId 会话 ID
     * @param token 要验证的 Token
     * @return 如果 Token 有效返回 true
     */
    bool validateToken(const std::string& sessionId, const std::string& token);

    /**
     * @brief 验证 HTTP 请求的 CSRF Token
     * 
     * 从请求的 Header 或 Body 中提取 Token，并进行验证。
     * 
     * @param req HTTP 请求对象
     * @param sessionId 会话 ID
     * @return 如果 Token 有效返回 true
     */
    bool validateRequest(const drogon::HttpRequestPtr& req, const std::string& sessionId);

    /**
     * @brief 获取 CSRF Token 的 Cookie 名称
     * @return Cookie 名称（默认 "XSRF-TOKEN"）
     */
    const std::string& cookieName() const { return cookieName_; }

    /**
     * @brief 获取 CSRF Token 的 HTTP Header 名称
     * @return Header 名称（默认 "X-XSRF-TOKEN"）
     */
    const std::string& headerName() const { return headerName_; }

    /**
     * @brief 获取 CSRF Token 的表单字段名称
     * @return 表单字段名称（默认 "_csrf"）
     */
    const std::string& formFieldName() const { return formFieldName_; }

    /**
     * @brief 设置 CSRF Token Cookie 的属性
     * 
     * @param httpOnly 是否设置 HttpOnly 标志（防止 JavaScript 访问）
     * @param secure 是否设置 Secure 标志（仅 HTTPS 传输）
     * @param maxAgeSeconds Cookie 有效期（秒）
     */
    void setCookieConfig(bool httpOnly, bool secure, int maxAgeSeconds);

    /**
     * @brief 检查请求是否需要 CSRF 验证
     * 
     * GET、HEAD、OPTIONS 请求不需要 CSRF 验证。
     * 
     * @param req HTTP 请求对象
     * @return 如果需要验证返回 true
     */
    bool requiresCsrfCheck(const drogon::HttpRequestPtr& req) const;

    /**
     * @brief 添加豁免路径（不需要 CSRF 验证）
     * 
     * 支持通配符模式，如 "/api/webhook/*"。
     * 
     * @param pathPattern 路径模式
     */
    void addExemptPath(const std::string& pathPattern);

    /**
     * @brief 检查路径是否豁免 CSRF 验证
     * 
     * @param path 请求路径
     * @return 如果路径豁免返回 true
     */
    bool isPathExempt(const std::string& path) const;

    /**
     * @brief 创建 CSRF Token 的 Set-Cookie 响应头
     * 
     * @param sessionId 会话 ID
     * @return Set-Cookie 响应头字符串
     */
    std::string createSetCookieHeader(const std::string& sessionId);

private:
    CsrfProtection() = default;

    std::string cookieName_ = "XSRF-TOKEN";
    std::string headerName_ = "X-XSRF-TOKEN";
    std::string formFieldName_ = "_csrf";

    struct CookieConfig {
        bool httpOnly = true;
        bool secure = false;  // 生产环境应设为true
        int maxAge = 86400;   // 24小时
        std::string sameSite = "Strict";
    } cookieConfig_;

    std::unordered_set<std::string> exemptPaths_;
    mutable std::mutex mutex_;

    // Session -> Token 映射
    std::unordered_map<std::string, std::string> tokenStore_;
    mutable std::mutex tokenMutex_;
};

/**
 * @class SecurityHeaders
 * @brief 安全响应头管理
 * 
 * 功能概述：
 *   - 添加安全相关的 HTTP 响应头
 *   - 防止常见的 Web 安全漏洞（XSS、点击劫持、MIME 嗅探等）
 *   - 配置 CORS、HSTS、CSP 等安全策略
 * 
 * 支持的安全头：
 *   - X-Frame-Options：防止点击劫持
 *   - Content-Security-Policy：防止 XSS 和注入攻击
 *   - Strict-Transport-Security：强制 HTTPS
 *   - X-Content-Type-Options：防止 MIME 嗅探
 *   - X-XSS-Protection：启用浏览器 XSS 防护
 *   - Referrer-Policy：控制 Referrer 信息泄露
 *   - CORS 相关头：控制跨域请求
 */
class SecurityHeaders {
public:
    /**
     * @brief 获取 SecurityHeaders 单例
     * @return SecurityHeaders 实例引用
     */
    static SecurityHeaders& instance() {
        static SecurityHeaders inst;
        return inst;
    }

    /**
     * @brief 为 HTTP 响应添加所有配置的安全头
     * 
     * @param resp HTTP 响应对象
     */
    void apply(drogon::HttpResponsePtr& resp) const;

    /// @name 安全策略配置方法
    /// @{

    /**
     * @brief 设置 X-Frame-Options 响应头
     * 
     * 防止网页被嵌入到其他网站的 iframe 中（防止点击劫持）。
     * 
     * @param policy 策略值：
     *   - "DENY"：不允许被任何网站嵌入
     *   - "SAMEORIGIN"：仅允许同源网站嵌入
     *   - "ALLOW-FROM uri"：仅允许指定网站嵌入
     */
    void setFrameOptions(const std::string& policy);

    /**
     * @brief 设置 Content-Security-Policy 响应头
     * 
     * 防止 XSS 和注入攻击，限制资源加载来源。
     * 
     * @param csp CSP 策略字符串，如：
     *   "default-src 'self'; script-src 'self' 'unsafe-inline'"
     */
    void setContentSecurityPolicy(const std::string& csp);

    /**
     * @brief 设置 HSTS（HTTP Strict Transport Security）最大有效期
     * 
     * 强制浏览器使用 HTTPS 连接，防止中间人攻击。
     * 
     * @param seconds 有效期（秒），通常为 31536000（1 年）
     */
    void setHstsMaxAge(int seconds);

    /**
     * @brief 设置 X-XSS-Protection 响应头
     * 
     * 启用浏览器内置的 XSS 防护机制。
     * 
     * @param mode 防护模式：
     *   - "1; mode=block"：检测到 XSS 时阻止页面加载
     *   - "0"：禁用 XSS 防护
     */
    void setXssProtection(const std::string& mode);

    /**
     * @brief 设置 X-Content-Type-Options 响应头
     * 
     * 防止浏览器 MIME 嗅探，强制使用 Content-Type 头指定的类型。
     * 
     * @param nosniff 通常为 "nosniff"
     */
    void setContentTypeOptions(const std::string& nosniff = "nosniff");

    /// @}

    /// @name CORS 配置方法
    /// @{

    /**
     * @brief 设置允许的 CORS 源
     * 
     * @param origins 允许的源列表，如 {"https://example.com", "https://app.example.com"}
     */
    void setCorsOrigins(const std::vector<std::string>& origins);

    /**
     * @brief 设置允许的 CORS 请求方法
     * 
     * @param methods HTTP 方法列表，如 {"GET", "POST", "PUT", "DELETE"}
     */
    void setCorsMethods(const std::vector<std::string>& methods);

    /**
     * @brief 设置 CORS 预检请求的缓存时间
     * 
     * @param seconds 缓存时间（秒），通常为 3600（1 小时）
     */
    void setCorsMaxAge(int seconds);

    /// @}

    /**
     * @brief 设置 Referrer-Policy 响应头
     * 
     * 控制浏览器发送 Referrer 信息的策略。
     * 
     * @param policy 策略值，如 "strict-origin-when-cross-origin"
     */
    void setReferrerPolicy(const std::string& policy);

private:
    SecurityHeaders() = default;

    // Frame Options
    std::string frameOptions_ = "SAMEORIGIN";

    // Content Security Policy
    std::string csp_ =
        "default-src 'self'; "
        "script-src 'self'; "
        "object-src 'none'; "
        "base-uri 'self'; "
        "form-action 'self'; "
        "frame-ancestors 'self'";

    // HSTS
    bool hstsEnabled_ = false;
    int hstsMaxAge_ = 31536000;  // 1年

    // X-Content-Type-Options
    std::string xContentTypeOptions_ = "nosniff";

    // X-XSS-Protection
    std::string xssProtection_ = "1; mode=block";

    // Referrer-Policy
    std::string referrerPolicy_ = "strict-origin-when-cross-origin";

    // CORS
    std::vector<std::string> corsOrigins_;
    std::vector<std::string> corsMethods_ = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
    int corsMaxAge_ = 3600;
};

// ─────────────────────────────────────────────────────────────────────────────
// 统一的请求安全验证中间件
// ─────────────────────────────────────────────────────────────────────────────
class SecurityFilter : public drogon::HttpFilter<SecurityFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr &req,
                  drogon::FilterCallback &&fcb,
                  drogon::FilterChainCallback &&fccb) override;

    // 初始化（从配置）
    static void init(const std::string& configPath);

private:
    static bool initialized_;
};

// ════════════════════════════════════════════════════════════════════════════
// RequestSignature.h — 请求签名验证
// ════════════════════════════════════════════════════════════════════════════

#include <chrono>
#include <unordered_map>

class RequestSignature {
public:
    static RequestSignature& instance() {
        static RequestSignature inst;
        return inst;
    }

    // 签名参数
    struct SignParams {
        std::string appId;           // 应用ID
        std::string secret;          // 密钥
        std::string method;          // HTTP方法
        std::string path;            // 请求路径
        std::string timestamp;       // 时间戳（ISO8601或Unix）
        std::string nonce;           // 随机数
        std::string queryString;     // 查询参数（已排序）
        std::string bodyHash;        // 请求体SHA256
    };

    // 生成签名
    // 生成签名
    static std::string generateSignature(const SignParams& params);
    std::string generateSignatureImpl(const SignParams& params) const;

    // 生成签名头
    static std::string sign(const SignParams& params);

    // 验证签名
    bool validate(
        const drogon::HttpRequestPtr& req,
        const std::string& appId,
        const std::string& secret,
        int timestampToleranceSec = 300  // 5分钟容差
    );

    // 注册应用密钥
    void registerApp(const std::string& appId, const std::string& secret);

    // 移除应用
    void unregisterApp(const std::string& appId);

    // 获取签名头名
    const std::string& signatureHeader() const { return signatureHeader_; }
    const std::string& appIdHeader() const { return appIdHeader_; }
    const std::string& timestampHeader() const { return timestampHeader_; }
    const std::string& nonceHeader() const { return nonceHeader_; }

    // 允许的签名算法
    enum class Algorithm { HmacSha256, HmacSha512 };
    void setAlgorithm(Algorithm alg) { algorithm_ = alg; }

private:
    RequestSignature() = default;

    std::string signatureHeader_ = "X-Signature";
    std::string appIdHeader_ = "X-App-Id";
    std::string timestampHeader_ = "X-Timestamp";
    std::string nonceHeader_ = "X-Nonce";

    Algorithm algorithm_ = Algorithm::HmacSha256;

    // AppId -> Secret
    std::unordered_map<std::string, std::string> appSecrets_;
    std::mutex mutex_;

    // 防重放：已使用的nonce（appId + nonce + timestamp -> true）
    std::unordered_set<std::string> usedNonces_;
    std::mutex nonceMutex_;
};
