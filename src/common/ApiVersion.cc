#include "ApiVersion.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <iomanip>

namespace api_version {

// ─────────────────────────────────────────────────────────────────────────────
// VersionManager
// ─────────────────────────────────────────────────────────────────────────────
void VersionManager::registerVersion(const VersionInfo& info) {
    versions_[info.version] = info;
    if (info.policy == DeprecationPolicy::ACTIVE) {
        if (latest_ < info.version) {
            latest_ = info.version;
        }
    }
}

std::vector<VersionInfo> VersionManager::allVersions() const {
    std::vector<VersionInfo> result;
    for (auto& [v, info] : versions_) {
        result.push_back(info);
    }
    std::sort(result.begin(), result.end(),
        [](const VersionInfo& a, const VersionInfo& b) { return a.version < b.version; });
    return result;
}

Version VersionManager::latestVersion() const {
    return latest_;
}

bool VersionManager::hasVersion(const Version& v) const {
    return versions_.find(v) != versions_.end();
}

const VersionInfo* VersionManager::getVersionInfo(const Version& v) const {
    auto it = versions_.find(v);
    if (it != versions_.end()) return &it->second;
    return nullptr;
}

Version VersionManager::parseRequestVersion(const drogon::HttpRequestPtr& req) const {
    const auto& path = req->getPath();

    // 1. 尝试从URL路径解析：/v1/, /v2/
    // 匹配格式: /v数字/ 或 /v数字.数字/
    size_t vPos = path.find("/v");
    if (vPos != std::string::npos) {
        size_t start = vPos + 2;
        size_t slash = path.find('/', start);
        if (slash != std::string::npos && slash > start) {
            std::string ver = path.substr(start, slash - start);
            return parseVersion("v" + ver);
        }
    }

    // 2. 尝试从Accept Header解析
    std::string accept = req->getHeader("Accept");
    size_t rpos = accept.find("vnd.ruoyi.v");
    if (rpos != std::string::npos) {
        size_t start = rpos + 10; // skip "vnd.ruoyi."
        size_t end = start;
        while (end < accept.size() && (isdigit(accept[end]) || accept[end] == '.')) end++;
        if (end > start) {
            return parseVersion("v" + accept.substr(start, end - start));
        }
    }

    // 3. 尝试从X-API-Version Header解析
    std::string apiVer = req->getHeader("X-API-Version");
    if (!apiVer.empty()) {
        return parseVersion(apiVer);
    }

    return Version{1, 0, "v1"};
}

Version VersionManager::getCompatibleVersion(const Version& requested) const {
    // 如果请求的版本存在，直接返回
    if (hasVersion(requested)) return requested;

    // 否则找最接近的较低版本
    Version compatible{1, 0, "v1"};
    for (auto& [v, info] : versions_) {
        if (v < requested && v > compatible && info.policy != DeprecationPolicy::REMOVED) {
            compatible = v;
        }
    }
    return compatible;
}

void VersionManager::addRouteHandler(
    const std::string& path,
    const std::string& method,
    const std::function<void(const drogon::HttpRequestPtr&,
                            std::function<void(const drogon::HttpResponsePtr&)>&,
                            const Version&)>& handler) {
    routeHandlers_.push_back({path, method});
    (void)handler; // 实际使用时存储handler
}

// ─────────────────────────────────────────────────────────────────────────────
// ApiVersionFilter
// ─────────────────────────────────────────────────────────────────────────────
void ApiVersionFilter::doFilter(const drogon::HttpRequestPtr &req,
                               drogon::FilterCallback &&fcb,
                               drogon::FilterChainCallback &&fccb) {
    auto requested = VersionManager::instance().parseRequestVersion(req);
    auto compatible = VersionManager::instance().getCompatibleVersion(requested);

    // 检查版本是否已废弃
    auto info = VersionManager::instance().getVersionInfo(compatible);
    if (info && info->policy == DeprecationPolicy::REMOVED) {
        Json::Value body;
        body["code"] = 410;
        body["msg"] = "此API版本已移除，请升级到最新版本";
        body["latest"] = VersionManager::instance().latestVersion().full;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->addHeader("Content-Type", "application/json");
        fcb(resp);
        return;
    }

    // 如果版本不兼容，添加警告头
    if (requested != compatible) {
        // 需要版本降级，这里简单处理直接用兼容版本
    }

    // 添加版本相关头
    // 注意：在实际handler中通过 addVersionHeaders 添加

    fccb();
}

} // namespace api_version

// ════════════════════════════════════════════════════════════════════════════
// CsrfProtection 实现
// ════════════════════════════════════════════════════════════════════════════

std::string CsrfProtection::generateToken(const std::string& sessionId) {
    unsigned char buf[32];
    RAND_bytes(buf, sizeof(buf));

    std::ostringstream ss;
    for (auto b : buf) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    std::string token = ss.str();

    std::lock_guard<std::mutex> lk(tokenMutex_);
    tokenStore_[sessionId] = token;
    return token;
}

bool CsrfProtection::validateToken(const std::string& sessionId, const std::string& token) {
    std::lock_guard<std::mutex> lk(tokenMutex_);
    auto it = tokenStore_.find(sessionId);
    if (it == tokenStore_.end()) return false;
    return it->second == token;
}

bool CsrfProtection::validateRequest(const drogon::HttpRequestPtr& req,
                                     const std::string& sessionId) {
    if (!requiresCsrfCheck(req)) return true;

    // 从Header获取
    std::string token = req->getHeader(headerName_);

    // 从Cookie获取
    if (token.empty()) {
        const auto& cookies = req->getCookies();
        auto it = cookies.find(cookieName_);
        if (it != cookies.end()) {
            token = it->second;
        }
    }

    // 从Body获取（表单提交）
    if (token.empty()) {
        auto body = req->getJsonObject();
        if (body && body->isMember(formFieldName_)) {
            token = (*body)[formFieldName_].asString();
        }
    }

    return validateToken(sessionId, token);
}

bool CsrfProtection::requiresCsrfCheck(const drogon::HttpRequestPtr& req) const {
    auto method = req->getMethod();

    // 安全方法不需要检查
    if (method == drogon::Get || method == drogon::Head || method == drogon::Options) {
        return false;
    }

    // 检查豁免路径
    if (isPathExempt(req->getPath())) {
        return false;
    }

    return true;
}

void CsrfProtection::addExemptPath(const std::string& pathPattern) {
    std::lock_guard<std::mutex> lk(mutex_);
    exemptPaths_.insert(pathPattern);
}

bool CsrfProtection::isPathExempt(const std::string& path) const {
    std::lock_guard<std::mutex> lk(mutex_);
    for (const auto& pattern : exemptPaths_) {
        // 简单通配符支持：/api/** 或 /api/*
        if (pattern == "/**") return true;
        size_t patternLen = pattern.size();
        if (patternLen >= 2 && pattern[patternLen-2] == '/' && pattern[patternLen-1] == '*' && pattern[patternLen-3] == '*') {
            // ends with /**
            auto prefix = pattern.substr(0, patternLen - 2);
            if (path.size() >= prefix.size() && path.substr(0, prefix.size()) == prefix) return true;
        } else if (patternLen >= 1 && pattern[patternLen-1] == '*' && patternLen >= 2 && pattern[patternLen-2] != '*') {
            // ends with /* (but not /**)
            auto prefix = pattern.substr(0, patternLen - 1);
            if (path.size() >= prefix.size() && path.substr(0, prefix.size()) == prefix) {
                // 排除子路径
                auto remaining = path.substr(prefix.size());
                if (remaining.find('/') == std::string::npos) return true;
            }
        }
        if (path == pattern) return true;
    }
    return false;
}

std::string CsrfProtection::createSetCookieHeader(const std::string& sessionId) {
    std::ostringstream ss;
    ss << cookieName_ << "=" << generateToken(sessionId);
    ss << "; Path=/";
    ss << "; HttpOnly";
    if (cookieConfig_.secure) ss << "; Secure";
    ss << "; SameSite=" << cookieConfig_.sameSite;
    ss << "; Max-Age=" << cookieConfig_.maxAge;
    return ss.str();
}

void CsrfProtection::setCookieConfig(bool httpOnly, bool secure, int maxAgeSeconds) {
    cookieConfig_.httpOnly = httpOnly;
    cookieConfig_.secure = secure;
    cookieConfig_.maxAge = maxAgeSeconds;
}

// ════════════════════════════════════════════════════════════════════════════
// SecurityHeaders 实现
// ════════════════════════════════════════════════════════════════════════════

void SecurityHeaders::apply(drogon::HttpResponsePtr& resp) const {
    // Frame Options
    resp->addHeader("X-Frame-Options", frameOptions_);

    // Content Security Policy
    resp->addHeader("Content-Security-Policy", csp_);

    // HSTS
    if (hstsEnabled_) {
        std::ostringstream hsts;
        hsts << "max-age=" << hstsMaxAge_;
        resp->addHeader("Strict-Transport-Security", hsts.str());
    }

    // X-Content-Type-Options
    resp->addHeader("X-Content-Type-Options", xContentTypeOptions_);

    // X-XSS-Protection
    resp->addHeader("X-XSS-Protection", xssProtection_);

    // Referrer-Policy
    resp->addHeader("Referrer-Policy", referrerPolicy_);

    // CORS
    if (!corsOrigins_.empty()) {
        std::ostringstream allowOrigin;
        allowOrigin << corsOrigins_[0];  // 简化处理
        resp->addHeader("Access-Control-Allow-Origin", allowOrigin.str());
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
        resp->addHeader("Access-Control-Max-Age", std::to_string(corsMaxAge_));
        resp->addHeader("Access-Control-Allow-Credentials", "true");
    }

    // 额外安全头
    resp->addHeader("X-Permitted-Cross-Domain-Policies", "none");
    resp->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    resp->addHeader("Pragma", "no-cache");
}

void SecurityHeaders::setFrameOptions(const std::string& policy) {
    frameOptions_ = policy;
}

void SecurityHeaders::setContentSecurityPolicy(const std::string& csp) {
    csp_ = csp;
}

void SecurityHeaders::setHstsMaxAge(int seconds) {
    hstsEnabled_ = true;
    hstsMaxAge_ = seconds;
}

void SecurityHeaders::setXssProtection(const std::string& mode) {
    xssProtection_ = mode;
}

void SecurityHeaders::setCorsOrigins(const std::vector<std::string>& origins) {
    corsOrigins_ = origins;
}

void SecurityHeaders::setCorsMaxAge(int seconds) {
    corsMaxAge_ = seconds;
}

void SecurityHeaders::setReferrerPolicy(const std::string& policy) {
    referrerPolicy_ = policy;
}

// ════════════════════════════════════════════════════════════════════════════
// SecurityFilter 实现
// ════════════════════════════════════════════════════════════════════════════

bool SecurityFilter::initialized_ = false;

void SecurityFilter::init(const std::string& configPath) {
    // 从配置文件加载安全设置
    // CSRF豁免路径、安全头配置等
    initialized_ = true;
}

void SecurityFilter::doFilter(const drogon::HttpRequestPtr &req,
                             drogon::FilterCallback &&fcb,
                             drogon::FilterChainCallback &&fccb) {
    // 1. 应用安全头（通过响应中间件）
    auto tempResp = drogon::HttpResponse::newHttpResponse();
    SecurityHeaders::instance().apply(tempResp);  // 简化，实际应在响应路径处理

    // 2. CSRF验证
    // 注意：实际验证需要sessionId，这里简化处理

    // 3. 请求签名验证（如需要）
    // RequestSignature::instance().validate(...);

    fccb();
}

// ════════════════════════════════════════════════════════════════════════════
// RequestSignature 实现
// ════════════════════════════════════════════════════════════════════════════

std::string RequestSignature::generateSignature(const SignParams& params) {
    return instance().generateSignatureImpl(params);
}

std::string RequestSignature::generateSignatureImpl(const SignParams& params) const {
    // 构建签名字符串
    std::ostringstream ss;
    ss << params.method << "\n";
    ss << params.path << "\n";
    ss << params.timestamp << "\n";
    ss << params.nonce << "\n";
    if (!params.queryString.empty()) ss << params.queryString << "\n";
    if (!params.bodyHash.empty()) ss << params.bodyHash;

    unsigned char* mac = nullptr;
    unsigned int macLen = 0;

    if (algorithm_ == Algorithm::HmacSha256) {
        mac = HMAC(EVP_sha256(),
            params.secret.data(), params.secret.size(),
            (unsigned char*)ss.str().data(), ss.str().size(),
            nullptr, &macLen);
    } else {
        mac = HMAC(EVP_sha512(),
            params.secret.data(), params.secret.size(),
            (unsigned char*)ss.str().data(), ss.str().size(),
            nullptr, &macLen);
    }

    if (!mac) return "";

    std::ostringstream hex;
    for (unsigned int i = 0; i < macLen; i++) {
        hex << std::hex << std::setw(2) << std::setfill('0') << (int)mac[i];
    }
    return hex.str();
}

std::string RequestSignature::sign(const SignParams& params) {
    return generateSignature(params);
}

bool RequestSignature::validate(const drogon::HttpRequestPtr& req,
                                const std::string& appId,
                                const std::string& secret,
                                int timestampToleranceSec) {
    std::string signature = req->getHeader(signatureHeader_);
    std::string timestamp = req->getHeader(timestampHeader_);
    std::string nonce = req->getHeader(nonceHeader_);

    if (signature.empty() || timestamp.empty() || nonce.empty()) {
        return false;
    }

    // 检查时间戳容差
    try {
        auto ts = std::stoll(timestamp);
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (std::llabs(now - ts) > timestampToleranceSec) {
            return false;
        }
    } catch (...) {
        return false;
    }

    // 防重放：检查nonce
    {
        std::lock_guard<std::mutex> lk(nonceMutex_);
        std::string nonceKey = appId + ":" + nonce + ":" + timestamp;
        if (usedNonces_.count(nonceKey)) {
            return false;  // 重放攻击
        }
        usedNonces_.insert(nonceKey);
        // 清理过期nonce（简单策略）
        if (usedNonces_.size() > 100000) {
            usedNonces_.clear();
        }
    }

    // 计算签名
    SignParams params;
    params.appId = appId;
    params.secret = secret;
    params.method = req->getMethodString();
    params.path = req->getPath();
    params.timestamp = timestamp;
    params.nonce = nonce;
    params.queryString = req->getQuery();
    // Body hash
    auto body = req->getJsonObject();
    if (body) {
        std::string bodyStr = body->toStyledString();
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256((unsigned char*)bodyStr.data(), bodyStr.size(), hash);
        std::ostringstream h;
        for (auto c : hash) h << std::hex << std::setw(2) << (int)c;
        params.bodyHash = h.str();
    }

    auto expected = generateSignature(params);
    return signature == expected;
}

void RequestSignature::registerApp(const std::string& appId, const std::string& secret) {
    std::lock_guard<std::mutex> lk(mutex_);
    appSecrets_[appId] = secret;
}

void RequestSignature::unregisterApp(const std::string& appId) {
    std::lock_guard<std::mutex> lk(mutex_);
    appSecrets_.erase(appId);
}
