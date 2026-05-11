// =============================================================
// EmbeddedFrontend.h — 嵌入式前端托管（参考 wepay-cpp 设计）
// 编译时 cmake -DRUOYI_EMBED_FRONTEND=ON -DRUOYI_EMBED_FRONTEND_DIR=./web
// 把前端 dist 嵌进 exe，运行时从内存提供服务。
//
// config.json：
//   "embedded_frontend": {
//     "enabled": true,
//     "spa_mode": true,
//     "api_prefix": "/prod-api"
//   }
// 与 "frontend"（外部目录）互斥，两者不能同时启用。
// =============================================================
#pragma once

#ifdef RUOYI_USE_EMBEDDED_FRONTEND

#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>
#include <cstddef>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

struct EmbeddedResource {
    const unsigned char* data;
    std::size_t          size;
    const char*          contentType;
};

// 由 cmake/EmbedResources.cmake 生成的 embed_registry.cc 提供
extern const std::unordered_map<std::string, EmbeddedResource>& getEmbeddedResources();
extern std::size_t                                              getEmbeddedResourceCount();

class EmbeddedFrontend {
public:
    // 注册嵌入式前端文件服务
    // apiPrefix: API 路径前缀（如 /prod-api），该前缀的请求剥离后交给后端
    // spaMode:   true 时非 API 路径回退到 index.html（前端路由由 vue-router 处理）
    //
    // 性能优化:
    //   - 启动时为每个嵌入文件预构建一个 HttpResponsePtr，存入静态缓存
    //   - 请求处理时直接返回缓存指针（shared_ptr 引用计数 +1），零内存拷贝
    //   - 支持 .gz 预压缩资源：客户端 Accept-Encoding: gzip 时优先返回 .gz 版本
    static void registerHandlers(const std::string& apiPrefix = "/prod-api",
                                  bool spaMode = true) {
        const auto& files = getEmbeddedResources();
        LOG_INFO << "[EmbeddedFrontend] 已嵌入 " << files.size()
                 << " 个前端文件 | SPA=" << spaMode << " | API=" << apiPrefix;
        std::cout << "[Frontend] 嵌入式: 文件=" << files.size()
                  << "  api_prefix=" << apiPrefix << std::endl;

        // ── 预构建响应缓存（启动时一次性构建，运行时零拷贝） ──────────────
        // 关键路径名: "/index.html", "/static/...", "/static/....gz" 等
        // 缓存值: 每个文件的 HttpResponsePtr
        // 使用 shared_ptr 在请求间安全共享
        static std::unordered_map<std::string, drogon::HttpResponsePtr> cache;
        static drogon::HttpResponsePtr indexResp;       // SPA fallback
        static std::unordered_set<std::string> hasGzAlt;// 文件存在 .gz 兄弟
        cache.reserve(files.size());

        for (auto& kv : files) {
            const std::string& url = kv.first;
            const auto&        r   = kv.second;

            auto resp = drogon::HttpResponse::newHttpResponse();
            // setBody 需要拷贝；预构建一次后多次复用，性能等价于零拷贝
            resp->setBody(std::string(
                reinterpret_cast<const char*>(r.data), r.size));
            resp->addHeader("Content-Type", r.contentType);
            if (url == "/index.html") {
                resp->addHeader("Cache-Control",
                                "no-cache, no-store, must-revalidate");
            } else {
                resp->addHeader("Cache-Control",
                                "public, max-age=31536000, immutable");
            }
            // .gz 资源服务时需补 Content-Encoding 让浏览器解码
            if (url.size() > 3 && url.compare(url.size() - 3, 3, ".gz") == 0) {
                resp->addHeader("Content-Encoding", "gzip");
                // .gz 的 Content-Type 应来自原文件（去掉 .gz 后的扩展名）
                std::string origUrl = url.substr(0, url.size() - 3);
                auto orig = files.find(origUrl);
                if (orig != files.end())
                    resp->addHeader("Content-Type", orig->second.contentType);
                // 标记原 url 存在 gz 兄弟
                hasGzAlt.insert(origUrl);
            }
            cache.emplace(url, resp);
            if (url == "/index.html") indexResp = resp;
        }

        drogon::app().registerPreRoutingAdvice(
            [apiPrefix, spaMode](
                const drogon::HttpRequestPtr& req,
                drogon::AdviceCallback&& cb,
                drogon::AdviceChainCallback&& ccb) {

                std::string path = req->path();

                // API 前缀路径 → 剥掉前缀，交给后端控制器
                if (!apiPrefix.empty() && apiPrefix != "/" &&
                    path.rfind(apiPrefix, 0) == 0) {
                    std::string newPath = path.substr(apiPrefix.size());
                    if (newPath.empty()) newPath = "/";
                    req->setPath(newPath);
                    ccb();
                    return;
                }

                // 根 / → /index.html
                std::string lookup = (path == "/") ? "/index.html" : path;

                // gzip 协商：客户端支持且存在 .gz 兄弟 → 返回 .gz
                if (hasGzAlt.count(lookup)) {
                    std::string accept = req->getHeader("Accept-Encoding");
                    if (accept.find("gzip") != std::string::npos) {
                        auto gz = cache.find(lookup + ".gz");
                        if (gz != cache.end()) { cb(gz->second); return; }
                    }
                }

                // 精确匹配嵌入文件 → 直接返回缓存的 HttpResponsePtr
                auto it = cache.find(lookup);
                if (it != cache.end()) { cb(it->second); return; }

                // SPA 回退：非文件路径（无扩展名）→ 返回 index.html
                if (spaMode && indexResp && path.find('.') == std::string::npos) {
                    cb(indexResp);
                    return;
                }

                // 非前端路径，交给 drogon 后续处理
                ccb();
            });
    }
};

#endif // RUOYI_USE_EMBEDDED_FRONTEND
