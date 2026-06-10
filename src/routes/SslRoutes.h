/**
 * @file SslRoutes.h
 * @brief SSL/HTTPS 配置管理路由
 * 
 * 功能概述：
 *   - SSL 配置管理：提供 SSL/HTTPS 证书配置管理界面
 *   - 令牌验证：支持多种令牌来源（Header、Cookie、Query 参数）
 *   - 管理员认证：仅允许已认证的管理员访问
 *   - localhost 白名单：开发环境可直接访问
 * 
 * 路由信息：
 *   - 路径：/ssl-config
 *   - 方法：GET
 *   - 认证：需要有效的 JWT 令牌或 localhost 访问
 *   - 返回：HTML 配置管理页面
 * 
 * 令牌来源优先级：
 *   1. Authorization 请求头（标准 Bearer 令牌）
 *   2. Cookie 中的 Admin-Token
 *   3. Query 参数中的 token
 * 
 * 认证流程：
 *   1. 尝试从请求中提取令牌
 *   2. 解析 JWT 令牌，验证有效性
 *   3. 如果令牌无效，检查请求来源 IP 是否在 localhost 白名单中
 *   4. 认证失败返回 401 Unauthorized
 *   5. 认证成功加载 web/ssl-config.html 页面
 * 
 * 使用示例：
 *   // 浏览器访问（需要登录）
 *   http://localhost:18080/ssl-config?token=eyJhbG...
 *   
 *   // 或使用 Cookie
 *   curl -H "Cookie: Admin-Token=eyJhbG..." http://localhost:18080/ssl-config
 * 
 * @see AuthHelper - 认证辅助函数
 * @see SecurityUtils - 安全工具类
 * @see JwtUtils - JWT 工具类
 */

#pragma once
#include "AppIncludes.h"

/**
 * @brief 注册 SSL/HTTPS 配置管理路由
 * 
 * 在应用启动时调用此函数，注册 /ssl-config 路由。
 * 该路由提供 SSL/HTTPS 证书配置管理的 Web 界面。
 * 
 * 路由处理流程：
 *   1. 提取令牌（从 Header、Cookie 或 Query 参数）
 *   2. 验证令牌有效性（JWT 解析和缓存检查）
 *   3. 如果令牌无效，检查 localhost 白名单
 *   4. 认证失败返回 401 错误页面
 *   5. 认证成功加载并返回 SSL 配置管理页面
 * 
 * @note 
 *   - 此函数应在 main() 中调用
 *   - 需要 web/ssl-config.html 文件存在
 *   - localhost 白名单包括：127.0.0.1、::1（IPv6）、0.0.0.0
 * 
 * @see main.cc - 应用程序入口
 */
inline void registerSslRoutes() {
    // ── SSL/HTTPS 配置管理页（无需前端，浏览器直接访问）─────────────────
    drogon::app().registerHandler("/ssl-config",
        [](const drogon::HttpRequestPtr& req,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            // 第一步：从多个来源提取令牌
            auto token = SecurityUtils::getToken(req);
            
            // 如果 Header 中没有令牌，尝试从 Cookie 中提取
            if (token.empty()) {
                const std::string& cookieHdr = req->getHeader("cookie");
                const std::string key = "Admin-Token=";
                auto pos = cookieHdr.find(key);
                if (pos != std::string::npos) {
                    pos += key.size();
                    auto end = cookieHdr.find(';', pos);
                    token = cookieHdr.substr(pos, end == std::string::npos ? end : end - pos);
                }
            }
            
            // 最后尝试从 Query 参数中提取
            if (token.empty()) token = req->getParameter("token");
            
            // 第二步：验证令牌有效性
            bool ok = false;
            if (!token.empty()) {
                try {
                    // 解析 JWT 令牌，提取用户 UUID
                    auto uuid    = JwtUtils::parseUuid(token);
                    // 从 TokenCache 中验证令牌有效性
                    auto userKey = SecurityUtils::getTokenKey(uuid);
                    ok = (bool)TokenCache::instance().get(userKey);
                } catch (...) {
                    // 令牌解析或验证失败，继续检查 localhost 白名单
                }
            }
            
            // 第三步：如果令牌无效，检查 localhost 白名单
            if (!ok) {
                const auto& peer = req->getPeerAddr().toIp();
                ok = (peer == "127.0.0.1" || peer == "::1" || peer == "0.0.0.0");
            }
            
            // 第四步：认证失败，返回 401 错误页面
            if (!ok) {
                auto r = drogon::HttpResponse::newHttpResponse();
                r->setStatusCode(drogon::k401Unauthorized);
                r->setContentTypeCode(drogon::CT_TEXT_HTML);
                r->setBody("<html><body style='font-family:sans-serif;text-align:center;padding:60px'>"
                           "<h2>&#128274; 请先登录后携带 token 访问</h2>"
                           "<p>示例：/ssl-config?token=eyJhbG...</p></body></html>");
                cb(r); return;
            }
            
            // 第五步：认证成功，加载 SSL 配置管理页面
            std::string html;
            { 
                // 从文件系统加载 HTML 页面
                std::ifstream _htmlf("web/ssl-config.html", std::ios::binary);
                if (_htmlf) {
                    // 使用迭代器读取整个文件
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
