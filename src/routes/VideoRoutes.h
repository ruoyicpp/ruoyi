/**
 * @file VideoRoutes.h
 * @brief 视频播放路由
 * 
 * 功能概述：
 *   - 视频开关查询：/api/video/enabled 路由，查询视频功能是否启用
 *   - 随机视频获取：/api/video/random 路由，从外部 API 获取随机视频
 *   - 视频播放器：/api/video/player 路由，提供视频播放器 HTML 页面
 * 
 * 路由列表：
 *   - GET /api/video/enabled - 查询视频功能是否启用（无需登录）
 *   - GET /api/video/random - 获取随机视频 URL（无需登录）
 *   - GET /api/video/player - 获取视频播放器页面（无需登录）
 * 
 * 特性说明：
 *   - 无需认证：所有接口都支持 CORS，无需登录即可访问
 *   - 外部 API：随机视频接口调用 api.yujn.cn 获取视频
 *   - 配置驱动：视频开关可通过 sys_config 表配置
 *   - 缓存优化：视频播放器 HTML 使用静态变量缓存
 * 
 * @see DatabaseService - 数据库服务
 */

#pragma once
#include "AppIncludes.h"

/**
 * @brief 注册视频播放路由
 * 
 * 在应用启动时调用此函数，注册视频播放相关的路由。
 * 所有接口都支持 CORS，无需登录即可访问。
 * 
 * @note 此函数应在 main() 中调用
 */
inline void registerVideoRoutes() {
    // ── 视频功能开关查询 ────────────────────────────────────────────────
    /**
     * @brief 查询视频功能是否启用
     * 
     * GET /api/video/enabled - 返回视频功能的启用状态
     * 
     * 流程：
     *   1. 从 sys_config 表查询 sys.video.enabled 配置
     *   2. 如果配置值为 "false" 或 "0"，返回 false
     *   3. 否则返回 true（默认启用）
     *   4. 支持 CORS，允许跨域访问
     * 
     * 响应格式：
     *   {
     *     "enabled": true | false
     *   }
     * 
     * @return 视频功能启用状态（JSON 格式）
     */
    drogon::app().registerHandler("/api/video/enabled",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            // 从数据库查询视频功能开关配置
            auto& db = DatabaseService::instance();
            auto res = db.queryParams(
                "SELECT config_value FROM sys_config WHERE config_key=$1 LIMIT 1",
                {"sys.video.enabled"});
            
            // 默认启用，除非配置为 false 或 0
            bool enabled = true;
            if (res.ok() && res.rows() > 0) {
                std::string val = res.str(0, 0);
                enabled = !(val == "false" || val == "0");
            }
            
            // 返回 JSON 响应
            Json::Value j;
            j["enabled"] = enabled;
            auto r = drogon::HttpResponse::newHttpJsonResponse(j);
            r->addHeader("Access-Control-Allow-Origin", "*");  // 支持 CORS
            cb(r);
        }, {drogon::Get});

    // ── 随机视频接口 ───────────────────────────────────────────────────
    /**
     * @brief 获取随机视频 URL
     * 
     * GET /api/video/random - 从外部 API 获取随机视频 URL
     * 
     * 流程：
     *   1. 创建 HTTP 客户端连接到 api.yujn.cn
     *   2. 发送请求获取随机视频
     *   3. 从响应的 Location Header 或 Body 中提取视频 URL
     *   4. 返回视频 URL（JSON 格式）
     *   5. 支持 CORS，允许跨域访问
     * 
     * 响应格式：
     *   {
     *     "ok": true | false,
     *     "url": "https://...mp4"
     *   }
     * 
     * @return 随机视频 URL（JSON 格式）
     */
    drogon::app().registerHandler("/api/video/random",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            // 创建 HTTP 客户端
            auto client = drogon::HttpClient::newHttpClient("http://api.yujn.cn");
            auto extReq = drogon::HttpRequest::newHttpRequest();
            extReq->setPath("/api/zzxjj.php");
            extReq->setParameter("type", "video");
            extReq->setMethod(drogon::Get);
            
            // 异步发送请求
            client->sendRequest(extReq,
                [cb](drogon::ReqResult result, const drogon::HttpResponsePtr& resp) {
                    Json::Value j;
                    if (result == drogon::ReqResult::Ok) {
                        // 从 Location Header 或 Body 中提取视频 URL
                        std::string url = resp->getHeader("location");
                        if (url.empty()) url = std::string(resp->body());
                        j["url"] = url;
                        j["ok"]  = true;
                    } else {
                        // 请求失败
                        j["ok"]  = false;
                        j["url"] = "";
                    }
                    
                    // 返回 JSON 响应
                    auto r = drogon::HttpResponse::newHttpJsonResponse(j);
                    r->addHeader("Access-Control-Allow-Origin", "*");  // 支持 CORS
                    cb(r);
                });
        }, {drogon::Get});

    // ── 视频播放器页面 ──────────────────────────────────────────────────
    /**
     * @brief 获取视频播放器 HTML 页面
     * 
     * GET /api/video/player - 返回视频播放器 HTML 页面
     * 
     * 流程：
     *   1. 第一次请求时从文件系统加载 web/video-player.html
     *   2. 后续请求直接返回缓存的 HTML
     *   3. 如果文件不存在，返回空页面
     *   4. 返回 HTML 页面
     * 
     * @return 视频播放器 HTML 页面
     */
    drogon::app().registerHandler("/api/video/player",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            // 静态变量缓存 HTML 内容
            static std::string html;
            { 
                // 第一次加载时读取文件
                std::ifstream _htmlf("web/video-player.html", std::ios::binary);
                if (_htmlf) {
                    html = std::string(std::istreambuf_iterator<char>(_htmlf), {});
                }
            }
            
            // 返回 HTML 页面
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeCode(drogon::CT_TEXT_HTML);
            resp->setBody(html);
            cb(resp);
        }, {drogon::Get});
}
