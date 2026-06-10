/**
 * @file ErrorPage.h
 * @brief 统一错误页面渲染 — 生成美观的 HTML 错误页面
 * 
 * 功能概述：
 *   - 根据 HTTP 状态码生成对应的错误页面
 *   - 所有错误页为纯 HTML + CSS，无 JavaScript
 *   - 内嵌 CSS 样式，无需外部文件
 *   - 响应式设计，支持移动设备
 *   - 兼容所有现代浏览器
 * 
 * 支持的错误码：
 *   - 400 Bad Request - 请求参数错误
 *   - 401 Unauthorized - 认证失败
 *   - 403 Forbidden - 无权限访问
 *   - 404 Not Found - 资源不存在
 *   - 405 Method Not Allowed - 请求方法不允许
 *   - 413 Request Entity Too Large - 请求体过大
 *   - 500 Internal Server Error - 服务器内部错误
 *   - 502 Bad Gateway - 网关错误
 *   - 503 Service Unavailable - 服务暂不可用
 * 
 * 页面特性：
 *   - 渐变背景：蓝色到紫色的渐变
 *   - 动画效果：错误码浮动动画
 *   - 主题色：根据错误类型使用不同的主题色
 *   - 响应式：自适应桌面和移动设备
 *   - 无依赖：纯 HTML + CSS，无外部库
 * 
 * 使用示例：
 *   ```cpp
 *   // 在路由处理中使用
 *   auto resp = ErrorPage::build(drogon::k404NotFound);
 *   cb(resp);
 *   ```
 * 
 * @see drogon::HttpResponse - Drogon HTTP 响应类
 * @see drogon::HttpStatusCode - HTTP 状态码枚举
 */

#pragma once

#include <string>
#include <unordered_map>
#include <drogon/HttpResponse.h>

/**
 * @namespace ErrorPage
 * @brief 错误页面生成命名空间
 */
namespace ErrorPage {

/**
 * @brief 根据 HTTP 状态码生成错误页面响应
 * 
 * 根据传入的 HTTP 状态码，生成对应的美观错误页面。
 * 如果状态码不在支持列表中，默认返回 500 错误页面。
 * 
 * 页面内容包括：
 *   - 错误码（大号显示，带渐变色和浮动动画）
 *   - 错误标题（如 "404 资源不存在"）
 *   - 错误描述（主提示语）
 *   - 详细说明（副提示语）
 *   - 返回首页按钮
 * 
 * @param code HTTP 状态码（支持 400/401/403/404/405/413/500/502/503）
 * @return drogon::HttpResponsePtr HTTP 响应对象
 *         - Content-Type: text/html; charset=utf-8
 *         - Status Code: 对应的 HTTP 状态码
 *         - Body: 完整的 HTML 错误页面
 * 
 * @note 
 *   - 如果传入不支持的状态码，自动降级到 500 错误页面
 *   - 所有页面都是自包含的，无需外部资源
 *   - 页面大小约 2-3 KB，加载速度快
 * 
 * @example
 *   ```cpp
 *   // 在 404 处理中使用
 *   void notFound(const drogon::HttpRequestPtr &req,
 *                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
 *       auto resp = ErrorPage::build(drogon::k404NotFound);
 *       cb(resp);
 *   }
 *   ```
 */
drogon::HttpResponsePtr build(drogon::HttpStatusCode code);

}  // namespace ErrorPage
