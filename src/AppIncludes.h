#pragma once

// ── 标准库 ────────────────────────────────────────────────────────
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <chrono>
#include <ctime>
#include <thread>
#include <iomanip>
#include <mutex>
#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#  include <shlobj.h>
#else
#  include <unistd.h>
#  include <sys/statvfs.h>
#  include <sys/resource.h>
#endif

// ── Drogon / Trantor ─────────────────────────────────────────────
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>

// ── 数据库 SQL 宏定义 ─────────────────────────────────────────────
// MySQL 和 PostgreSQL 兼容层（编译时自动转换）
#include "mysql/db_sql_map.h"

// ── 数据库运行时适配层 ───────────────────────────────────────────
// 支持 MySQL 和 PostgreSQL 同时使用（运行时动态选择）
#include "mysql/DatabaseAdapter.h"

// ── 公共基础 ─────────────────────────────────────────────────────
// 【安全模块】— 认证、授权、加密
#include "common/CrashHandler.h"       // 崩溃捕获（SEH/VEH/terminate）+ 生成dump
#include "common/JwtUtils.h"           // JWT生成/解析（HS256/RS256）
#include "common/TokenCache.h"         // Token缓存（内存+Redis双后端）
#include "common/RuoYiException.h"     // 统一异常类（Auth/Validate/NotFound等）
#include "common/ApiVersion.h"         // API版本控制 + CSRF防护 + 安全响应头 + 请求签名
#include "common/SignUtils.h"          // API请求签名验证
#include "common/RateLimiter.h"       // IP限流（滑动窗口 + 自动封禁）
#include "common/XssUtils.h"          // XSS过滤 + SQL注入检测
#include "common/DeviceBinding.h"      // 设备绑定（硬件指纹）
#include "common/SslManager.h"         // SSL证书管理（Let's Encrypt/商业证书）
#include "common/TotpUtils.h"          // TOTP两步验证（RFC6238）

// 【加密模块】— 国密与国际算法
#include "common/GmCrypto.h"           // 国密算法（SM2/SM3/SM4，SM9）

// 【认证授权】— 第三方登录
#include "common/LdapAuth.h"           // LDAP/Active Directory认证
#include "common/OAuth2Manager.h"      // OAuth2第三方登录（GitHub/Google/钉钉等）
#include "common/LicenseManager.h"      // 软件许可证管理

// 【配置与日志】— 配置管理、日志输出
#include "common/ConfigLoader.h"        // YAML/JSON配置文件加载
#include "common/ConfigValidator.h"    // 配置验证（环境变量+多环境dev/staging/prod）
#include "common/HotConfig.h"          // 配置文件热重载
#include "common/JsonLogger.h"         // JSON结构化日志（.jsonl格式，ELK友好）
#include "common/StructuredLogger.h"   // 增强型结构化日志（多输出+脱敏+异步）
#include "common/ErrorLogger.h"        // 错误日志记录（error.log）
#include "common/ColorLogger.h"        // 控制台彩色日志
#include "common/MetricsCollector.h"   // Prometheus指标收集

// 【监控运维】— 追踪、脱敏、托管
#include "common/RequestTracing.h"     // X-Request-ID 全链路追踪
#include "common/DataMaskUtils.h"       // 敏感数据脱敏（手机号/身份证/银行卡）
#include "common/FrontendHost.h"       // 前端静态文件托管
#include "common/HttpStatus.h"         // HTTP状态码全集 + 二进制风格别名（_11011=404）
#include "common/TraceContext.h"       // 分布式追踪上下文（OpenTelemetry兼容）
#include "common/DatabaseInit.h"        // 自动建表 + 初始数据

// 【性能优化】— 并发、缓存、批处理
#include "common/Performance.h"        // 性能工具（读写锁/对象池/PreparedStatement缓存/批量处理）

// ── 服务层 ────────────────────────────────────────────────────────
// 【DevOps服务】— Nginx、DDNS、容器管理
#include "services/NginxManager.h"      // Nginx进程管理（配置热加载）
#include "services/DdnsGoManager.h"     // 动态DNS客户端管理
#include "services/KoboldCppManager.h"  // KoboldCpp服务管理
#include "services/KoboldCppService.h"  // KoboldCpp API封装
#include "services/WhisperService.h"    // Whisper语音识别服务
#include "services/VaultManager.h"      // HashiCorp Vault密钥管理
#include "services/StorageService.h"    // 文件存储服务（S3/本地）

// ── System服务 ────────────────────────────────────────────────────
#include "system/services/SysConfigService.h"  // 系统配置服务
#include "system/services/SysDictService.h"  // 字典数据服务
#include "system/services/TokenService.h"      // Token管理服务

// ── 过滤器 / 调度器 ──────────────────────────────────────────────
#include "filters/JwtAuthFilter.h"    // JWT认证过滤器
#include "monitor/JobScheduler.h"       // 定时任务调度器

// ── System 控制器 ────────────────────────────────────────────────
#include "system/controllers/SysLoginCtrl.h"    // 登录/登出/注册
#include "system/controllers/SysUserCtrl.h"    // 用户管理
#include "system/controllers/SysRoleCtrl.h"    // 角色管理
#include "system/controllers/SysMenuCtrl.h"    // 菜单权限
#include "system/controllers/SysDeptCtrl.h"    // 部门管理
#include "system/controllers/SysConfigCtrl.h"    // 系统配置
#include "system/controllers/SysDictCtrl.h"    // 字典数据
#include "system/controllers/SysPostCtrl.h"    // 岗位管理
#include "system/controllers/SysNoticeCtrl.h"    // 通知公告
#include "system/controllers/ApiKeyCtrl.h"    // API密钥管理
#include "system/controllers/NotifyCtrl.h"    // 站内通知
#include "system/controllers/SysEmailConfigCtrl.h"    // 邮件配置
#include "system/controllers/SysSslConfigCtrl.h"    // SSL证书配置
#include "system/controllers/SysCertManagerCtrl.h"    // 证书管理
#include "system/controllers/SysTotpCtrl.h"    // TOTP两步验证
#include "system/controllers/OAuth2Ctrl.h"    // OAuth2第三方登录
#include "system/controllers/SysMonitorConfigCtrl.h"    // 监控配置
#include "system/controllers/UnlockScreenCtrl.h"    // 解锁屏幕
#include "system/controllers/WsTicketCtrl.h"    // WebSocket工单

// ── Monitor 控制器 ────────────────────────────────────────────────
#include "monitor/controllers/SysLogCtrl.h"    // 操作日志
#include "monitor/controllers/SysLogFileCtrl.h"    // 日志文件
#include "monitor/controllers/SysOnlineCtrl.h"    // 在线用户
#include "monitor/controllers/WsNotifyCtrl.h"    // WebSocket通知
#include "monitor/controllers/SysJobCtrl.h"    // 定时任务
#include "monitor/controllers/ServerCtrl.h"    // 服务器监控
#include "monitor/controllers/CacheCtrl.h"    // 缓存监控
#include "monitor/controllers/DruidCtrl.h"    // 数据库连接池
#include "monitor/controllers/SysIpCtrl.h"    // IP黑名单
#include "monitor/controllers/SysRestartCtrl.h"    // 服务重启
#include "monitor/controllers/OpsCtrl.h"    // 运维工具

// ── 任务队列 ──────────────────────────────────────────────────────
#include "taskqueue/TaskQueue.h"    // 异步任务队列核心
#include "taskqueue/TaskQueueCtrl.h"    // 任务队列管理API

// ── Common 控制器 ────────────────────────────────────────────────
#include "common/controllers/CommonCtrl.h"    // 通用接口（文件上传/下载）
#include "common/controllers/DashboardCtrl.h"    // 仪表盘
#include "common/controllers/LicenseApiCtrl.h"    // 许可证API
#include "common/controllers/GmCryptoCtrl.h"    // 国密API
#include "common/controllers/HealthCtrl.h"    // 健康检查

// ── Tool 控制器 ──────────────────────────────────────────────────
#include "tool/controllers/GenCtrl.h"    // 代码生成
#include "tool/controllers/BuildCtrl.h"    // 项目构建
#include "tool/controllers/WebsiteInfoCtrl.h"    // 网站信息
#include "tool/controllers/VideoCtrl.h"    // 视频处理
#include "tool/controllers/SwaggerCtrl.h"    // Swagger文档

// ── AI ────────────────────────────────────────────────────────────
// 【AI服务】— 大模型、语音识别、向量模型
#include "ai/OnnxService.h"            // ONNX向量模型服务（embedding生成）
#include "ai/controllers/AiCtrl.h"     // AI对话接口
#include "ai/AiChatCtrl.h"            // AI聊天控制器
#include "ai/OnnxCtrl.h"              // ONNX模型管理

// ── IoT ──────────────────────────────────────────────────────────
// 【物联网】— Modbus协议、设备网关
#include "iot/ModbusGateway.h"        // Modbus TCP/RTU网关
#include "iot/IotCtrl.h"              // IoT设备控制器

// ── IM 即时通讯 ──────────────────────────────────────────────────
// 【IM系统】— 唐僧叨叨 CGO DLL 集成（运行时动态加载）
#include "im/TangSengDaoDaoWrapper.h"  // IM系统C++包装类（单例）
#include "im/ImProxy.h"                // IM系统代理（生命周期管理）
#include "im/ImCtrl.h"                 // IM HTTP控制器（RESTful API）
#include "im/ImPublicCtrl.h"           // IM公开控制器（无需认证）

// ── 插件系统 ─────────────────────────────────────────────────────
// 【插件】— 用户 DLL/SO 运行时加载（前端触发热激活）
#include "libs/plugin/PluginCtrl.h"   // 插件管理 API
