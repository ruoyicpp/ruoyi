/**
 * @file AppBootstrap.h
 * @brief main.cc 拆分的共享头文件
 *
 * 包含：
 *   - 所有模块共用的 #include（与原 main.cc 保持一致）
 *   - AppContext：跨阶段共享的启动上下文
 *   - AuthHelper：管理员令牌验证辅助命名空间
 *   - buildDbConnStr：构造 libpq 连接串（实现在 DbConnStr.cc）
 *   - ruoyi::boot::*：各启动阶段函数声明（实现在对应 .cc 中）
 *
 * 拆分文件清单（与原 main.cc 的执行顺序一致）：
 *   EarlyInit.cc      —— 早期初始化：日志兜底、OpenSSL、watchdog 移交、
 *                        单实例锁、WorkerOrchestrator 多进程编排
 *   ConfigInit.cc     —— 配置阶段：--config 解析、DefaultConfig、许可证、
 *                        集群模式、schema 校验、日志系统、stdout 重定向、
 *                        ConfigLoader、LogCollector、Vault、缓存、JWT、
 *                        安全配置（RateLimiter/SignUtils/IpUtils/WAF 等）
 *   HttpSetup.cc      —— HTTP 装配：CORS、前端托管（外部目录/嵌入式）、
 *                        Metrics/HotConfig/NginxLike、WAF 拦截、IP 限流、
 *                        Bot UA 拦截、自定义错误处理、安全响应头、
 *                        XSS/SQLi 告警、API 鉴权 advice、各类定时任务
 *   RoutesSetup.cc    —— 内置路由：/profile、/iconfont-sys.woff2、
 *                        /health、/version、/api/video/*、/ssl-config 页面
 *   CertRoutes.cc     —— /certmanager 页面 + /certmanager/api/* + /api/ssl/*
 *   StartupAdvice.cc  —— registerBeginningAdvice：DB 连接/初始化、菜单 URL
 *                        校正、设备绑定、缓存加载、子进程（KoboldCpp/DDNS/
 *                        Nginx/ExternalServices）、启动横幅
 *   RuntimeSetup.cc   —— JobScheduler、Token 恢复、日志归档、HTTPS 监听、
 *                        NginxEmbedded、ACME、TaskQueue、watchdog 心跳、
 *                        drogon::app().run() 与退出清理
 */

#pragma once

#include "AppIncludes.h"
#include "config/DefaultConfig.h"
#include "common/ColorLogger.h"
#ifdef RUOYI_USE_EMBEDDED_FRONTEND
#  include "common/EmbeddedFrontend.h"
#endif
#include "common/NginxLikeFeatures.h"
#include "common/NginxEmbedded.h"
#include "cache/CacheStrategy.h"
#include "cache/CacheWarmup.h"
#include "cache/CacheInvalidation.h"
#include "log/LogCollector.h"
#include "log/LogAnalyzer.h"
#include "log/LogSearchEngine.h"
#include "taskqueue/TaskQueue.h"
#include "taskqueue/TaskQueueExample.h"
#include "services/WorkerOrchestrator.h"
#include "services/AcmeManager.h"
#include "services/CertManagerDriver.h"
#ifdef _WIN32
#  include <io.h>      // _isatty / _fileno
#endif

#include <optional>
#include <memory>
#include <string>

// OpenSSL provider 初始化（由外部编译单元提供）
extern "C" void ruoyi_init_openssl_provider();

/**
 * @struct AppContext
 * @brief 跨启动阶段共享的上下文
 *
 * 各 boot::* 阶段通过该结构交换运行期状态（原 main() 内的局部变量）。
 */
struct AppContext {
    std::filesystem::path exePath;             ///< 可执行文件所在目录（ EarlyInit 解析并切换工作目录）
    std::string configFile = "config.json";    ///< --config 指定的配置文件路径
    std::shared_ptr<ConfigLoader> cfgLoader;   ///< 配置加载器（ConfigInit 阶段创建）
    bool isPrimary = true;                     ///< 是否主进程（单进程或 worker[0]）
};

/**
 * @brief 从 config.json 的 database 节构造 libpq 连接串（兼容旧版调用方式）
 * @param cfgFile 配置文件路径（通常为 "config.json"）
 * @param timeout 连接超时时间（秒，默认 5）
 * @return libpq 连接字符串；配置文件不存在或格式错误时返回空字符串
 */
std::string buildDbConnStr(const std::string& cfgFile, int timeout = 5);

/**
 * @brief 从 ConfigLoader 构造 libpq 连接串（推荐版本，支持 Vault 补全密码）
 * @param loader ConfigLoader 实例
 * @param timeout 连接超时时间（秒，默认 5）
 * @return libpq 连接字符串
 */
std::string buildDbConnStr(const ConfigLoader& loader, int timeout = 5);

/**
 * @namespace AuthHelper
 * @brief 公共认证辅助函数命名空间
 *
 * 提供管理员令牌验证和认证响应生成功能。
 * 支持多种令牌来源：HTTP Header、Cookie、Query 参数。
 * 包含 localhost 白名单，便于开发环境调试。
 */
namespace AuthHelper {
    /**
     * @brief 验证管理员令牌
     *
     * 令牌来源优先级：Authorization Header → Cookie(Admin-Token) → query(token)。
     * 令牌无效时检查请求来源 IP 是否在 localhost 白名单中。
     */
    inline bool verifyAdminToken(const drogon::HttpRequestPtr& req) {
        auto token = SecurityUtils::getToken(req);

        // 尝试从 Cookie 中获取 Admin-Token
        if (token.empty()) {
            const std::string& ck = req->getHeader("cookie");
            const std::string key = "Admin-Token=";
            auto pos = ck.find(key);
            if (pos != std::string::npos) {
                pos += key.size();
                auto end = ck.find(';', pos);
                token = ck.substr(pos, end == std::string::npos ? end : end - pos);
            }
        }

        // 尝试从 query 参数获取 token
        if (token.empty()) {
            token = req->getParameter("token");
        }

        // 验证 JWT token
        if (!token.empty()) {
            try {
                auto uuid = JwtUtils::parseUuid(token);
                auto userKey = SecurityUtils::getTokenKey(uuid);
                if (TokenCache::instance().get(userKey)) {
                    return true;
                }
            } catch (...) {}
        }

        // localhost 白名单（开发环境）
        const auto& peer = req->getPeerAddr().toIp();
        return (peer == "127.0.0.1" || peer == "::1" || peer == "0.0.0.0");
    }

    /**
     * @brief 生成 401 未授权 JSON 响应
     * @return HTTP 401 响应对象，包含 { "error": "Unauthorized" }
     */
    inline drogon::HttpResponsePtr make401JsonResponse() {
        Json::Value err;
        err["error"] = "Unauthorized";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k401Unauthorized);
        return resp;
    }
}

/**
 * @namespace ruoyi::boot
 * @brief main() 启动流程的各阶段函数
 *
 * 所有阶段函数按声明顺序在 main() 中依次调用，顺序与原文件一致。
 * 返回 std::optional<int>：std::nullopt 表示继续，有值表示该值即为进程退出码
 * （对应原 main() 中的 return 语句）。
 */
namespace boot {

/// 早期初始化：日志兜底、崩溃处理、OpenSSL、watchdog 移交、单实例锁、WorkerOrchestrator
std::optional<int> earlyInit(int argc, char* argv[], AppContext& ctx);

/// 配置与服务初始化：--config、许可证、集群、日志、ConfigLoader、缓存、安全配置
std::optional<int> configInit(int argc, char* argv[], AppContext& ctx);

/// HTTP 装配：CORS、前端托管、WAF、限流、错误处理、安全头、API 鉴权、定时任务
std::optional<int> setupHttp(AppContext& ctx);

/// 注册内置路由（/profile、/health、/version、/api/video、/ssl-config 等）
void registerBuiltinRoutes(AppContext& ctx);

/// 注册 certmanager 页面与 API、/api/ssl/* 路由
void registerCertRoutes(AppContext& ctx);

/// 注册 registerBeginningAdvice（DB 初始化、菜单 URL 校正、子进程启动等）
void registerStartupAdvice(AppContext& ctx);

/// 运行时服务：JobScheduler、Token 恢复、HTTPS 监听、ACME、TaskQueue、心跳线程
/// 返回的心跳停止标志由 stopHeartbeat() 在 run() 返回后使用
void startRuntimeServices(AppContext& ctx);

/// 停止心跳线程并执行退出清理（TaskQueue/ACME/NginxEmbedded 停止）
void shutdownCleanup();

} // namespace boot
