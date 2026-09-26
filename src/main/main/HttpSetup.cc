/**
 * @file HttpSetup.cc
 * @brief HTTP 装配阶段（原 main.cc 中部）
 *
 * 顺序与原文件一致：
 *   1. CORS（preflight + postHandling）
 *   2. 前端托管（外部 ./web 目录 / 嵌入式 RUOYI_USE_EMBEDDED_FRONTEND）
 *   3. MetricsCollector /actuator/* + DB 打点钩子
 *   4. HotConfig 文件监视器
 *   5. nginx 风格功能集成（proxy_pass / allow-deny / limit_conn / access_log）
 *   6. WAF 请求拦截（仅 Linux）
 *   7. IP 限流（DDoS 防御，托管前端时跳过静态资源）
 *   8. Bot UA 拦截
 *   9. 自定义错误处理器（JSON 404/405/500 + SPA fallback）
 *   10. 安全响应头（XSS/点击劫持/CSP）
 *   11. XSS/SQLi 告警 advice
 *   12. 接口验证 advice（challenge token / HMAC 验签 / JWT 放行）
 *   13. 定时任务：限流器清理、审计队列、AI 风控、日志留存、日志索引、
 *       慢 SQL、集群会话心跳、缓存同步、配置热更新、验证码、备份、MQTT
 */

#include "AppBootstrap.h"

namespace boot {

// ── 前端托管共享状态（原 main() 内的 static 局部变量，仅本文件使用）──────────
//   feHosted=true  → 启用 SPA 404 回退 + 限流跳过静态资源
//   feApiPrefix    → 用于区分 API 与静态资源
//   feIndexPath    → 外部模式下的 index.html 绝对路径
static bool        feHosted    = false;
static bool        feSpaMode   = false;
static bool        feEmbedded  = false;
static std::string feApiPrefix;
static std::string feIndexPath;

std::optional<int> setupHttp(AppContext& ctx) {
    auto& configFile = ctx.configFile;
    // ── CORS（从 config.json cors 段读取，无需重新编译）─────────────────────
    {
        struct CorsCfg {
            std::vector<std::string> origins;
            std::string methods;
            std::string headers;
            std::string expose;
            bool credentials = false;
        };
        auto corsCfg = std::make_shared<CorsCfg>();
        std::ifstream ccf("config.json");
        if (ccf.is_open()) {
            Json::Value root; Json::CharReaderBuilder rb; std::string errs;
            if (Json::parseFromStream(rb, ccf, &root, &errs) && root.isMember("cors")) {
                auto& c = root["cors"];
                if (c.isMember("allow_origins"))
                    for (auto& o : c["allow_origins"]) corsCfg->origins.push_back(o.asString());
                if (c.isMember("allow_methods")) {
                    std::string m;
                    for (auto& v : c["allow_methods"]) { if (!m.empty()) m+=','; m+=v.asString(); }
                    corsCfg->methods = m;
                }
                if (c.isMember("allow_headers")) {
                    std::string h;
                    for (auto& v : c["allow_headers"]) { if (!h.empty()) h+=','; h+=v.asString(); }
                    corsCfg->headers = h;
                }
                if (c.isMember("expose_headers")) {
                    std::string e;
                    for (auto& v : c["expose_headers"]) { if (!e.empty()) e+=','; e+=v.asString(); }
                    corsCfg->expose = e;
                }
                corsCfg->credentials = c.get("allow_credentials", false).asBool();
            }
        }
        // 若 menu.api_base_url 已配置，自动将其 origin 加入 CORS 白名单
        // 避免生产域名只填了 api_base_url 而忘了在 cors.allow_origins 重复填写
        {
            std::ifstream maf("config.json");
            if (maf.is_open()) {
                Json::Value mr; Json::CharReaderBuilder rb2; std::string err2;
                if (Json::parseFromStream(rb2, maf, &mr, &err2)
                    && mr.isMember("menu") && mr["menu"].isMember("api_base_url")) {
                    std::string abu = mr["menu"]["api_base_url"].asString();
                    if (!abu.empty()) {
                        // 提取 scheme://host[:port]
                        auto pos = abu.find("://");
                        if (pos != std::string::npos) {
                            auto rest = abu.substr(pos + 3);
                            auto slash = rest.find('/');
                            std::string origin = abu.substr(0, pos + 3)
                                + (slash != std::string::npos ? rest.substr(0, slash) : rest);
                            // 避免重复
                            bool found = false;
                            for (auto& o : corsCfg->origins) if (o == origin) { found = true; break; }
                            if (!found) {
                                corsCfg->origins.push_back(origin);
                                LOG_INFO << "[CORS] auto-added origin from api_base_url: " << origin;
                            }
                        }
                    }
                }
            }
        }
        if (corsCfg->origins.empty()) corsCfg->origins.push_back("*");
        if (corsCfg->methods.empty())  corsCfg->methods  = "GET,POST,PUT,DELETE,OPTIONS";
        if (corsCfg->headers.empty())  corsCfg->headers  = "*";
        LOG_INFO << "[CORS] origins=" << corsCfg->origins[0]
                 << " credentials=" << corsCfg->credentials;

        // 解析 Origin → allowOrigin 的公共逻辑
        auto resolveOrigin = [corsCfg](const std::string& origin) -> std::string {
            if (origin.empty()) return "";
            bool wildcard = (corsCfg->origins.size() == 1 && corsCfg->origins[0] == "*");
            if (wildcard) return "*";
            for (auto& o : corsCfg->origins)
                if (o == origin) return o;
            return "";
        };

        // Preflight (OPTIONS) → 直接返回 CORS 头
        drogon::app().registerPreRoutingAdvice(
            [corsCfg, resolveOrigin](const drogon::HttpRequestPtr &req,
                      drogon::AdviceCallback &&acb,
                      drogon::AdviceChainCallback &&accb) {
                if (req->method() != drogon::Options) { accb(); return; }
                std::string allowOrigin = resolveOrigin(req->getHeader("Origin"));
                if (allowOrigin.empty()) { accb(); return; }
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->addHeader("Access-Control-Allow-Origin",  allowOrigin);
                resp->addHeader("Access-Control-Allow-Methods", corsCfg->methods);
                resp->addHeader("Access-Control-Allow-Headers", corsCfg->headers);
                resp->addHeader("Access-Control-Max-Age",       "86400");
                if (!corsCfg->expose.empty())
                    resp->addHeader("Access-Control-Expose-Headers", corsCfg->expose);
                if (corsCfg->credentials && allowOrigin != "*")
                    resp->addHeader("Access-Control-Allow-Credentials", "true");
                if (allowOrigin != "*")
                    resp->addHeader("Vary", "Origin");
                resp->setStatusCode(drogon::k204NoContent);
                acb(resp);
            });

        // 实际请求 → postHandling 追加 CORS 头
        drogon::app().registerPostHandlingAdvice(
            [corsCfg, resolveOrigin](const drogon::HttpRequestPtr &req,
                                     const drogon::HttpResponsePtr &resp) {
                std::string allowOrigin = resolveOrigin(req->getHeader("Origin"));
                if (allowOrigin.empty()) return;
                resp->addHeader("Access-Control-Allow-Origin", allowOrigin);
                if (!corsCfg->expose.empty())
                    resp->addHeader("Access-Control-Expose-Headers", corsCfg->expose);
                if (corsCfg->credentials && allowOrigin != "*")
                    resp->addHeader("Access-Control-Allow-Credentials", "true");
                if (allowOrigin != "*")
                    resp->addHeader("Vary", "Origin");
            });
    }

    // ── 前端托管（两种模式二选一）────────────────────
    //   模式 A: "frontend"           — 外部 ./web 目录（方便热更新）
    //   模式 B: "embedded_frontend"  — 编译进 exe（单文件分发，需 cmake -DRUOYI_EMBED_FRONTEND=ON）
    try {
        Json::Value cfgRoot;
        {
            std::ifstream _fcf(configFile);
            if (_fcf.is_open()) {
                Json::CharReaderBuilder _rb; std::string _err;
                Json::parseFromStream(_rb, _fcf, &cfgRoot, &_err);
            }
        }
        bool extEnabled = cfgRoot.isMember("frontend") &&
                          cfgRoot["frontend"].get("enabled", false).asBool();
        bool embEnabled = cfgRoot.isMember("embedded_frontend") &&
                          cfgRoot["embedded_frontend"].get("enabled", false).asBool();

        if (extEnabled && embEnabled) {
            std::cerr << "\n[错误] frontend 与 embedded_frontend 不能同时启用，"
                         "请在 config.json 中只启用其中一个。\n" << std::endl;
            return 1;
        }

        // 模式 A: 外部目录托管
        if (extEnabled) {
            const auto& fc       = cfgRoot["frontend"];
            std::string distPath = fc.get("dist_path", "./web").asString();
            bool        spaMode  = fc.get("spa_mode", true).asBool();
            std::string apiPrefix= fc.get("api_prefix", "/prod-api").asString();
            int         cacheSec = fc.get("cache_seconds", 3600).asInt();

            if (std::filesystem::exists(distPath)
                && std::filesystem::exists(distPath + "/index.html")) {

                drogon::app().setDocumentRoot(distPath);
                drogon::app().setStaticFilesCacheTime(cacheSec);
                // 内置压缩支持（drogon 默认即开启，此处显式声明语义）：
                //   enableGzip       — 实时压缩响应（非二进制 + >1KB）
                //   setGzipStatic    — 客户端 Accept-Encoding: gzip 时，
                //                       优先发同目录 .gz 预压缩文件（Vue dist
                //                       的 compression-webpack-plugin 输出已带 .gz）
                //   setBrStatic      — 同理优先发 .br Brotli 预压缩
                drogon::app().enableGzip(true);
                drogon::app().setGzipStatic(true);
                drogon::app().setBrStatic(true);
                LOG_INFO << "[Frontend] external dir: "
                         << std::filesystem::absolute(distPath).string()
                         << " | SPA=" << spaMode
                         << " | API=" << apiPrefix
                         << " | cache=" << cacheSec << "s";
                std::cout << "[Frontend] 外部前端: "
                          << std::filesystem::absolute(distPath).string()
                          << "  api_prefix=" << apiPrefix << std::endl;

                // API 路径剥离前缀（前端一般经 /prod-api/* 调用）
                if (!apiPrefix.empty() && apiPrefix != "/") {
                    drogon::app().registerPreRoutingAdvice(
                        [apiPrefix](const drogon::HttpRequestPtr& req,
                                    drogon::AdviceCallback&&,
                                    drogon::AdviceChainCallback&& ccb) {
                            std::string p = req->path();
                            if (p.rfind(apiPrefix, 0) == 0) {
                                std::string np = p.substr(apiPrefix.size());
                                if (np.empty()) np = "/";
                                req->setPath(np);
                            }
                            ccb();
                        });
                }

                // 注意：不在此处 setCustom404Page。统一由后面的 setCustomErrorHandler
                // 智能判断：API 路径返 JSON 404；SPA 路径才回退到 index.html。
                // 这样可以避免 API 调用 typo 路径时被误回 HTML 导致前端 axios JSON 解析失败。
                feHosted    = true;
                feSpaMode   = spaMode;
                feApiPrefix = apiPrefix;
                feIndexPath = std::filesystem::absolute(distPath + "/index.html").string();
                // 根据前端 SPA 模式自动切换 HTTP 响应策略
                // SPA 模式：HTTP 200 + X-Business-Code 头；非 SPA：HTTP 真实状态码
                HttpStatus::setSpaMode(spaMode);
            } else {
                LOG_WARN << "[Frontend] dist_path 不存在或缺少 index.html: "
                         << distPath << "（已跳过托管）";
                std::cout << "[Frontend] 警告: " << distPath
                          << " 不存在或缺少 index.html，已跳过托管" << std::endl;
            }
        }

        // 模式 B: 嵌入式（需在编译期 -DRUOYI_EMBED_FRONTEND=ON）
        if (embEnabled) {
#ifdef RUOYI_USE_EMBEDDED_FRONTEND
            const auto& ec = cfgRoot["embedded_frontend"];
            bool        spaMode  = ec.get("spa_mode", true).asBool();
            std::string apiPrefix= ec.get("api_prefix", "/prod-api").asString();
            EmbeddedFrontend::registerHandlers(apiPrefix, spaMode);
            feHosted    = true;
            feEmbedded  = true;
            feSpaMode   = spaMode;
            feApiPrefix = apiPrefix;
            // 根据前端 SPA 模式自动切换 HTTP 响应策略
            // SPA 模式：HTTP 200 + X-Business-Code 头；非 SPA：HTTP 真实状态码
            HttpStatus::setSpaMode(spaMode);
#else
            std::cerr << "\n[错误] embedded_frontend 已启用，但本次编译未嵌入前端！\n"
                      << "  请用 cmake -DRUOYI_EMBED_FRONTEND=ON -DRUOYI_EMBED_FRONTEND_DIR=./web 重新编译。\n"
                      << std::endl;
            return 1;
#endif
        }
    } catch (const std::exception& e) {
        LOG_WARN << "[Frontend] 加载配置失败: " << e.what();
    }

    // ── 可观测性：/actuator/* 端点 + HTTP 自动打点 advice ─────────────
    // /actuator/health  /actuator/info  /actuator/metrics  /actuator/db
    // POST /actuator/reload（仅 loopback）
    try {
        MetricsCollector::instance().registerActuator();
        MetricsCollector::instance().attachAdvice();
        // 把 DB 查询打点钩到 Metrics（DatabaseService 不直接依赖 MetricsCollector）
        DbMetricsHook::hook = [](long ms, bool ok, bool isWrite) {
            MetricsCollector::instance().onDbQuery(ms, ok, isWrite);
        };
        // 慢 SQL 审计钩子：logSlow 触发 → 入队（O(1)，不写库防死锁）
        DbMetricsHook::slowHook = [](const char* op, const std::string& sql, long ms) {
            SlowLogQueue::instance().push(op, sql, ms);
        };
        LOG_INFO << "[Metrics] /actuator/* endpoints registered, "
                    "HTTP duration histogram + DB hook attached";
        std::cout << "[Metrics] /actuator/metrics 已启用 (含 DB 慢查询计数)" << std::endl;
    } catch (const std::exception& e) {
        LOG_WARN << "[Metrics] 启用失败: " << e.what();
    }

    // ── HotConfig 文件监视器（5s 间隔检查 config.json mtime） ─────────
    try {
        HotConfig::instance().start(configFile, []{
            LOG_INFO << "[HotConfig] config.json reloaded";
            std::cout << "[HotConfig] config.json reloaded" << std::endl;
        });
    } catch (const std::exception& e) {
        LOG_WARN << "[HotConfig] 启动失败: " << e.what();
    }

    // ── nginx 风格功能集成（proxy_pass / allow-deny / limit_conn / access_log）──
    // 优先于通用限流：proxy 命中后直接转发上游，不会进入 drogon 路由
    try {
        ruoyi::nginx_like::registerAll(drogon::app().getCustomConfig());
    } catch (const std::exception& e) {
        LOG_WARN << "[NginxLike] 加载失败: " << e.what();
    }

    // ── WAF 请求拦截（仅 Linux；在限流之前执行，最先挡掉恶意请求）─────────
#ifdef __linux__
    drogon::app().registerPreRoutingAdvice(
        [](const drogon::HttpRequestPtr &req,
           drogon::AdviceCallback &&acb,
           drogon::AdviceChainCallback &&accb) {
            if (!WafEngine::instance().isEnabled()) { accb(); return; }
            auto verdict = WafEngine::instance().inspect(req);
            if (verdict.action == WafAction::Block ||
                verdict.action == WafAction::Ban) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k403Forbidden);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"code\":403,\"msg\":\"请求被安全策略拦截\"}");
                acb(resp);
                return;
            }
            // 跳转验证码：配置了 captcha_url 则 302，否则返回 449 让前端弹验证
            if (verdict.action == WafAction::Captcha) {
                auto& url = WafEngine::instance().captchaUrl();
                if (!url.empty()) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k302Found);
                    resp->addHeader("Location", url);
                    acb(resp);
                } else {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode((drogon::HttpStatusCode)449);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody("{\"code\":449,\"msg\":\"需要完成安全验证\"}");
                    acb(resp);
                }
                return;
            }
            // AI 风控：通过 WAF 的请求异步采样送检（不阻塞，纳秒级入队）
            AiRiskEngine::instance().inspect(req);
            accb();
        });
#endif // __linux__

    // ── IP 限流 (DDoS 防御) ─────────────────────────────────────────────────
    // 当合并部署托管前端时，跳过静态资源（带扩展名且非 API 前缀），
    // 避免单个用户加载几十个 JS/CSS 触发 200/min 限流误封。
    drogon::app().registerPreRoutingAdvice(
        [](const drogon::HttpRequestPtr &req,
           drogon::AdviceCallback &&acb,
           drogon::AdviceChainCallback &&accb) {
            if (feHosted) {
                std::string p = req->path();
                bool isApi = !feApiPrefix.empty() && feApiPrefix != "/" &&
                             p.rfind(feApiPrefix, 0) == 0;
                // 静态资源：路径含扩展名且非 API
                auto slash = p.find_last_of('/');
                auto seg   = (slash == std::string::npos) ? p : p.substr(slash + 1);
                bool hasExt = seg.find('.') != std::string::npos;
                if (!isApi && hasExt) { accb(); return; }
            }
            std::string ip = IpUtils::getIpAddr(req);
            if (!::RateLimiter::instance().allow(ip)) {
                MetricsCollector::instance().onRateLimited();
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode((drogon::HttpStatusCode)429);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"code\":429,\"msg\":\"请求过于频繁，请稍后重试\"}");
                resp->addHeader("Retry-After", "300");
                acb(resp);
                return;
            }
            accb();
        });

    // ── Bot UA 拦截（绕过 nginx 直连后端时的第二道防线）─────────────────────
    drogon::app().registerPreRoutingAdvice(
        [](const drogon::HttpRequestPtr &req,
           drogon::AdviceCallback &&acb,
           drogon::AdviceChainCallback &&accb) {
            // OPTIONS 预检请求放行
            if (req->method() == drogon::Options) { accb(); return; }

            // IM 公开接口白名单（无需认证，允许 Bot 访问）
            std::string path = req->path();
            if (path.find("/im/") == 0) {
                accb();
                return;
            }

            std::string ua = req->getHeader("User-Agent");
            if (isBotUserAgent(ua)) {
                LOG_WARN << "[Security] Bot UA blocked (global): "
                         << ua.substr(0, 80) << " ip=" << IpUtils::getIpAddr(req)
                         << " path=" << req->path();
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k403Forbidden);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"code\":403,\"msg\":\"非法请求\"}");
                acb(resp);
                return;
            }
            accb();
        });

    // ── 自定义默认错误响应：404/405/500 等也走 AjaxResult JSON 格式 ──
    // 否则 drogon 默认返回 HTML，前端 axios 解析失败后报"未知错误"
    // 同时支持 SPA fallback：托管前端时，非 API 且无扩展名路径回退到 index.html
    drogon::app().setCustomErrorHandler(
        [](drogon::HttpStatusCode code,
           const drogon::HttpRequestPtr &req) -> drogon::HttpResponsePtr {
            // 404/405 SPA 回退：仅对前端路由路径生效，避免误伤 API typo
            // 405 也需要回退：drogon 对无扩展名路径可能因方法不匹配返回 405
            bool isSpaCode = (code == drogon::k404NotFound ||
                              code == drogon::k405MethodNotAllowed);
            if (isSpaCode && feHosted && feSpaMode) {
                std::string p = req->path();
                // API 判断：只有路径以 feApiPrefix 开头才视为 API
                // 去掉 segment-counting 启发式规则（2+ 段会误判 /system/user 等 SPA 嵌套路由）
                bool isApi = !feApiPrefix.empty() && feApiPrefix != "/" &&
                             p.rfind(feApiPrefix, 0) == 0;
                auto slash = p.find_last_of('/');
                auto seg   = (slash == std::string::npos) ? p : p.substr(slash + 1);
                bool hasExt = seg.find('.') != std::string::npos;
                // 非 API + 无扩展名 → 视作 vue-router 前端路径，回退 index.html
                if (!isApi && !hasExt) {
                    if (!feEmbedded && !feIndexPath.empty()) {
                        auto resp = drogon::HttpResponse::newHttpResponse();
                        resp->setStatusCode(drogon::k200OK);
                        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
                        std::ifstream f(feIndexPath, std::ios::binary);
                        if (f) {
                            std::stringstream ss; ss << f.rdbuf();
                            resp->setBody(ss.str());
                        }
                        return resp;
                    }
                    // 嵌入式：EmbeddedFrontend 的 advice 已处理，几乎不会到这里；
                    // 兜底返回简单 200 让前端继续加载（极少触发）
                }
            }
            return ErrorPage::build(code);
        });

    // ── 安全响应头（XSS/点击劫持/内容嗅探防御）────────────────────────────
    drogon::app().registerPostHandlingAdvice(
        [](const drogon::HttpRequestPtr& req,
           const drogon::HttpResponsePtr& resp) {
            resp->addHeader("X-Content-Type-Options",  "nosniff");
            resp->addHeader("X-XSS-Protection",        "1; mode=block");
            resp->addHeader("Referrer-Policy",         "strict-origin-when-cross-origin");
            // /ai/* 页面允许被跨域 iframe 嵌入（前端内嵌 AI 会话页）
            const std::string& p = req->path();
            bool isAiPage = (p == "/ai" || p == "/ai/" ||
                             (p.size() > 4 && p.compare(0, 4, "/ai/") == 0));
            bool isCertmgrPage = (p == "/certmanager" ||
                                  p.rfind("/certmanager/", 0) == 0);
            if (!isAiPage && !isCertmgrPage) {
                resp->addHeader("X-Frame-Options", "SAMEORIGIN");
                resp->addHeader("Content-Security-Policy",
                    "default-src 'self'; script-src 'self' 'unsafe-inline'; "
                    "style-src 'self' 'unsafe-inline'; img-src 'self' data:");
            } else if (isCertmgrPage) {
                // certmanager UI 依赖 tailwindcss/jsdelivr CDN，放宽 CSP
                resp->addHeader("X-Frame-Options", "SAMEORIGIN");
                resp->addHeader("Content-Security-Policy",
                    "default-src 'self'; "
                    "script-src 'self' 'unsafe-inline' 'unsafe-eval' "
                        "https://cdn.tailwindcss.com https://cdn.jsdelivr.net; "
                    "style-src 'self' 'unsafe-inline' https://cdn.tailwindcss.com "
                        "https://cdn.jsdelivr.net https://fonts.googleapis.com; "
                    "font-src 'self' https://fonts.gstatic.com; "
                    "img-src 'self' data:;");
            }
        });

    // ── XSS 过滤（POST/PUT 请求 JSON body 净化）──────────────────────────
    drogon::app().registerPreHandlingAdvice(
        [](const drogon::HttpRequestPtr& req,
           drogon::AdviceCallback&&,
           drogon::AdviceChainCallback&& accb) {
            if (req->method() == drogon::Post || req->method() == drogon::Put) {
                auto body = req->getJsonObject();
                if (body) {
                    // SQL 注入特征告警（不阻断，已有参数化查询防御）
                    auto& bv = *body;
                    for (auto& key : bv.getMemberNames()) {
                        if (bv[key].isString()) {
                            const std::string& val = bv[key].asString();
                            if (XssUtils::hasSqlSignature(val)) {
                                LOG_WARN << "[SQLi] suspicious input key=" << key
                                         << " ip=" << req->peerAddr().toIp()
                                         << " path=" << req->path();
                            }
                        }
                    }
                }
            }
            accb();
        });

    // ── 接口验证（公开接口 + server-to-server）────────────────────────────
    // 规则：
    //   /challenge /forgotPassword /resetPassword → 完全放行（自带鉴权机制）
    //   公开接口(/login /captchaImage /register /forgotPassword)：
    //     浏览器客户端 → 必须携带 X-Challenge-Token（后端颁发，60s 一次性）
    //     server-to-server → 携带 X-App-Id + X-Sign（HMAC-SHA256）
    //   其他接口携带 X-App-Id → server-to-server 验签
    //   其他接口不带任何签名头 → 走 JWT 中间件
    drogon::app().registerPreHandlingAdvice(
        [](const drogon::HttpRequestPtr& req,
           drogon::AdviceCallback&& acb,
           drogon::AdviceChainCallback&& accb) {
            auto& sv = SignUtils::instance();
            const std::string& path = req->path();

            // ── WebSocket 路径早期 token 预检 ──────────────────────
            // drogon 框架在 router 匹配后才回调 handleNewConnection，
            // 此时 HTTP 已完成 WS 升级。若到 handleNewConnection 才发现
            // 缺 token 再 shutdown，攻击者可借机消耗 socket/内存资源。
            // 这里在 PreHandling 阶段先拒掉缺 token 的 WS 请求。
            // /ws/ticket 是普通 HTTP 接口（签发 WS 票据），走正常 JWT 流程
            // 只对真正的 WebSocket 升级路径（/ws/notify 等）做 token 参数预检
            if (path.size() >= 4 && path.compare(0, 4, "/ws/") == 0
                && path != "/ws/ticket") {
                if (req->getParameter("token").empty()
                    && req->getParameter("ticket").empty()) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k401Unauthorized);
                    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                    resp->setBody(R"({"code":401,"msg":"缺少 token/ticket 参数"})");
                    acb(resp); return;
                }
                // 有 token/ticket 时继续走 WebSocketController 流程
                accb(); return;
            }

            // 引导接口、重置接口、健康检查、版本接口完全放行
            if (path == "/challenge" || path == "/resetPassword" || path == "/forgotPassword"
                || path == "/health" || path == "/version" || path == "/ssl-config"
                || path == "/api/druid") {
                accb(); return;
            }
            // certmanager API + Web UI：handler 内部自行鉴权
            if (path.rfind("/api/ssl/", 0) == 0) { accb(); return; }
            if (path == "/certmanager" || path.rfind("/certmanager/", 0) == 0) { accb(); return; }
            // AI 内置助手页 + AI 健康检查 + AI 聊天后端：放行
            // /ai/page 是 InnerLink iframe 嵌入的内置 HTML 页面
            // /ai/chat 由该页面 fetch 调用，已通过 fallback 集成讯飞星火外部 API
            if (path == "/ai/page" || path == "/ai/chat"
                || path == "/ai/health" || path == "/ai/generate") {
                accb(); return;
            }

            std::string appId          = req->getHeader("X-App-Id");
            std::string challengeToken = req->getHeader("X-Challenge-Token");
            bool isPublicRoute = (path == "/login" || path == "/captchaImage"
                               || path == "/register" || path == "/sendRegCode");
            bool hasSignHeader = !appId.empty();
            bool hasChallengeToken = !challengeToken.empty();

            // 公开接口验证
            if (isPublicRoute) {
                // 方式1：浏览器挑战令牌
                if (hasChallengeToken) {
                    auto cacheKey = "challenge:" + challengeToken;
                    auto cached   = MemCache::instance().getString(cacheKey);
                    if (!cached) {
                        auto resp = drogon::HttpResponse::newHttpResponse();
                        resp->setStatusCode(drogon::k403Forbidden);
                        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                        resp->setBody("{\"code\":403,\"msg\":\"挑战令牌无效或已过期\"}");
                        acb(resp); return;
                    }
                    MemCache::instance().remove(cacheKey); // 一次性使用
                    accb(); return;
                }
                // 方式2：server-to-server HMAC 签名
                if (sv.hasApps() && hasSignHeader) {
                    std::string errMsg;
                    if (!sv.verify(req, errMsg)) {
                        LOG_WARN << "[Sign] 验签失败: " << errMsg << " path=" << path
                                 << " ip=" << IpUtils::getIpAddr(req);
                        auto resp = drogon::HttpResponse::newHttpResponse();
                        resp->setStatusCode(drogon::k403Forbidden);
                        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                        resp->setBody("{\"code\":403,\"msg\":\"" + errMsg + "\"}");
                        acb(resp); return;
                    }
                    accb(); return;
                }
                // 没有配置任何 server-to-server 应用时，公开接口直接放行
                if (!sv.hasApps()) { accb(); return; }
                // 两种方式都没有 → 拒绝
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k403Forbidden);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"code\":403,\"msg\":\"缺少访问凭证 (X-Challenge-Token 或 X-App-Id)\"}");
                acb(resp); return;
            }

            // 非公开接口：带 X-App-Id → server-to-server 验签，否则放行走 JWT
            if (!hasSignHeader || !sv.hasApps()) { accb(); return; }
            std::string errMsg;
            if (!sv.verify(req, errMsg)) {
                LOG_WARN << "[Sign] 验签失败: " << errMsg << " path=" << path
                         << " ip=" << IpUtils::getIpAddr(req);
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k403Forbidden);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody("{\"code\":403,\"msg\":\"" + errMsg + "\"}");
                acb(resp); return;
            }
            accb();
        });

    // ── 定期清理限流器过期记录（每2分钟）────────────────────────────────
    drogon::app().getLoop()->runEvery(120.0, []{
        ::RateLimiter::instance().cleanup();
    });

#ifdef __linux__
    // ── 审计队列：每3秒批量刷新到 Manticore ──────────────────────────
    drogon::app().getLoop()->runEvery(3.0, []{
        AuditQueue::instance().flush();
    });
    // ── AI 风控：每2秒批量送检评分 ────────────────────────────────────
    drogon::app().getLoop()->runEvery(2.0, []{
        AiRiskEngine::instance().flush();
    });
    // ── 风控存储：每5分钟清理过期封禁/票据/计数器 ──────────────────────
    drogon::app().getLoop()->runEvery(300.0, []{
        RiskStore::instance().cleanup();
    });
#endif // __linux__

    // ── 日志留存：每日统一清理（PG日志表+本地文件+Manticore审计）──────
    {
        auto& root = drogon::app().getCustomConfig();
        if (root.isMember("log") && root["log"].isMember("retention")) {
            auto& rt = root["log"]["retention"];
            LogRetention::Config lc;
            lc.enabled      = rt.get("enabled", true).asBool();
            lc.operLogDays  = rt.get("oper_log_days", 90).asInt();
            lc.loginLogDays = rt.get("login_log_days", 90).asInt();
            lc.localLogDays = rt.get("local_log_days", 30).asInt();
            lc.maxFiles     = rt.get("max_files", 5000).asInt();
            lc.maxTotalMb   = rt.get("max_total_mb", 2048).asInt64();
            lc.logDir       = rt.get("log_dir", "logs").asString();
            lc.archiveDir   = rt.get("archive_dir", "").asString();
            LogRetention::instance().init(lc);
        } else {
            LogRetention::instance().init({});   // 默认策略
        }
        drogon::app().getLoop()->runEvery(86400.0, []{
            LogRetention::instance().runOnce();
        });
        // 每小时兜底：文件数/总大小超限即按 mtime 从旧到新删（防日志爆量）
        drogon::app().getLoop()->runEvery(3600.0, []{
            LogRetention::instance().enforceCaps();
        });
    }

    // ── 日志全文索引：./logs → Manticore sys_logs（每3秒增量 tail）──────
    {
        auto& root = drogon::app().getCustomConfig();
        LogIndexer::Config li;
        if (root.isMember("log") && root["log"].isMember("manticore")) {
            auto& mc = root["log"]["manticore"];
            li.enabled         = mc.get("enabled", false).asBool();
            li.endpoint        = mc.get("endpoint", "").asString();
            li.index           = mc.get("index", "sys_logs").asString();
            li.logDir          = mc.get("log_dir", "./logs").asString();
            li.batchSize       = mc.get("batch_size", 500).asInt();
            li.maxLinesPerTick = mc.get("max_lines_per_tick", 20000).asInt();
        }
        // endpoint 未配置时复用 security.audit.endpoint（同一 Manticore 实例）
        if (li.endpoint.empty() && root.isMember("security") &&
            root["security"].isMember("audit"))
            li.endpoint = root["security"]["audit"]
                          .get("endpoint", "http://127.0.0.1:7700").asString();
        if (li.endpoint.empty()) li.endpoint = "http://127.0.0.1:7700";
        LogIndexer::instance().init(li);
        drogon::app().getLoop()->runEvery(3.0, []{
            LogIndexer::instance().tick();
        });
    }

    // ── 运维模块：慢SQL审计 + 集群会话 + 备份管理 ─────────────────────
    {
        auto& root = drogon::app().getCustomConfig();

        // 慢SQL审计：初始化 + 每10秒批量落库
        SlowLogQueue::Config sc;
        if (root.isMember("database") && root["database"].isMember("slow_log")) {
            auto& sl = root["database"]["slow_log"];
            sc.enabled       = sl.get("enabled", true).asBool();
            sc.alertMs       = sl.get("alert_ms", 2000).asInt64();
            sc.queueCapacity = sl.get("queue_capacity", 2000).asInt();
        }
        SlowLogQueue::instance().init(sc);
        drogon::app().getLoop()->runEvery(10.0, []{
            SlowLogQueue::instance().flush();
        });

        // 集群会话：节点标识 + 每60秒心跳续期
        std::string nodeId;
        if (root.isMember("cluster"))
            nodeId = root["cluster"].get("node_id", "").asString();
        ClusterSession::instance().init(nodeId);
        drogon::app().getLoop()->runEvery(60.0, []{
            ClusterSession::instance().heartbeat();
        });

        // 缓存一致性：Redis pub/sub 失效广播（多节点本地缓存同步）
        CacheSync::instance().start();

        // 配置热更新：绑定 /actuator/reload + config.json mtime 自动监听
        ConfigReloadHook::fn = []{
            return ConfigReloader::instance().reload();
        };
        ConfigReloader::instance().startWatcher();

        // 行为验证码：go-captcha gRPC 客户端（captcha.behavioral.enabled=true 时启用）
        {
            CaptchaClient::Config cc;
            if (root.isMember("captcha") && root["captcha"].isMember("behavioral")) {
                auto& bh = root["captcha"]["behavioral"];
                cc.enabled    = bh.get("enabled", false).asBool();
                cc.serverAddr = bh.get("server_addr", "127.0.0.1:18090").asString();
                cc.timeoutMs  = bh.get("timeout_ms", 3000).asInt();
                cc.type       = bh.get("type", "slide").asString();
            }
            CaptchaClient::instance().init(cc);
        }

        // 备份管理：初始化 + 每小时检查是否到备份时间
        BackupService::Config bc;
        if (root.isMember("backup")) {
            auto& bk = root["backup"];
            bc.enabled      = bk.get("enabled", false).asBool();
            bc.dir          = bk.get("dir", "backups").asString();
            bc.keepCount    = bk.get("keep_count", 7).asInt();
            bc.scheduleHour = bk.get("schedule_hour", 3).asInt();
        }
        // 数据库连接复用 database 段
        if (root.isMember("database")) {
            auto& d = root["database"];
            bc.dbHost = d.get("host", "127.0.0.1").asString();
            bc.dbPort = d.get("port", 5432).asInt();
            bc.dbName = d.get("dbname", "").asString();
            bc.dbUser = d.get("user", "").asString();
            bc.dbPass = d.get("passwd", "").asString();
        }
        BackupService::instance().init(bc);
        drogon::app().getLoop()->runEvery(3600.0, []{
            BackupService::instance().tickHourly();
        });

        // MQTT 客户端：设备接入/订阅转发/在线管理
        if (root.isMember("mqtt")) {
            auto& mq = root["mqtt"];
            MqttClient::Config mc;
            mc.enabled   = mq.get("enabled", false).asBool();
            mc.host      = mq.get("host", "127.0.0.1").asString();
            mc.port      = mq.get("port", 1883).asInt();
            mc.clientId  = mq.get("client_id", "").asString();
            mc.username  = mq.get("username", "").asString();
            mc.password  = mq.get("password", "").asString();
            mc.keepalive = mq.get("keepalive", 60).asInt();
            mc.forwardWs = mq.get("forward_ws", true).asBool();
            mc.persist   = mq.get("persist", false).asBool();
            mc.topics.clear();
            for (auto& t : mq["topics"]) mc.topics.push_back(t.asString());
            if (mc.topics.empty())
                mc.topics = {"device/+/status", "device/+/data"};
            MqttClient::instance().init(mc);
        }
    }

    return std::nullopt;
}

} // namespace boot
