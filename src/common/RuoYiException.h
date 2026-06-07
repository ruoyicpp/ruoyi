/**
 * @file RuoYiException.h
 * @brief 统一异常体系 — 业务异常定义
 * 
 * 功能概述：
 *   - 异常分类：按 HTTP 状态码分类异常
 *   - 统一处理：统一的异常处理和响应格式
 *   - 错误信息：包含错误消息和详细信息
 *   - JSON 响应：自动转换为 JSON 响应
 * 
 * HTTP 状态码约定：
 *   - 400：参数验证错误、格式错误
 *   - 401：未认证（未登录/Token 过期）
 *   - 403：无权限（禁止访问）
 *   - 404：资源不存在
 *   - 409：业务冲突（用户名重复、数据已存在）
 *   - 429：请求过于频繁（限流）
 *   - 500：服务器内部错误
 * 
 * 异常类型：
 *   - RuoYiException：基础异常
 *   - UnauthorizedException：未认证异常（401）
 *   - TokenExpiredException：Token 过期异常
 *   - TokenInvalidException：Token 无效异常
 *   - ForbiddenException：无权限异常（403）
 *   - NotFoundException：资源不存在异常（404）
 *   - ConflictException：业务冲突异常（409）
 *   - RateLimitException：限流异常（429）
 *   - ValidationException：参数验证异常（400）
 *   - InternalException：内部错误异常（500）
 * 
 * 使用示例：
 *   // 抛出异常
 *   throw ValidationException("用户名不能为空");
 *   throw UnauthorizedException("请先登录");
 *   throw ForbiddenException("无权限访问该资源");
 *   throw NotFoundException("用户不存在");
 *   throw ConflictException("用户名已存在");
 *   throw RateLimitException("请求过于频繁，请稍后再试");
 *   
 *   // 捕获异常
 *   try {
 *       // 业务代码
 *   } catch (const RuoYiException& e) {
 *       // 返回 JSON 响应
 *       auto resp = drogon::HttpResponse::newHttpJsonResponse(
 *           Json::Value{{"code", e.httpCode()}, {"msg", e.what()}}
 *       );
 *       resp->setStatusCode(e.httpCode());
 *       return resp;
 *   }
 * 
 * 特性：
 *   - 继承自 std::runtime_error：兼容标准异常
 *   - HTTP 状态码：每个异常都有对应的 HTTP 状态码
 *   - 详细信息：支持错误消息和详细信息
 *   - JSON 转换：自动转换为 JSON 响应
 *   - 类型名称：每个异常都有类型名称，便于识别
 * 
 * 异常处理流程：
 *   1. 业务代码抛出异常
 *   2. 异常处理器捕获异常
 *   3. 获取 HTTP 状态码和错误消息
 *   4. 转换为 JSON 响应
 *   5. 返回给客户端
 */

#pragma once
#include <string>
#include <stdexcept>
#include <map>
#include <memory>

// 前向声明
namespace drogon { using HttpRequestPtr = std::shared_ptr<class HttpRequest>; }
namespace drogon { using HttpResponsePtr = std::shared_ptr<class HttpResponse>; }

class RuoYiException : public std::runtime_error {
public:
    RuoYiException(int httpCode, const std::string& msg, const std::string& detail = "")
        : std::runtime_error(msg), httpCode_(httpCode), detail_(detail) {}

    int httpCode() const { return httpCode_; }
    const std::string& detail() const { return detail_; }

    // 转换为统一JSON响应
    std::string toJson() const;

    // 获取错误类型名称
    virtual std::string typeName() const { return "Error"; }

protected:
    int httpCode_;
    std::string detail_;
};

// ─────────────────────────────────────────────────────────────────────────────
// 认证异常 (401)
// ─────────────────────────────────────────────────────────────────────────────
class UnauthorizedException : public RuoYiException {
public:
    explicit UnauthorizedException(const std::string& msg = "未认证，请登录")
        : RuoYiException(401, msg) {}
    UnauthorizedException(const std::string& msg, const std::string& detail)
        : RuoYiException(401, msg, detail) {}
    std::string typeName() const override { return "Unauthorized"; }
};

// Token过期
class TokenExpiredException : public UnauthorizedException {
public:
    TokenExpiredException()
        : UnauthorizedException("登录已过期，请重新登录") {}
    std::string typeName() const override { return "TokenExpired"; }
};

// Token无效
class TokenInvalidException : public UnauthorizedException {
public:
    TokenInvalidException()
        : UnauthorizedException("Token无效") {}
    std::string typeName() const override { return "TokenInvalid"; }
};

// ─────────────────────────────────────────────────────────────────────────────
// 权限异常 (403)
// ─────────────────────────────────────────────────────────────────────────────
class ForbiddenException : public RuoYiException {
public:
    explicit ForbiddenException(const std::string& msg = "无权限访问该资源")
        : RuoYiException(403, msg) {}
    ForbiddenException(const std::string& msg, const std::string& detail)
        : RuoYiException(403, msg, detail) {}
    std::string typeName() const override { return "Forbidden"; }
};

// 权限不足（业务权限，非登录权限）
class InsufficientPermissionException : public ForbiddenException {
public:
    explicit InsufficientPermissionException(const std::string& requiredPerm = "")
        : ForbiddenException("权限不足"), requiredPerm_(requiredPerm) {}
    std::string typeName() const override { return "InsufficientPermission"; }
    const std::string& requiredPermission() const { return requiredPerm_; }
private:
    std::string requiredPerm_;
};

// ─────────────────────────────────────────────────────────────────────────────
// 验证异常 (400)
// ─────────────────────────────────────────────────────────────────────────────
class ValidateException : public RuoYiException {
public:
    explicit ValidateException(const std::string& msg = "参数验证失败")
        : RuoYiException(400, msg) {}
    ValidateException(const std::string& msg, const std::string& field)
        : RuoYiException(400, msg), field_(field) {}
    std::string typeName() const override { return "ValidateError"; }
    const std::string& field() const { return field_; }
private:
    std::string field_;
};

// 密码复杂度不足
class PasswordComplexityException : public ValidateException {
public:
    explicit PasswordComplexityException(const std::string& reason)
        : ValidateException("密码复杂度不足", "password") {
        (void)reason; // 实际原因已在消息中
    }
    std::string typeName() const override { return "PasswordComplexityError"; }
};

// ─────────────────────────────────────────────────────────────────────────────
// 资源不存在 (404)
// ─────────────────────────────────────────────────────────────────────────────
class NotFoundException : public RuoYiException {
public:
    explicit NotFoundException(const std::string& resource = "资源")
        : RuoYiException(404, resource + "不存在") {}
    NotFoundException(const std::string& resource, const std::string& id)
        : RuoYiException(404, resource + "不存在: " + id), resource_(resource), id_(id) {}
    std::string typeName() const override { return "NotFound"; }
    const std::string& resourceType() const { return resource_; }
    const std::string& resourceId() const { return id_; }
private:
    std::string resource_;
    std::string id_;
};

// ─────────────────────────────────────────────────────────────────────────────
// 业务冲突 (409)
// ─────────────────────────────────────────────────────────────────────────────
class ConflictException : public RuoYiException {
public:
    explicit ConflictException(const std::string& msg = "业务冲突")
        : RuoYiException(409, msg) {}
    std::string typeName() const override { return "Conflict"; }
};

// 用户名已存在
class UserExistsException : public ConflictException {
public:
    explicit UserExistsException(const std::string& username)
        : ConflictException("用户名已存在: " + username), username_(username) {}
    std::string typeName() const override { return "UserExists"; }
    const std::string& username() const { return username_; }
private:
    std::string username_;
};

// ─────────────────────────────────────────────────────────────────────────────
// 限流异常 (429)
// ─────────────────────────────────────────────────────────────────────────────
class RateLimitException : public RuoYiException {
public:
    RateLimitException()
        : RuoYiException(429, "请求过于频繁，请稍后再试") {}
    explicit RateLimitException(int waitSeconds)
        : RuoYiException(429, "请求过于频繁，请在 " + std::to_string(waitSeconds) + " 秒后重试"),
          waitSeconds_(waitSeconds) {}
    std::string typeName() const override { return "RateLimit"; }
    int waitSeconds() const { return waitSeconds_; }
private:
    int waitSeconds_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// 服务器异常 (500)
// ─────────────────────────────────────────────────────────────────────────────
class ServerException : public RuoYiException {
public:
    explicit ServerException(const std::string& msg = "服务器内部错误")
        : RuoYiException(500, msg) {}
    ServerException(const std::string& msg, const std::string& detail)
        : RuoYiException(500, msg, detail) {}
    std::string typeName() const override { return "ServerError"; }
};

// 数据库异常
class DatabaseException : public ServerException {
public:
    DatabaseException()
        : ServerException("数据库操作失败") {}
    DatabaseException(const std::string& detail)
        : ServerException("数据库操作失败", detail) {}
    std::string typeName() const override { return "DatabaseError"; }
};

// 外部服务异常
class ExternalServiceException : public ServerException {
public:
    ExternalServiceException(const std::string& service, const std::string& detail)
        : ServerException("外部服务[" + service + "]调用失败", detail), service_(service) {}
    std::string typeName() const override { return "ExternalServiceError"; }
    const std::string& serviceName() const { return service_; }
private:
    std::string service_;
};

// ─────────────────────────────────────────────────────────────────────────────
// 全局异常处理器（可在 main.cc 中注册到 Drogon）
// ─────────────────────────────────────────────────────────────────────────────
class ExceptionHandler {
public:
    static ExceptionHandler& instance() {
        static ExceptionHandler inst;
        return inst;
    }

    // 将异常转换为 AjaxResult 格式的 JSON
    static std::string toAjaxResultJson(const RuoYiException& e) {
        std::string json = "{";
        json += "\"code\":" + std::to_string(e.httpCode()) + ",";
        json += "\"msg\":\"" + std::string(e.what()) + "\",";
        json += "\"type\":\"" + e.typeName() + "\"";
        if (!e.detail().empty()) {
            json += ",\"detail\":\"" + e.detail() + "\"";
        }
        json += "}";
        return json;
    }

    // 从异常创建 HttpResponse
    std::shared_ptr<drogon::HttpResponse> toResponse(const RuoYiException& e) const;

private:
    ExceptionHandler() = default;
};

// ════════════════════════════════════════════════════════════════════════════
// 便捷宏：抛出统一异常
// ════════════════════════════════════════════════════════════════════════════
#define THROW_UNAUTH(msg)           throw UnauthorizedException(msg)
#define THROW_TOKEN_EXPIRED()       throw TokenExpiredException()
#define THROW_TOKEN_INVALID()       throw TokenInvalidException()
#define THROW_FORBIDDEN(msg)        throw ForbiddenException(msg)
#define THROW_NO_PERM(perm)         throw InsufficientPermissionException(perm)
#define THROW_VALIDATE(msg)         throw ValidateException(msg)
#define THROW_VALIDATE_FIELD(msg, f) throw ValidateException(msg, f)
#define THROW_NOT_FOUND(res)         throw NotFoundException(res)
#define THROW_NOT_FOUND_ID(res, id) throw NotFoundException(res, id)
#define THROW_CONFLICT(msg)         throw ConflictException(msg)
#define THROW_USER_EXISTS(u)        throw UserExistsException(u)
#define THROW_RATE_LIMIT()          throw RateLimitException()
#define THROW_RATE_LIMIT_SECONDS(s)  throw RateLimitException(s)
#define THROW_DB_ERROR()            throw DatabaseException()
#define THROW_DB_ERROR_DETAIL(d)    throw DatabaseException(d)
#define THROW_EXT_SERVICE(s, d)     throw ExternalServiceException(s, d)
