/**
 * @file ErrorPage.cc
 * @brief 统一错误页面渲染实现
 * 
 * 实现了根据 HTTP 状态码生成美观错误页面的功能。
 * 所有错误页面都是自包含的 HTML + CSS，无需外部资源。
 * 
 * 页面设计特点：
 *   - 现代化设计：渐变背景、阴影、圆角
 *   - 动画效果：错误码浮动动画
 *   - 响应式布局：自适应桌面和移动设备
 *   - 快速加载：无 JavaScript，纯 HTML + CSS
 *   - 易于定制：支持主题色自定义
 */

#include "ErrorPage.h"

namespace {

/**
 * @brief 生成通用错误页面 HTML
 * 
 * 根据参数生成完整的 HTML 错误页面。页面包含：
 *   - 响应式 Meta 标签
 *   - 内嵌 CSS 样式（无外部文件）
 *   - 错误码显示（带渐变色和浮动动画）
 *   - 错误标题和描述
 *   - 返回首页按钮
 * 
 * CSS 特性：
 *   - 使用 Flexbox 居中布局
 *   - 渐变背景（蓝色到紫色）
 *   - 浮动动画（3 秒循环）
 *   - 按钮悬停效果
 *   - 移动设备适配（480px 以下）
 * 
 * @param code        HTTP 状态码数字（如 404）
 * @param title       页面标题（如 "404 资源不存在"）
 * @param msg         主提示语（如 "资源不存在"）
 * @param subMsg      副提示语（详细说明，可为空）
 * @param accentColor 主题渐变色起始值（如 "#667eea"）
 * @return 完整的 HTML 字符串（自包含，无外部依赖）
 * 
 * @note
 *   - HTML 使用 R"(...)" 原始字符串，便于维护
 *   - CSS 内嵌在 <style> 标签中
 *   - 支持所有现代浏览器（Chrome、Firefox、Safari、Edge）
 *   - 页面大小约 2-3 KB
 */
std::string makePage(int code, const std::string& title,
                     const std::string& msg,
                     const std::string& subMsg,
                     const std::string& accentColor) {
    return R"(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>)" + title + R"(</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;
display:flex;align-items:center;justify-content:center;min-height:100vh;
background:linear-gradient(135deg,#f5f7fa 0%,#c3cfe2 100%);color:#374151}
.container{text-align:center;padding:2rem;max-width:520px}
.code{font-size:7rem;font-weight:800;line-height:1;
background:linear-gradient(135deg,)" + accentColor + R"(,#764ba2);
-webkit-background-clip:text;-webkit-text-fill-color:transparent;
background-clip:text;animation:float 3s ease-in-out infinite}
@keyframes float{0%,100%{transform:translateY(0)}50%{transform:translateY(-10px)}}
.msg{font-size:1.5rem;margin-top:1rem;font-weight:600;color:#1f2937}
.sub{font-size:.95rem;margin-top:.75rem;color:#6b7280;line-height:1.6}
.divider{width:60px;height:3px;background:linear-gradient(135deg,)" + accentColor + R"(,#764ba2);
margin:1.5rem auto;border-radius:2px}
.home-btn{display:inline-block;margin-top:2rem;padding:.75rem 2.5rem;
background:linear-gradient(135deg,)" + accentColor + R"(,#764ba2);color:#fff;
border:none;border-radius:50px;font-size:1rem;cursor:pointer;text-decoration:none;
transition:all .3s;box-shadow:0 4px 15px rgba(102,126,234,.4)}
.home-btn:hover{transform:translateY(-2px);box-shadow:0 6px 20px rgba(102,126,234,.6)}
.home-btn:active{transform:translateY(0)}
.icon{font-size:3rem;margin-bottom:.5rem}
@media(max-width:480px){.code{font-size:4.5rem}.msg{font-size:1.2rem}.sub{font-size:.85rem}}
</style>
</head>
<body>
<div class="container">
<div class="icon">&#9888;</div>
<div class="code">)" + std::to_string(code) + R"(</div>
<div class="msg">)" + msg + R"(</div>
<div class="divider"></div>
<div class="sub">)" + subMsg + R"(</div>
<a href="/" class="home-btn">&#8592; 返回首页</a>
</div>
</body>
</html>)";
}

}  // anonymous namespace

namespace ErrorPage {

/**
 * @brief 根据 HTTP 状态码生成错误页面响应
 * 
 * 实现了错误页面的生成逻辑。支持以下错误码：
 * 
 * | 状态码 | 标题 | 主提示 | 主题色 |
 * |--------|------|--------|--------|
 * | 400 | 请求参数错误 | 请求参数错误 | #e74c3c (红色) |
 * | 401 | 认证失败 | 认证失败，请重新登录 | #f39c12 (橙色) |
 * | 403 | 无权限访问 | 无权限访问 | #9b59b6 (紫色) |
 * | 404 | 资源不存在 | 资源不存在 | #3498db (蓝色) |
 * | 405 | 方法不允许 | 请求方法不允许 | #e67e22 (深橙) |
 * | 413 | 请求体过大 | 请求体过大 | #e74c3c (红色) |
 * | 500 | 服务器内部错误 | 服务器内部错误 | #34495e (深灰) |
 * | 502 | 网关错误 | 网关错误，请稍后重试 | #7f8c8d (灰色) |
 * | 503 | 服务暂不可用 | 服务暂不可用，请稍后重试 | #95a5a6 (浅灰) |
 * 
 * 流程：
 *   1. 在静态映射表中查找状态码
 *   2. 如果不存在，递归调用返回 500 错误页面
 *   3. 从映射表中提取页面配置（标题、提示语、主题色）
 *   4. 调用 makePage() 生成 HTML 内容
 *   5. 创建 HTTP 响应对象
 *   6. 设置 Content-Type 为 text/html
 *   7. 设置响应体和状态码
 *   8. 返回响应对象
 * 
 * @param code HTTP 状态码（drogon::HttpStatusCode 枚举值）
 * @return drogon::HttpResponsePtr 
 *         - 包含完整 HTML 错误页面的 HTTP 响应
 *         - Content-Type: text/html; charset=utf-8
 *         - Status Code: 对应的 HTTP 状态码
 * 
 * @note
 *   - 如果传入不支持的状态码，自动返回 500 错误页面
 *   - 所有页面都是自包含的，无需外部资源
 *   - 页面大小约 2-3 KB，加载速度快
 *   - 支持所有现代浏览器
 */
drogon::HttpResponsePtr build(drogon::HttpStatusCode code) {
    // 错误页面配置映射表
    // 键：HTTP 状态码
    // 值：(HTTP码数字, 页面标题, 主提示语, 副提示语, 主题色)
    static const std::unordered_map<drogon::HttpStatusCode,
                                   std::tuple<int, std::string, std::string, std::string, std::string>> pages = {
        // 400 Bad Request - 请求参数错误
        { drogon::k400BadRequest,
          { 400, "400 请求参数错误", "请求参数错误",
            "您提交的数据格式不正确或缺少必需参数，请检查后重试。",
            "#e74c3c" }},  ///< 红色主题
        
        // 401 Unauthorized - 认证失败
        { drogon::k401Unauthorized,
          { 401, "401 认证失败", "认证失败，请重新登录",
            "登录已过期或身份验证未通过，请返回登录页重新认证。",
            "#f39c12" }},  ///< 橙色主题
        
        // 403 Forbidden - 无权限访问
        { drogon::k403Forbidden,
          { 403, "403 无权限访问", "无权限访问",
            "您没有执行此操作的权限，如有需要请联系管理员授权。",
            "#9b59b6" }},  ///< 紫色主题
        
        // 404 Not Found - 资源不存在
        { drogon::k404NotFound,
          { 404, "404 资源不存在", "资源不存在",
            "您访问的页面或接口不存在，可能已被移除或地址有误。",
            "#3498db" }},  ///< 蓝色主题
        
        // 405 Method Not Allowed - 请求方法不允许
        { drogon::k405MethodNotAllowed,
          { 405, "405 方法不允许", "请求方法不允许",
            "当前请求方式不被支持，请使用正确的 HTTP 方法。",
            "#e67e22" }},  ///< 深橙色主题
        
        // 413 Request Entity Too Large - 请求体过大
        { drogon::k413RequestEntityTooLarge,
          { 413, "413 请求体过大", "请求体过大",
            "上传文件或提交数据超过服务器限制，请减小内容后重试。",
            "#e74c3c" }},  ///< 红色主题
        
        // 500 Internal Server Error - 服务器内部错误
        { drogon::k500InternalServerError,
          { 500, "500 服务器内部错误", "服务器内部错误",
            "服务器遇到了意外情况，请稍后刷新页面或联系管理员。",
            "#34495e" }},  ///< 深灰色主题
        
        // 502 Bad Gateway - 网关错误
        { drogon::k502BadGateway,
          { 502, "502 网关错误", "网关错误，请稍后重试",
            "上游服务器响应异常，请稍后刷新，如问题持续请联系管理员。",
            "#7f8c8d" }},  ///< 灰色主题
        
        // 503 Service Unavailable - 服务暂不可用
        { drogon::k503ServiceUnavailable,
          { 503, "503 服务暂不可用", "服务暂不可用，请稍后重试",
            "服务器正在维护或负载过高，预计很快恢复，请稍后再试。",
            "#95a5a6" }},  ///< 浅灰色主题
    };

    // 在映射表中查找状态码
    auto it = pages.find(code);
    if (it == pages.end()) {
        // 不支持的状态码，递归返回 500 错误页面
        return build(drogon::k500InternalServerError);
    }

    // 从映射表中提取页面配置
    int httpCode;
    std::string title, msg, sub, accent;
    std::tie(httpCode, title, msg, sub, accent) = it->second;

    // 生成 HTML 内容
    std::string html = makePage(httpCode, title, msg, sub, accent);

    // 创建 HTTP 响应对象
    auto resp = drogon::HttpResponse::newHttpResponse();
    
    // 设置 Content-Type 为 text/html
    resp->setContentTypeCode(drogon::CT_TEXT_HTML);
    
    // 设置响应体（HTML 内容）
    resp->setBody(html);
    
    // 设置 HTTP 状态码
    resp->setStatusCode(code);
    
    // 返回响应对象
    return resp;
}

}  // namespace ErrorPage
