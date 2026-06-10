/**
 * @file CommonRoutes.h
 * @brief 通用路由注册
 * 
 * 功能概述：
 *   - 上传文件服务：/profile/{dir}/{file} 路由，提供上传文件下载
 *   - 字体文件服务：/iconfont-sys.woff2 路由，提供 iconfont 字体文件
 *   - 健康检查：/health 路由，检查应用和数据库健康状态
 *   - 版本信息：/version 路由，返回应用版本信息
 * 
 * 路由列表：
 *   - GET /profile/{dir}/{file} - 下载上传的文件
 *   - GET /iconfont-sys.woff2 - 获取 iconfont 字体文件
 *   - GET /health - 健康检查（返回 UP 或 DEGRADED）
 *   - GET /version - 获取应用版本信息
 * 
 * 特性说明：
 *   - 文件缓存：iconfont 字体文件使用 std::once_flag 单次加载
 *   - 缓存控制：字体文件设置 24 小时缓存
 *   - 健康检查：支持 PostgreSQL 和 SQLite 双驱动
 *   - 故障转移：数据库连接失败时返回 503 Service Unavailable
 * 
 * @see DatabaseService - 数据库服务
 * @see MemCache - 内存缓存
 */

#pragma once
#include "AppIncludes.h"

/**
 * @brief 注册通用路由
 * 
 * 在应用启动时调用此函数，注册通用的公共路由。
 * 包括文件服务、字体文件、健康检查和版本信息等。
 * 
 * @note 此函数应在 main() 中调用
 */
inline void registerCommonRoutes() {
    // ── 上传文件服务：/profile/{dir}/{file} → uploads/{dir}/{file} ────────
    /**
     * @brief 上传文件下载处理器
     * 
     * 处理 /profile/{dir}/{file} 请求，从 uploads 目录中提供文件下载。
     * 
     * 流程：
     *   1. 构建文件路径：uploads/{dir}/{file}
     *   2. 检查文件是否存在且不是目录
     *   3. 如果文件不存在，返回 404 Not Found
     *   4. 如果文件存在，返回文件内容
     * 
     * @param dir 目录名称（例如：profile、avatar）
     * @param file 文件名称（例如：user.jpg）
     * @return 文件内容或 404 错误
     */
    auto serveUpload = [](const drogon::HttpRequestPtr &,
                          std::function<void(const drogon::HttpResponsePtr &)> &&cb,
                          const std::string &dir, const std::string &file) {
        // 构建完整的文件路径
        std::string filePath = "uploads/" + dir + "/" + file;
        
        // 检查文件是否存在且不是目录
        if (!std::filesystem::exists(filePath) || std::filesystem::is_directory(filePath)) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            cb(resp);
            return;
        }
        
        // 返回文件内容
        cb(drogon::HttpResponse::newFileResponse(filePath));
    };
    drogon::app().registerHandler("/profile/{dir}/{file}", serveUpload, {drogon::Get});

    // ── iconfont 字体文件路由 ──────────────────────────────────────────────
    /**
     * @brief iconfont 字体文件服务
     * 
     * 提供 iconfont 字体文件下载。
     * 使用 std::once_flag 确保文件只加载一次，提高性能。
     * 设置 24 小时缓存，减少网络传输。
     * 
     * 流程：
     *   1. 第一次请求时从文件系统加载 iconfont-sys.woff2
     *   2. 后续请求直接返回缓存的字体数据
     *   3. 如果文件不存在，返回 404 Not Found
     *   4. 如果文件存在，返回字体数据并设置缓存头
     * 
     * @return 字体文件内容或 404 错误
     */
    drogon::app().registerHandler("/iconfont-sys.woff2",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            // 静态变量存储字体数据，使用 once_flag 确保只加载一次
            static std::string fontData;
            static std::once_flag once;
            
            // 第一次调用时加载字体文件
            std::call_once(once, []() {
                std::ifstream f("iconfont-sys.woff2", std::ios::binary);
                if (f) {
                    // 使用迭代器读取整个文件
                    fontData = std::string(std::istreambuf_iterator<char>(f), {});
                }
            });
            
            // 构建响应
            auto resp = drogon::HttpResponse::newHttpResponse();
            if (fontData.empty()) {
                // 文件不存在，返回 404
                resp->setStatusCode(drogon::k404NotFound);
            } else {
                // 文件存在，返回字体数据并设置缓存
                resp->setContentTypeString("font/woff2");
                resp->addHeader("Cache-Control", "public,max-age=86400");  // 24 小时缓存
                resp->setBody(fontData);
            }
            cb(resp);
        }, {drogon::Get});

    // ── /health 健康检查 ──────────────────────────────────────────────────
    /**
     * @brief 应用健康检查端点
     * 
     * 检查应用和数据库的健康状态。
     * 用于负载均衡器和监控系统的健康检查。
     * 
     * 响应格式：
     *   {
     *     "status": "UP" | "DEGRADED",
     *     "db": "PostgreSQL 5.4.1 @ localhost:5432",
     *     "cache": "Redis 7.0.0 @ localhost:6379"
     *   }
     * 
     * 状态说明：
     *   - UP (200)：应用和数据库都正常
     *   - DEGRADED (503)：数据库连接失败，使用 SQLite 降级
     * 
     * @return 健康检查结果（JSON 格式）
     */
    drogon::app().registerHandler("/health",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            // 获取数据库服务实例
            auto& db = DatabaseService::instance();
            
            // 检查数据库连接状态
            bool dbOk = db.isConnected() || db.isUsingSqlite();
            
            // 构建响应 JSON
            Json::Value j;
            j["status"] = dbOk ? "UP" : "DEGRADED";
            j["db"]     = db.backendInfo();      // 数据库后端信息
            j["cache"]  = MemCache::backendInfo();  // 缓存后端信息
            
            // 返回响应
            auto resp = drogon::HttpResponse::newHttpJsonResponse(j);
            resp->setStatusCode(dbOk ? drogon::k200OK : drogon::k503ServiceUnavailable);
            cb(resp);
        }, {drogon::Get});

    // ── /version 版本信息 ──────────────────────────────────────────────────
    /**
     * @brief 应用版本信息端点
     * 
     * 返回应用的名称和版本号。
     * 用于客户端检查应用版本。
     * 
     * 响应格式：
     *   {
     *     "app": "ruoyi-cpp",
     *     "version": "1.0.0"
     *   }
     * 
     * @return 版本信息（JSON 格式）
     */
    drogon::app().registerHandler("/version",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            // 构建版本信息 JSON
            Json::Value j;
            j["app"]     = "ruoyi-cpp";
            j["version"] = "1.0.0";
            
            // 返回 JSON 响应
            cb(drogon::HttpResponse::newHttpJsonResponse(j));
        }, {drogon::Get});
}
