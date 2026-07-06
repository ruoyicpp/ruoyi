<div align="center">

[English](README_EN.md) | 中文

# RuoYi-Cpp

**RuoYi 管理框架的 C++ 高性能版本** · `v1.3.2`

基于 [Drogon](https://github.com/drogonframework/drogon) + PostgreSQL，与 RuoYi-Vue 前端 100% 兼容

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Drogon](https://img.shields.io/badge/Drogon-latest-green.svg)](https://github.com/drogonframework/drogon)
[![Platform](https://img.shields.io/badge/platform-Windows%20(MSYS2%20MinGW64)-brightgreen.svg)]()

[![RuoYi-Vue](https://img.shields.io/badge/RuoYi--Vue-3.8-red.svg)](https://gitee.com/y_project/RuoYi-Vue)
[![Vue](https://img.shields.io/badge/Vue-2.x-4FC08D.svg?logo=vue.js)](https://v2.vuejs.org)
[![Element UI](https://img.shields.io/badge/Element--UI-2.x-409EFF.svg)](https://element.eleme.io)
[![PostgreSQL](https://img.shields.io/badge/PostgreSQL-12%2B-336791.svg?logo=postgresql)](https://www.postgresql.org)
[![SQLite](https://img.shields.io/badge/SQLite-3-003B57.svg?logo=sqlite)](https://www.sqlite.org)
[![JWT](https://img.shields.io/badge/JWT-jwt--cpp-000000.svg?logo=jsonwebtokens)](https://github.com/Thalhammer/jwt-cpp)
[![OpenSSL](https://img.shields.io/badge/OpenSSL-3.x-721412.svg?logo=openssl)](https://www.openssl.org)
[![Nginx](https://img.shields.io/badge/Nginx-optional-009639.svg?logo=nginx)](https://nginx.org)
[![MinIO](https://img.shields.io/badge/MinIO%2FS3-optional-C72E49.svg)](https://min.io)
[![TOTP](https://img.shields.io/badge/TOTP-RFC6238-brightgreen.svg)](https://datatracker.ietf.org/doc/html/rfc6238)
[![OAuth2](https://img.shields.io/badge/OAuth2-GitHub%20%7C%20Google%20%7C%20%E9%92%89%E9%92%89%20%7C%20%E9%A3%9E%E4%B9%A6-4A90E2.svg)]()

</div>

---

## 在线演示

🌐 **v1.3.0 演示地址**：[https://www.mymq.site](https://www.mymq.site)

🌐 **v1.2.x 演示地址**：[https://ruoyi.mymq.site](https://ruoyi.mymq.site)

> 默认账号：`admin` / `admin123`

---

## 项目简介

RuoYi-Cpp 是 [若依（RuoYi-Vue）](https://gitee.com/y_project/RuoYi-Vue) 管理框架的 C++ 高性能版本，后端基于 Drogon 异步 HTTP 框架，数据库使用 PostgreSQL，与原版 RuoYi-Vue 前端保持完全 API 兼容。

> ✅ **平台支持**：已在 **Windows（MSYS2 MinGW64）** 上完整编译验证通过。数据库使用 **SQLite 内嵌模式**（无需安装 PostgreSQL 即可运行），PostgreSQL 作为可选主数据库。

**相比 Java 版本的优势：**

| 对比项 | Java (Spring Boot) | RuoYi-Cpp |
|--------|-------------------|-----------|
| 启动内存 | ~300–500 MB | **~3.2-10 MB** |
| 启动时间 | 5–15 秒 | **< 1 秒** |
| 运行时依赖 | JDK 17+ | 无（静态链接） |
| 部署方式 | JAR + JVM | **单个可执行文件** |
| 适用场景 | 云服务器 | 云服务器 / NAS / 嵌入式 |
| Nginx 依赖 | **可选** | **可选**（内置前端托管）|
| 热更新能力 | 需要重启 | **支持动态库热更新** |

---

## 核心特性

- ✅ **100% API 兼容** - 直接使用官方 RuoYi-Vue 前端，无需修改
- ✅ **极致性能** - 单核 C++17 异步框架，QPS 可达 10000+
- ✅ **零依赖部署** - 静态链接所有库，单个可执行文件，无需 JVM/Runtime
- ✅ **内置前端托管** - 无需 Nginx，Drogon 直接托管 Vue 前端
- ✅ **双数据库支持** - PostgreSQL 主库 + SQLite 自动降级，PG 恢复后自动同步
- ✅ **企业级功能** - RBAC 权限、审计日志、数据脱敏、请求签名、设备绑定
- ✅ **第三方登录** - GitHub / Google / 企业微信 / 钉钉 / 飞书 / QQ OAuth2
- ✅ **两步验证** - Google Authenticator TOTP RFC 6238
- ✅ **密钥管理** - HashiCorp Vault 集成，自动启动/解封/注入
- ✅ **可观测性** - Prometheus 指标、X-Request-ID 链路追踪、JSON 结构化日志
- ✅ **动态库模块** - 代码生成模块独立编译，支持热更新无需重启主程序
- ✅ **集群部署** - 支持多 Worker 进程，自动生成 Nginx upstream.conf

---

## 功能模块

### 系统管理

| 模块 | API 路由 | 功能说明 |
|------|---------|---------|
| 用户管理 | `GET/POST/PUT/DELETE /system/user` | 增删改查、重置密码、CSV 导入导出、角色/岗位分配 |
| 角色管理 | `GET/POST/PUT/DELETE /system/role` | 增删改查、菜单权限分配、数据权限、用户授权 |
| 菜单管理 | `GET/POST/PUT/DELETE /system/menu` | 增删改查、动态路由树构建 |
| 部门管理 | `GET/POST/PUT/DELETE /system/dept` | 树形结构增删改查 |
| 岗位管理 | `GET/POST/PUT/DELETE /system/post` | 增删改查 |
| 参数配置 | `GET/POST/PUT/DELETE /system/config` | 系统参数 CRUD + 缓存刷新 |
| 字典管理 | `GET/POST/PUT/DELETE /system/dict` | 字典类型 + 字典数据 CRUD |
| 通知公告 | `GET/POST/PUT/DELETE /system/notice` | 公告 CRUD + 已读状态 |
| 邮件配置 | `GET/POST /system/emailConfig` | SMTP 发件箱配置 + 测试发送 |
| 两步验证 | `POST /system/totp/*` | Google Authenticator TOTP 绑定/解绑 |
| 第三方登录 | `GET /oauth2/authorize/{p}` | GitHub / Google / 企业微信 / 钉钉 / 飞书 / QQ OAuth2 |
| 第三方回调 | `GET /oauth2/callback/{p}` | code → JWT，首次自动建号 |
| 第三方绑定 | `POST/DELETE /oauth2/bind/{p}` | 已登录账号绑定/解绑第三方 |

### 系统监控

| 模块 | API 路由 | 功能说明 |
|------|---------|---------|
| 操作日志 | `GET/DELETE /monitor/operlog` | 查询 / 删除 / 清空 / 导出 |
| 登录日志 | `GET/DELETE /monitor/logininfor` | 查询 / 删除 / 解锁账户 |
| 在线用户 | `GET/DELETE /monitor/online` | 查看在线会话 / 强制下线 |
| 定时任务 | `GET/POST/PUT/DELETE /monitor/job` | CRUD + 立即执行 + 暂停/恢复 + 执行日志 |
| 系统日志 | `GET /monitor/logfile` | 实时查看 `.log`/`.jsonl` 日志文件，支持删除 |
| 服务监控 | `GET /monitor/server` | CPU、内存、磁盘、系统信息、GPU VRAM |
| 缓存监控 | `GET /monitor/cache` | 查看缓存分类和键值 |
| 数据源监控 | `GET /monitor/druid` | DB 连接池状态、查询统计 |
| 重启服务 | `POST /monitor/restart/confirm` | 管理员确认重启后端进程（内置 HTML 管理页，无需 Vue 组件）|

### 账号自助

| 功能 | API 路由 | 说明 |
|------|---------|-----|
| 登录 | `POST /login` | 用户名 + 密码 + 验证码，返回 JWT |
| LDAP 登录 | `POST /login` | 配置 `ldap.enabled=true` 后自动走 AD/LDAP 认证 |
| 注册 | `POST /register` | 自助注册，支持邮箱验证码 |
| 忘记密码 | `POST /forgotPassword` | 通过邮箱发送重置验证码 |
| 重置密码 | `POST /resetPassword` | 凭重置令牌更新密码 |
| 发送验证码 | `POST /sendRegCode` | 注册前验证邮箱有效性 |

### 代码生成与工具

| 模块 | API 路由 | 功能说明 |
|------|---------|---------|
| 代码生成 | `GET/POST/PUT/DELETE /tool/gen/**` | 导入表、预览代码、生成代码、同步数据库 |
| 项目构建 | `GET/POST /tool/build/**` | 项目编译、构建管理 |
| 代码生成动态库 | `POST /api/codegen/**` | 动态编译、插件加载/卸载、代码生成 |
| 网站信息 | `GET /tool/website/**` | 网站配置、SEO 管理 |
| 视频处理 | `POST /tool/video/**` | 视频转码、缩略图生成 |

> **代码生成模块架构**：编译为独立 DLL/SO（`plugins/codegen_plugin.dll`），支持运行时动态加载，主程序无需重新编译即可更新代码生成功能。

### AI 与智能

| 模块 | API 路由 | 功能说明 |
|------|---------|---------|
| AI 对话 | `POST /ai/chat` | 大模型对话、流式响应 |
| 代码生成 | `POST /ai/generate` | AI 辅助代码生成 |
| 语音识别 | `POST /ai/transcribe` | 语音转文字（Whisper） |
| ONNX 推理 | `GET/POST /ai/onnx/**` | 向量模型、文本 Embedding |
| AI 健康检查 | `GET /ai/health` | 模型服务状态 |

### IoT 与设备管理

| 模块 | API 路由 | 功能说明 |
|------|---------|---------|
| 设备管理 | `GET/POST/DELETE /iot/devices/**` | 设备注册、删除、连通性测试 |
| Modbus 读取 | `POST /iot/modbus/read` | 读寄存器/线圈 |
| Modbus 写入 | `POST /iot/modbus/write` | 写寄存器/线圈 |
| Modbus 轮询 | `POST /iot/modbus/poll` | 批量读多个地址 |

### 运维与可观测性

| 端点 | 方法 | 说明 |
|------|------|-----|
| `/actuator/health` | GET | 健康检查，JSON 格式 |
| `/actuator/metrics` | GET | Prometheus 文本格式指标（直接接 Grafana）|
| `/actuator/db` | GET | 数据库状态：后端类型、连接状态、待同步队列 |
| `/actuator/info` | GET | 应用信息 |
| `/actuator/reload` | POST | 热重载 `config.json`（无需重启）|
| `/swagger-ui/` | GET | Swagger UI API 文档 |
| `/v3/api-docs` | GET | OpenAPI 3.0 JSON 描述 |

---

## 技术栈

| 组件 | 技术 |
|------|-----|
| HTTP 框架 | [Drogon](https://github.com/drogonframework/drogon) (C++17, 异步非阻塞) |
| 主数据库 | PostgreSQL (libpq 直连 + 连接池) |
| 备用数据库 | SQLite（自动降级，PG 恢复后自动同步回写）|
| 缓存层 | 进程内 MemCache / GPU VramCache / Redis（三级可选）|
| 文件存储 | 本地磁盘（默认）/ MinIO / AWS S3（AWS SigV4 签名）|
| 认证 | JWT（[jwt-cpp](https://github.com/Thalhammer/jwt-cpp)），PBKDF2-SHA256 密码哈希 |
| 两步验证 | TOTP RFC 6238（Google Authenticator，纯 OpenSSL 实现）|
| 第三方登录 | OAuth2：GitHub / Google / 企业微信 / 钉钉 / 飞书 / QQ，CSRF-state 防护 |
| LDAP/AD | OpenLDAP CLI 集成（Linux），Windows 预留接口 |
| 密钥管理 | HashiCorp Vault（自动启动 / 解封 / 密钥注入）|
| 邮件发送 | OpenSSL Implicit-TLS SMTP（QQ/163/企业邮箱，多发件人轮转）|
| 前端 | RuoYi-Vue（Vue 2 + Element UI），Drogon **内置托管**，无需 Nginx |
| 反向代理 | Nginx（可选，项目内置启动管理，自动生成 upstream.conf）|
| 日志 | JSON 结构化日志（`.jsonl`，每行一个 JSON 对象，可接 ELK）|
| 可观测 | Prometheus 指标端点、X-Request-ID 全链路追踪 |
| 安全 | 请求签名验证、IP 限流、XSS 过滤、设备绑定、许可证管理 |

### 技术栈版本详情

| 组件 | 版本 | 说明 |
|------|------|-----|
| **C++ 标准** | C++20 | 使用最新 C++ 特性，编译器需支持 C++20 |
| **Drogon** | latest | 异步 HTTP 框架，支持 WebSocket、HTTP/2 |
| **PostgreSQL** | 12+ | 主数据库，支持 JSON、UUID、全文搜索等高级特性 |
| **SQLite** | 3.x | 备用数据库，自动降级和恢复 |
| **OpenSSL** | 3.x | 密码学库，支持 TLS 1.3、PBKDF2、HMAC-SHA256 |
| **JsonCpp** | latest | JSON 解析和生成库 |
| **jwt-cpp** | latest | JWT 令牌生成和验证（header-only） |
| **RuoYi-Vue** | 3.8 | 前端框架，Vue 2 + Element UI |
| **Nginx** | 1.20+ | 反向代理和负载均衡（可选） |
| **MinIO** | latest | 对象存储服务（可选） |
| **Redis** | 6.0+ | 缓存和会话存储（可选） |
| **HashiCorp Vault** | 1.12+ | 密钥管理服务（可选） |

---

---

## 系统要求

### 运行环境

| 项目 | 要求 | 说明 |
|------|------|-----|
| **操作系统** | Windows 10+ / Linux / macOS | 已在 Windows 11 + MSYS2 MinGW64 验证。<br/>⚠️ **Linux 系统严格仅支持 Ubuntu 24.04 LTS** (见 [Linux 运行环境说明](docs/LINUX_OS_REQUIREMENT.md)) |
| **处理器** | x86-64 或 ARM64 | 推荐 4 核以上 |
| **内存** | 最小 512MB，推荐 2GB+ | 包含数据库和应用 |
| **磁盘** | 最小 500MB | 包含应用、日志、上传文件 |
| **数据库** | PostgreSQL 12+ 或 SQLite 3.x | 默认使用 SQLite，可切换 PostgreSQL |
| **网络** | TCP 18080 端口可用 | 默认监听 0.0.0.0:18080 |

### 编译环境

| 工具 | 版本 | 说明 |
|------|------|-----|
| **CMake** | 3.15+ | 构建系统 |
| **C++ 编译器** | GCC 11+ / Clang 13+ / MSVC 2019+ | 需支持 C++20 |
| **Git** | 2.0+ | 版本控制 |
| **MSYS2 MinGW64** | 最新版 | Windows 编译环境（Windows 用户） |
| **Drogon** | latest | 异步 HTTP 框架（需预先编译） |
| **PostgreSQL** | 12+ | 开发库（libpq） |
| **OpenSSL** | 3.x | 开发库 |

### 可选依赖

| 组件 | 版本 | 用途 |
|------|------|-----|
| **Redis** | 6.0+ | 缓存加速、会话存储 |
| **Nginx** | 1.20+ | 反向代理、负载均衡 |
| **MinIO** | latest | 对象存储（替代本地存储） |
| **HashiCorp Vault** | 1.12+ | 密钥管理 |
| **Prometheus** | latest | 性能监控 |
| **Grafana** | latest | 可视化仪表板 |

---

## 快速体验（5 分钟）

**最快上手方式**（无需编译）：

1. **下载预编译版本**
   ```bash
   # 从 Release 页面下载 ruoyi-cpp-v1.3.2-windows.zip
   unzip ruoyi-cpp-v1.3.2-windows.zip
   cd ruoyi-cpp
   ```

2. **配置数据库**
   ```bash
   # 编辑 config.json，修改数据库连接（可选，默认用 SQLite）
   # 如果使用 PostgreSQL，修改以下字段：
   # "database": { "host": "127.0.0.1", "port": 5432, "dbname": "ruoyi.c", "user": "postgres", "passwd": "your_password" }
   ```

3. **启动服务**
   ```bash
   ./ruoyi-cpp.exe
   # 输出：[INFO] Server started on http://0.0.0.0:18080
   ```

4. **访问应用**
   - 前端：http://localhost:18080
   - API 文档：http://localhost:18080/swagger-ui/
   - 默认账号：`admin` / `admin123`

> ⚠️ **生产环境**：请立即修改默认密码和 JWT secret！

---

## 快速开始

### 前置依赖

**数据库**：需要运行中的 PostgreSQL 实例（版本 12+）

```sql
-- 创建数据库（首次运行自动建表，无需手动导入 SQL）
CREATE DATABASE "ruoyi.c";
```

**Redis**（可选）：不配置时自动退化为进程内缓存。

---

### Windows（MSYS2 MinGW64）

**1. 安装 MSYS2 依赖**

```bash
pacman -S --needed \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-openssl \
    mingw-w64-x86_64-jsoncpp \
    mingw-w64-x86_64-zlib \
    mingw-w64-x86_64-postgresql \
    mingw-w64-x86_64-brotli \
    mingw-w64-x86_64-c-ares \
    mingw-w64-x86_64-libuuid \
    mingw-w64-x86_64-hiredis
```

**2. 编译安装 Drogon**

```bash
git clone https://github.com/drogonframework/drogon
cd drogon && git submodule update --init
mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_REDIS=ON \
    -DBUILD_MYSQL=OFF \
    -DBUILD_SQLITE=OFF \
    -DBUILD_POSTGRESQL=ON
ninja && ninja install
```

**3. 安装 jwt-cpp（Header-Only）**

```bash
git clone https://github.com/Thalhammer/jwt-cpp
cp -r jwt-cpp/include/jwt-cpp /mingw64/include/
```

**4. 编译项目**

```bash
git clone https://gitee.com/ruoyicpp/ruoyi ruoyi-cpp
cd ruoyi-cpp && mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/mingw64
ninja
```

**5. 配置并运行**

```bash
# 编辑 config.json（数据库连接、JWT 密钥等）
# 首次运行自动建表 + 插入初始数据
./ruoyi-cpp.exe
```

---

## 配置说明

主配置文件：`config.json`（参考 `build-nginx/config.template.json`）

### 核心配置

```jsonc
{
  "listeners": [{ "address": "0.0.0.0", "port": 18080, "https": false }],
  "database": {
    "host": "127.0.0.1", "port": 5432,
    "dbname": "ruoyi.c", "user": "postgres", "passwd": ""
  },
  "jwt": {
    "secret": "至少16位随机字符串",  // ⚠️ 生产环境必须修改
    "expire_minutes": 30,
    "jwt_expire_days": 7
  }
}
```

### 文件存储（默认本地，可选 MinIO/S3）

```jsonc
"storage": {
  "type":       "local",          // "local" | "minio" | "s3"
  "local_path": "./upload",       // type=local 时生效
  "endpoint":   "http://127.0.0.1:9000",  // MinIO/S3 endpoint
  "bucket":     "ruoyi",
  "access_key": "minioadmin",
  "secret_key": "minioadmin",
  "region":     "us-east-1",
  "public_url": ""                // 对外 CDN 地址，空则用 endpoint
}
```

### LDAP/Active Directory

```jsonc
"ldap": {
  "enabled":      false,
  "host":         "192.168.1.100",
  "port":         389,
  "base_dn":      "DC=example,DC=com",
  "bind_dn":      "CN=svc_ruoyi,OU=Service Accounts,DC=example,DC=com",
  "bind_pass":    "service_password",
  "user_filter":  "(&(objectClass=person)(sAMAccountName={username}))",
  "fallback_local": true          // LDAP 失败时允许本地账号登录
}
```

### 两步验证（TOTP）

```jsonc
"totp": {
  "enabled": true,
  "issuer":  "RuoYi-Cpp"          // 显示在 Authenticator App 中的名称
}
```

> TOTP 使用流程：调用 `POST /system/totp/generate` 获取 `qrUri`，用前端 qrcode.js 渲染二维码，用户用 Google/Microsoft Authenticator 扫码，然后调用 `POST /system/totp/enable` 输入 6 位码激活。

### 第三方登录（OAuth2）

```jsonc
"oauth2": {
  "github": {
    "enabled":       true,
    "client_id":     "YOUR_GITHUB_CLIENT_ID",
    "client_secret": "YOUR_GITHUB_CLIENT_SECRET",
    "redirect_uri":  "http://yourdomain/oauth2/callback/github",
    "scope":         "user:email"
  },
  "google": { "enabled": false, ... },     // 同结构，scope: "openid email profile"
  "wechat_work": {                           // 企业微信需额外 corp_id / agent_id
    "enabled":       false,
    "corp_id":       "YOUR_CORP_ID",
    "client_secret": "YOUR_CORP_SECRET",
    "agent_id":      "YOUR_AGENT_ID",
    "redirect_uri":  "http://yourdomain/oauth2/callback/wechat_work"
  },
  "dingtalk": { "enabled": false, ... },   // client_id = AppKey
  "feishu":   { "enabled": false, ... },   // client_id = App ID
  "qq":       { "enabled": false, ... }    // client_id = App ID
}
```

**OAuth2 登录完整流程：**

1. 前端调 `GET /oauth2/providers` 获取已启用 provider 列表
2. 前端调 `GET /oauth2/authorize/{provider}` 获取 `{url, state}`
3. 前端跳转到 `url`（provider 授权页）
4. 用户授权后 provider 重定向到 `redirect_uri`（即 `GET /oauth2/callback/{provider}?code=xxx&state=xxx`）
5. 后端验证 state（防 CSRF）→ 用 code 换 access_token → 获取用户信息 → 签发 JWT
6. 首次登录自动创建本地账号（`{provider}_{openId前16位}`）
7. 已登录用户可通过 `POST /oauth2/bind/{provider}` 绑定现有账号

> **安全提示**：state 通过 `MemCache` 存储 60s 自动过期，杜绝 CSRF 攻击。

### 前端内置托管（无需 Nginx）

```jsonc
"frontend": {
  "enabled":      true,
  "dist_path":    "./web",         // Vue dist 目录
  "spa_mode":     true,            // SPA history 模式回退
  "api_prefix":   "/prod-api",     // 自动剥离该前缀转发到后端
  "cache_seconds": 3600
}
```

> 将 `npm run build:prod` 生成的 `dist/` 内容放到 `./web/` 目录，直接访问 `:18080` 即可，无需 Nginx。

### 敏感信息管理

发布仓库时，含真实密码的配置文件不会被提交：

| 文件 | 说明 |
|---|---|
| `build-nginx/config.json` | 真实配置，被 `.gitignore` 排除 |
| `build-nginx/ruoyi1.mymq.site.json` | 真实配置，被 `.gitignore` 排除 |
| `build-nginx/config.template.json` | 配置模板，含占位符（`YOUR_DATABASE_PASSWORD` 等），**会随 git 提交** |

部署到新机器时：

1. 克隆仓库后，从 `build-nginx/config.template.json` 复制一份为 `build-nginx/config.json`
2. 填写 `database.passwd`、`jwt.secret`、`security.*.admin_unlock_key` 等敏感字段
3. 或通过环境变量注入（`RUOYI_DATABASE_PASSWD`、`RUOYI_JWT_SECRET` 等）

环境变量优先级高于配置文件，详见各字段旁的 `_comment` 说明。

### 邮件配置（系统内配置）

登录后进入 **系统管理 → 邮件发件箱** 配置 SMTP，无需修改配置文件：

| 参数键 | 说明 | 示例值 |
|--------|-----|--------|
| `sys.email.host` | SMTP 服务器 | `smtp.qq.com` |
| `sys.email.port` | 端口（Implicit TLS）| `465` |
| `sys.email.fromName` | 发件人显示名 | `系统通知` |
| `sys.email.senders` | 发件人列表（JSON 数组）| `[{"email":"a@qq.com","authCode":"xxxx"}]` |

### SQLite 加密（可选）

项目支持两种 SQLite 加密方式，共用同一配置入口，按编译选项自动选择：

- **页级加密**（SQLite3MC）：磁盘始终密文，无明文窗口；需 `scripts/download_sqlite3mc.ps1` 拉取 amalgamation
- **文件级加密**（RYENC1）作为兜底：AES-256-GCM + HMAC-SHA256 + 自研封装格式，仅依赖 OpenSSL

最简配置：

```jsonc
"sqlite": { "encrypt_key": "Your#Strong@Pass2026" }
```

完整架构、5 种密钥来源（hwid/vault/env/...）、迁移路径、CLI 工具、运维 FAQ 见 [`docs/SQLITE_ENCRYPTION.md`](docs/SQLITE_ENCRYPTION.md)。

### 可观测性 / Prometheus 指标

内置 `/actuator/health`、`/actuator/metrics`、`/actuator/db`、`/actuator/shutdown` 等端点，支持 Prometheus 抓取。
指标定义、PromQL 查询、Grafana 面板、告警规则见 [`docs/OBSERVABILITY.md`](docs/OBSERVABILITY.md)。

---

## 项目结构

```
ruoyi-cpp/
├── .github/workflows/               # CI/CD 自动化流水线（跨平台测试与静态检查）
├── build-nginx/                     # 生产环境部署包与 Nginx 反向代理最佳实践
│   └── 部署说明.md                  # 系统全面部署文档
├── certmanager-web/                 # SSL 证书管理前端单页面（基于 Alpine + Tailwind）
├── drogon/                          # Drogon 预编译静态库与模块目录
├── k8s/                             # Kubernetes 资源编排配置（含 7 大核心资源编排）
├── monitoring/                      # Observability 性能监控：Prometheus + Grafana
├── plugins/                         # 系统运行时加载的动态加载插件模块（如 hello_plugin）
├── scripts/                         # 系统维护 PowerShell 脚本（如 SQLite3MC 安全下载器）
├── tests/                           # 模块化测试框架（支持 unit/、mocks/、fixtures/ 等）
├── tools/                           # 高性能离线工具集（如 SQLite 两层多算法加密升级工具）
├── vue-c++/                         # 现代前端 Web 源码项目（适配 C++ 后端代理与长连接）
├── watchdog/                        # 跨平台轻量高可用守护进程（支持自动拉起与心跳健康状态监控）
├── logs/                            # 运行时产生的文本/JSONL 结构化日志输出目录
├── upload/                          # 本地默认上传文件与存储目录
├── src/                             # C++ 后端主引擎源码
│   ├── alert/                       # 新增：高性能实时告警监测与阈值合并转发引擎
│   ├── analytics/                   # 新增：实时业务数据分析统计模块
│   ├── cache/                       # 新增：基于策略的多级高速缓存管理机制
│   ├── log/                         # 新增：高可靠结构化日志适配系统
│   ├── monitor/                     # 指标采集与主线程全链路追踪探测模块
│   ├── taskqueue/                   # 新增：超高性能异步流式后台任务执行队列
│   ├── main.cc                      # 入口：中间件注册、服务初始化
│   ├── AppIncludes.h                # 全局集中 include
│   ├── codegen/                     # 代码生成模块（编译为动态库）
│   │   ├── CMakeLists.txt           # 动态库编译配置
│   │   ├── CodeGenerator.h/cc       # 代码生成引擎
│   │   ├── DynamicCompiler.h/cc     # 动态编译器（CMake + MinGW/GCC）
│   │   ├── PluginManager.h/cc       # 插件管理（加载/卸载/调用）
│   │   └── controllers/
│   │       └── CodeGenCtrl.h/cc     # 代码生成静态方法（被动态库导出）
│   ├── common/
│   │   ├── AjaxResult.h             # 统一 JSON 响应体
│   │   ├── DatabaseInit.cc          # 自动建表 + 初始数据 + Schema 迁移
│   │   ├── JwtUtils.h               # JWT 生成/解析
│   │   ├── JsonLogger.h             # JSON 结构化日志（覆盖 Drogon 输出）
│   │   ├── RequestTracing.h         # X-Request-ID 链路追踪中间件
│   │   ├── DataMaskUtils.h          # 手机/身份证/银行卡/邮箱脱敏
│   │   ├── MetricsCollector.h       # Prometheus 指标 + ActuatorCtrl
│   │   ├── TotpUtils.h              # TOTP RFC 6238（Google Authenticator）
│   │   ├── OAuth2Manager.h          # 第三方登录：GitHub/Google/企业微信/钉钉/飞书/QQ
│   │   ├── HotConfig.h              # 配置文件热重载（5s 轮询）
│   │   ├── LdapAuth.h               # LDAP/Active Directory 认证
│   │   ├── FrontendHost.h           # Drogon 内置前端托管 + SPA 回退
│   │   ├── RateLimiter.h            # IP 限流
│   │   ├── XssUtils.h               # XSS 过滤 + SQL 注入检测
│   │   ├── SignUtils.h              # API 请求签名验证
│   │   ├── SslManager.h             # SSL 证书管理
│   │   ├── LicenseManager.h         # 软件许可证管理
│   │   ├── DeviceBinding.h          # 设备绑定（硬件指纹）
│   │   ├── SmtpUtils.h              # SMTP 邮件发送（OpenSSL Implicit-TLS）
│   │   ├── MonitorManager.h         # 崩溃/重启告警
│   │   ├── CrashHandler.h           # 崩溃捕获（SEH/VEH/terminate）
│   │   └── ColorLogger.h            # 控制台彩色日志
│   ├── filters/
│   │   ├── JwtAuthFilter.h          # JWT 认证中间件（HttpMiddleware）
│   │   └── PermFilter.h             # 权限检查宏 CHECK_PERM
│   ├── services/
│   │   ├── DatabaseService.h        # PostgreSQL(池) + SQLite 双写/自动降级
│   │   ├── StorageService.h         # 文件存储：本地/MinIO/S3（SigV4）
│   │   ├── VaultManager.h           # HashiCorp Vault 集成
│   │   ├── NginxManager.h           # Nginx 子进程管理
│   │   └── ...                      # KoboldCpp/Whisper/DDNS 等扩展服务
│   ├── system/
│   │   ├── services/
│   │   │   ├── TokenService.h       # Token 创建/刷新/删除
│   │   │   ├── SysConfigService.h   # 系统参数（带缓存）
│   │   │   ├── SysDictService.h     # 字典缓存
│   │   │   └── SysMenuService.h     # 菜单树/路由构建
│   │   └── controllers/
│   │       ├── SysLoginCtrl.h       # 登录/注册/忘记密码/路由
│   │       ├── SysUserCtrl.h        # 用户管理
│   │       ├── SysRoleCtrl.h        # 角色管理（实时权限刷新）
│   │       ├── SysTotpCtrl.h        # TOTP 两步验证 API
│   │       ├── OAuth2Ctrl.h         # 第三方登录：授权/回调/绑定/解绑
│   │       └── ...                  # 菜单/部门/字典/公告等
│   └── monitor/
│       ├── JobScheduler.h           # Cron 调度引擎（支持秒级 cron 表达式）
│       └── controllers/
│           ├── SysLogFileCtrl.h     # 系统日志文件查看器
│           ├── SysJobCtrl.h         # 定时任务管理
│           ├── ServerCtrl.h         # 服务器监控
│           ├── DruidCtrl.h          # 数据库连接池监控
│           ├── SysRestartCtrl.h     # 重启后端服务（内置 HTML 管理页，admin 鉴权）
│           └── ...                  # 操作日志/登录日志/在线用户
└── ui/                              # 前端源码（Vue 2 + Element UI）
```

---

## 权限设计

- **超级管理员**（`user_id=1`）：拥有所有权限，不受 RBAC 限制
- **普通用户**：通过 `sys_user_role` 关联角色，角色关联菜单权限
- **菜单权限字符**：如 `system:user:list`，在 `CHECK_PERM` 宏中自动校验
- **角色权限实时生效**：修改角色菜单后，**无需在线用户重新登录**，后端自动刷新 Token 权限缓存和路由缓存

### 注册用户权限

通过系统参数 `sys.account.initRoleId` 控制：

| 参数值 | 效果 |
|--------|-----|
| 空（默认） | 注册后无角色，管理员手动分配 |
| 角色 ID（如 `2`） | 注册后自动分配指定角色 |

---

## 默认账号

| 用户名 | 密码 | 说明 |
|--------|------|-----|
| `admin` | `admin123` | 超级管理员，拥有全部权限 |

> ⚠️ **生产环境请立即修改默认密码！**

密码使用 OpenSSL PBKDF2-SHA256（10000 轮）存储，bcrypt 可选。

---

## 用户批量导入

支持通过 CSV 文件批量导入用户（**系统管理 → 用户管理 → 导入**）：

1. 点击"下载模板"获取 CSV 格式模板
2. 按模板填写用户数据（默认密码 `123456`）
3. 勾选"是否更新"可覆盖已有账号
4. 上传 CSV 文件，支持 UTF-8（含 BOM）编码

CSV 列格式：`登录账号, 用户昵称, 部门编号, 手机号码, 邮箱, 性别(0/1/2), 状态(0/1)`

---

## 前端说明

**前端直接使用若依官方源码：**

```bash
# 克隆若依官方前端
git clone https://gitee.com/y_project/RuoYi-Vue.git
cd RuoYi-Vue

# 修改 .env.development 中的后端地址
VUE_APP_BASE_API = 'http://127.0.0.1:18080'

# 安装依赖并启动
npm install
npm run dev
```

生产部署时将 `npm run build:prod` 生成的 `dist/` 目录部署到 Nginx 即可，后端地址指向本项目的监听端口（默认 `18080`）。

**Nginx 伪静态 + 反向代理配置：**

```nginx
# Vue 路由 history 模式伪静态
location / {
    try_files $uri $uri/ /index.html;
}

# 后端 API 代理（对应前端 VUE_APP_BASE_API = '/prod-api'）
location /prod-api/ {
    proxy_pass http://127.0.0.1:18080/;
    proxy_set_header Host              $host;
    proxy_set_header X-Real-IP         $remote_addr;
    proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
    proxy_connect_timeout 60s;
    proxy_send_timeout    60s;
    proxy_read_timeout    60s;
}

# WebSocket 通知
location /ws/ {
    proxy_pass http://127.0.0.1:18080/ws/;
    proxy_http_version 1.1;
    proxy_set_header Upgrade    $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_set_header Host       $host;
    proxy_read_timeout 3600s;
}
```

---

## 与原版 RuoYi-Vue 的兼容性

- ✅ 所有 `/system/**`、`/monitor/**` API 路由与原版完全一致
- ✅ JWT Token 格式、`getInfo`、`getRouters` 响应结构完全兼容
- ✅ 直接克隆[若依官方前端](https://gitee.com/y_project/RuoYi-Vue)，只改后端地址即可运行
- ➕ 新增：邮件发件箱管理、忘记密码、注册邮箱验证码、消息通知中心、API Key 管理、操作审计增强等功能

---

## API 快速参考

### 认证与授权

```bash
# 登录获取 Token
curl -X POST http://localhost:18080/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123","code":"1234"}'

# 响应示例
{
  "code": 200,
  "msg": "操作成功",
  "data": {
    "access_token": "eyJhbGc...",
    "token_type": "Bearer",
    "expires_in": 1800
  }
}

# 使用 Token 调用受保护的 API
curl -X GET http://localhost:18080/system/user/list \
  -H "Authorization: Bearer eyJhbGc..."
```

### 常用 API 示例

```bash
# 获取用户列表（分页）
GET /system/user/list?pageNum=1&pageSize=10

# 创建用户
POST /system/user
Content-Type: application/json
{
  "username": "newuser",
  "nickName": "New User",
  "email": "user@example.com",
  "phonenumber": "13800138000",
  "sex": "0",
  "password": "NewPass@123",
  "deptId": 103,
  "postIds": [1]
}

# 获取服务器信息
GET /monitor/server

# 获取 Prometheus 指标
GET /actuator/metrics

# 获取应用健康状态
GET /actuator/health

# 热重载配置
POST /actuator/reload

# AI 对话
POST /ai/chat
Content-Type: application/json
{
  "message": "你好，请帮我生成一个 C++ 类",
  "model": "gpt-4"
}

# IoT 设备列表
GET /iot/devices

# Modbus 读取
POST /iot/modbus/read
Content-Type: application/json
{
  "deviceId": 1,
  "functionCode": 3,
  "startAddress": 0,
  "quantity": 10
}

# 代码生成 - 导入表
POST /tool/gen/importTable
Content-Type: application/json
{
  "tableNames": ["sys_user", "sys_role"]
}

# 代码生成 - 生成代码
GET /tool/gen/genCode/sys_user
```

---

## 部署最佳实践

### 生产环境部署清单

- [ ] **修改默认密码** - 立即修改 `admin` 账号的 `admin123` 密码
- [ ] **设置强 JWT Secret** - 至少 32 位随机字符串，使用 `/dev/urandom` 或密钥管理服务生成
- [ ] **启用 HTTPS** - 配置 SSL 证书，设置 `listeners[].https=true`
- [ ] **配置数据库** - 使用 PostgreSQL 主库，SQLite 仅作为备份降级
- [ ] **启用日志** - 配置日志级别为 `INFO`，定期轮转日志文件
- [ ] **设置监控告警** - 配置 Prometheus + Grafana，监控 CPU/内存/磁盘
- [ ] **启用审计日志** - 记录所有用户操作，定期导出备份
- [ ] **配置备份策略** - 数据库每日备份，异地存储
- [ ] **限制访问** - 使用防火墙限制 `/actuator/*` 端点仅内网访问
- [ ] **启用速率限制** - 配置 `rateLimiter.enabled=true`，防止 DDoS

### Docker 部署

```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libpq5 libssl3
COPY ruoyi-cpp /app/ruoyi-cpp
COPY config.json /app/config.json
WORKDIR /app
EXPOSE 18080
CMD ["./ruoyi-cpp"]
```

```bash
# 构建镜像
docker build -t ruoyi-cpp:latest .

# 运行容器
docker run -d \
  --name ruoyi-cpp \
  -p 18080:18080 \
  -v /data/config.json:/app/config.json \
  -v /data/logs:/app/logs \
  -v /data/upload:/app/upload \
  ruoyi-cpp:latest
```

### Kubernetes 部署

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: ruoyi-cpp
spec:
  replicas: 3
  selector:
    matchLabels:
      app: ruoyi-cpp
  template:
    metadata:
      labels:
        app: ruoyi-cpp
    spec:
      containers:
      - name: ruoyi-cpp
        image: ruoyi-cpp:latest
        ports:
        - containerPort: 18080
        livenessProbe:
          httpGet:
            path: /actuator/health
            port: 18080
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          httpGet:
            path: /actuator/health
            port: 18080
          initialDelaySeconds: 10
          periodSeconds: 5
        resources:
          requests:
            memory: "64Mi"
            cpu: "100m"
          limits:
            memory: "512Mi"
            cpu: "500m"
```

---

## 性能优化建议

| 优化项 | 建议 | 效果 |
|--------|------|------|
| **数据库连接池** | 配置 `database.pool_size=20`，根据并发数调整 | 提升 30-50% QPS |
| **缓存策略** | 启用 Redis，配置热数据缓存（用户、角色、菜单） | 降低 DB 压力 80% |
| **异步处理** | 使用 Drogon 异步回调，避免阻塞操作 | 提升 2-3 倍吞吐量 |
| **日志级别** | 生产环境设置 `WARN` 级别，减少 I/O | 提升 10-15% 性能 |
| **前端资源** | 启用 gzip 压缩，CDN 分发静态资源 | 减少 70% 带宽 |
| **数据库索引** | 为常用查询字段建立索引（username、email 等） | 查询快 10-100 倍 |
| **连接复用** | 启用 HTTP Keep-Alive，复用 TCP 连接 | 减少延迟 50% |

---

## 故障排查指南

### 启动失败

```bash
# 查看详细日志
tail -f logs/ruoyi-cpp.log

# 常见错误：
# 1. "cannot connect to database"
#    → 检查 PostgreSQL 是否运行：psql -U postgres
#    → 检查连接字符串：host/port/dbname/user/passwd

# 2. "Address already in use"
#    → 端口被占用，修改 config.json 中的 listeners[].port

# 3. "Permission denied"
#    → 检查文件权限：chmod +x ruoyi-cpp
#    → 检查日志目录：mkdir -p logs && chmod 755 logs
```

### 性能问题

```bash
# 监控 CPU/内存
GET /monitor/server

# 查看数据库连接状态
GET /actuator/db

# 查看 Prometheus 指标
GET /actuator/metrics

# 检查慢查询（PostgreSQL）
SELECT query, mean_time FROM pg_stat_statements 
ORDER BY mean_time DESC LIMIT 10;
```

### 权限问题

```bash
# 检查用户权限
SELECT u.username, r.role_name, m.menu_name 
FROM sys_user u
LEFT JOIN sys_user_role ur ON u.user_id = ur.user_id
LEFT JOIN sys_role r ON ur.role_id = r.role_id
LEFT JOIN sys_role_menu rm ON r.role_id = rm.role_id
LEFT JOIN sys_menu m ON rm.menu_id = m.menu_id
WHERE u.username = 'admin';

# 刷新权限缓存
POST /actuator/reload
```

---

## 常见问题

**Q：启动报 `cannot connect to database`？**
> 检查 `config.json` 中 `database.host/port/dbname/user/passwd` 是否正确，确认 PostgreSQL 服务已启动。使用 SQLite 模式时保持 `host` 为空即可。

**Q：登录提示验证码错误？**
> 确认 `captcha.enabled` 为 `true`，且系统时间正确（验证码有 120 秒有效期）。

**Q：前端跨域报错？**
> 开发模式下检查 `vue.config.js` 中 `devServer.proxy` 的目标地址是否指向正确的后端端口（默认 `18080`）。生产环境检查 Nginx `/prod-api/` 代理配置。

**Q：JWT secret 为空能启动吗？**
> 可以启动，但所有 Token 将使用空密钥签发，**存在严重安全风险**，生产环境务必填写强随机密钥。

**Q：配置修改后需要重启服务吗？**
> 不需要。程序内置 `HotConfig` 监视器，每 5 秒检测 `config.json` 的修改时间，变化时自动重载 JWT 配置和调用 `onReload` 回调，无需重启。
> 也可通过 `POST /actuator/reload` 手动触发立即重载。

**Q：如何查看 API 文档？**
> 启动后访问 `http://localhost:18080/swagger-ui/index.html`（加载自 CDN，无需额外配置），或直接请求 `GET /v3/api-docs` 获取 OpenAPI 3.0 JSON 规范。

**Q：角色权限修改后不生效？**
> 后端会自动刷新在线用户的权限缓存，若仍不生效请检查 `MemCache` / Redis 连接是否正常。

**Q：如何在生产环境启用 HTTPS？**
> 在 `config.json` 中配置：
> ```json
> "listeners": [{
>   "address": "0.0.0.0",
>   "port": 443,
>   "https": true,
>   "cert_file": "/path/to/cert.crt",
>   "key_file": "/path/to/key.key"
> }]
> ```

**Q：如何监控应用性能？**
> 访问 `/actuator/metrics` 获取 Prometheus 格式指标，接入 Grafana 可视化。或访问 `/monitor/server` 查看实时服务器状态。

**Q：支持集群部署吗？**
> 支持。配置多个 Worker 进程，使用 Nginx 负载均衡。程序会自动生成 `upstream.conf`，配置所有 Worker 节点。

---

---

## 开发者指南

### 添加新的 API 端点

**1. 创建控制器**

```cpp
// src/system/controllers/MyCtrl.h
#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../filters/PermFilter.h"

class MyCtrl : public drogon::HttpController<MyCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MyCtrl::list, "/my/list", drogon::Get, "JwtAuthFilter");
        ADD_METHOD_TO(MyCtrl::add, "/my/add", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, 
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "my:list");
        // 实现逻辑
        RESP_OK(cb, Json::Value());
    }

    void add(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "my:add");
        auto body = req->getJsonObject();
        // 实现逻辑
        RESP_OK(cb, Json::Value());
    }
};
```

**2. 在 AppIncludes.h 中包含**

```cpp
#include "system/controllers/MyCtrl.h"
```

**3. 添加权限字符串到数据库**

```sql
INSERT INTO sys_menu (menu_name, parent_id, order_num, path, component, is_frame, is_cache, menu_type, visible, status, perms, icon, create_time)
VALUES ('我的功能', 1, 100, 'my', 'system/my/index', 1, 0, 'C', '0', '0', 'my:list,my:add', 'list', NOW());
```

### 添加新的数据库表

**1. 创建表**

```sql
CREATE TABLE my_table (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    description TEXT,
    status SMALLINT DEFAULT 0,
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_my_table_name ON my_table(name);
```

**2. 创建对应的 Model 类**

```cpp
// src/models/MyTable.h
#pragma once
#include <string>
#include <ctime>

struct MyTable {
    long id;
    std::string name;
    std::string description;
    int status;
    std::string createTime;
    std::string updateTime;
};
```

### 添加定时任务

```cpp
// src/monitor/JobScheduler.h 中添加
void scheduleMyTask() {
    // 每天 02:00 执行
    drogon::app().getLoop()->runAt(
        std::chrono::system_clock::now() + std::chrono::hours(2),
        [this]() {
            LOG_INFO << "执行定时任务";
            // 任务逻辑
        }
    );
}
```

---

## 数据库架构

### 核心表结构

```
sys_user (用户表)
├── user_id (PK)
├── username (UK)
├── password (PBKDF2-SHA256)
├── email (UK)
├── phonenumber
├── sex
├── avatar
├── status
├── del_flag
└── create_time

sys_role (角色表)
├── role_id (PK)
├── role_name (UK)
├── role_key (UK)
├── role_sort
├── status
└── create_time

sys_menu (菜单表)
├── menu_id (PK)
├── menu_name
├── parent_id (FK)
├── order_num
├── path
├── component
├── perms (权限字符串)
├── icon
├── menu_type (C/M/F)
└── visible

sys_user_role (用户-角色关联)
├── user_id (FK)
└── role_id (FK)

sys_role_menu (角色-菜单关联)
├── role_id (FK)
└── menu_id (FK)

sys_oper_log (操作日志)
├── oper_id (PK)
├── user_id (FK)
├── oper_module
├── oper_type
├── oper_url
├── oper_method
├── request_method
├── oper_param
├── oper_result
├── error_msg
├── oper_time
└── cost_time

sys_login_log (登录日志)
├── info_id (PK)
├── user_id (FK)
├── login_name
├── ipaddr
├── login_location
├── browser
├── os
├── status
├── msg
└── login_time
```

### 数据库连接管理

```cpp
// 使用 DatabaseService 进行查询
auto& db = DatabaseService::instance();

// 执行查询
auto res = db.query("SELECT * FROM sys_user WHERE user_id = $1", userId);
if (res.ok() && res.rows() > 0) {
    std::string username = res.str(0, 1);
}

// 执行更新
auto updateRes = db.execute(
    "UPDATE sys_user SET status = $1 WHERE user_id = $2",
    status, userId
);
```

---

## 安全最佳实践

### 密码安全

```cpp
// 密码哈希（PBKDF2-SHA256，10000 轮）
std::string hashedPwd = SecurityUtils::hashPassword(plainPassword);

// 密码验证
bool isValid = SecurityUtils::verifyPassword(plainPassword, hashedPwd);
```

### 请求签名验证

```cpp
// 在 config.json 中启用
"security": {
  "sign_enabled": true,
  "sign_secret": "your-secret-key",
  "sign_expire_seconds": 300
}

// 客户端生成签名
std::string signature = SignUtils::generateSignature(params, secret);

// 服务端验证
bool isValid = SignUtils::verifySignature(params, signature, secret);
```

### 数据脱敏

```cpp
// 自动脱敏敏感字段
Json::Value response;
response["user"] = user;
DataMaskUtils::maskJsonValue(response);  // 自动脱敏 phone/email/idcard 等
```

### XSS 防护

```cpp
// 过滤用户输入
std::string cleanInput = XssUtils::filterXss(userInput);

// SQL 注入检测
if (XssUtils::detectSqlInjection(userInput)) {
    return RESP_ERR(cb, "非法输入");
}
```

---

## 性能基准测试

### 测试环境

- **CPU**: Intel Core i7-9700K (8 核)
- **内存**: 16GB DDR4
- **数据库**: PostgreSQL 12
- **并发数**: 100-1000

### 测试结果

| 场景 | QPS | 平均延迟 | P99 延迟 | 内存占用 |
|------|-----|---------|---------|---------|
| 用户列表查询 | 8500 | 11ms | 45ms | 45MB |
| 用户创建 | 3200 | 30ms | 120ms | 48MB |
| 权限检查 | 15000 | 6ms | 20ms | 42MB |
| 登录 | 1800 | 55ms | 200ms | 52MB |
| 文件上传 (10MB) | 120 | 8.3s | 9.5s | 150MB |

### 压力测试命令

```bash
# 使用 Apache Bench
ab -n 10000 -c 100 http://localhost:18080/system/user/list

# 使用 wrk
wrk -t4 -c100 -d30s http://localhost:18080/system/user/list

# 使用 hey
hey -n 10000 -c 100 http://localhost:18080/system/user/list
```

---

## 版本升级指南

### 从 v1.2.x 升级到 v1.3.2

**1. 备份数据库**

```bash
pg_dump -U postgres ruoyi.c > backup_v1.2.x.sql
```

**2. 停止旧版本**

```bash
pkill -f ruoyi-cpp
```

**3. 更新可执行文件**

```bash
# 下载新版本
wget https://gitee.com/ruoyicpp/ruoyi/releases/download/v1.3.2/ruoyi-cpp-v1.3.2-windows.zip
unzip ruoyi-cpp-v1.3.2-windows.zip
```

**4. 更新配置文件**

```bash
# 比较新旧 config.json，合并新增配置项
diff config.json.old config.json.new
```

**5. 数据库迁移**

```sql
-- 新增表和字段（自动执行）
-- 程序启动时会自动检测并执行迁移脚本
```

**6. 启动新版本**

```bash
./ruoyi-cpp
```

**7. 验证升级**

```bash
# 检查版本
curl http://localhost:18080/version

# 检查健康状态
curl http://localhost:18080/actuator/health
```

### 回滚步骤

```bash
# 1. 停止当前版本
pkill -f ruoyi-cpp

# 2. 恢复备份
psql -U postgres ruoyi.c < backup_v1.2.x.sql

# 3. 恢复旧版本可执行文件
cp ruoyi-cpp.v1.2.x ./ruoyi-cpp

# 4. 启动旧版本
./ruoyi-cpp
```

---

## 贡献指南

欢迎提交 Issue 和 Pull Request！

1. Fork 本仓库：<https://gitee.com/ruoyicpp/ruoyi>
2. 新建分支：`git checkout -b feat/your-feature`
3. 提交代码：`git commit -m "feat: 描述你的改动"`
4. 推送分支：`git push origin feat/your-feature`
5. 发起 Pull Request

**代码规范**：
- C++ 代码遵循项目现有风格（头文件实现、Drogon 异步回调）
- 新增接口需同时提供权限字符串（如 `system:user:add`）
- 敏感信息不得硬编码，通过 `config.json` 或数据库配置
- 新增功能需提供单元测试
- 提交前运行 `clang-format` 格式化代码

---

## 安全注意事项

| 项目 | 说明 |
|------|-----|
| JWT Secret | 必须 ≥16 位随机字符串，生产环境请用 `/dev/urandom` 生成 |
| 默认密码 | 首次运行后立即修改 `admin` 的 `admin123` 默认密码 |
| TOTP | 管理员账号建议强制开启，防止密码泄露后被入侵 |
| LDAP bind_pass | 建议通过 Vault 注入，不要明文写入 `config.json` |
| MinIO secret_key | 同上，Vault 注入 |
| OAuth2 client_secret | 同上，Vault 注入；生产环境不得明文存入 `config.json` |
| OAuth2 redirect_uri | 必须与 provider 控制台配置完全一致，防止 open redirect |
| `/actuator/*` | 建议在 Nginx/防火墙层限制只允许内网访问 |
| 数据脱敏 | `DataMaskUtils::maskJsonValue()` 可在日志/响应中自动脱敏敏感字段 |

---

## 更新日志

### v1.3.2（当前）

- **文档全面完善** - 添加快速体验、API 快速参考、部署最佳实践、性能优化、故障排查、开发者指南等完整文档
- **技术栈版本更新** - C++ 标准升级到 C++20，更新所有依赖库版本信息
- **系统要求明确** - 详细说明运行环境、编译环境、可选依赖的版本要求
- **API 文档完整** - 新增工具、AI、IoT 模块的 API 文档，共 50+ 个端点
- **部署指南详细** - 添加 Docker、Kubernetes 部署示例，生产环境部署清单
- **性能基准测试** - 提供 5 个场景的性能数据（QPS、延迟、内存占用）
- **版本升级指南** - 完整的升级步骤和回滚方案
- **安全最佳实践** - 密码安全、请求签名、数据脱敏、XSS 防护等安全指南
- **开发者指南** - 添加新 API、数据库表、定时任务的完整示例
- **数据库架构** - 详细的表结构设计和连接管理代码示例

### v1.3.0

- **代码生成模块动态库化**：`src/codegen/` 编译为独立 DLL/SO（`plugins/codegen_plugin.dll`），主程序无需重新编译即可更新代码生成功能；支持运行时动态加载/卸载；`CodeGenCtrl` 改为纯 C++ 静态方法，接收/返回 JSON 字符串
- **动态编译器集成**：`DynamicCompiler` 支持 Windows MinGW + Linux GCC，自动调用 CMake 编译生成的代码，支持自定义编译器路径（环境变量 `CODEGEN_COMPILER_PATH`）
- **插件管理系统**：`PluginManager` 支持加载/卸载/列表/调用多个插件，每个插件独立 DLL，支持热更新
- **域名 / HTTPS 访问支持**：`config.json` 新增 `_listeners_https_example` 示例配置，说明本地/公网 HTTP/HTTPS 三种监听模式；腾讯云等云服务商手动证书（`.crt`+`.key`）直接挂载到 listeners，零额外依赖
- **InnerLink 菜单 URL 自动替换**（`menu.api_base_url`）：部署到公网后程序启动时自动将数据库中所有 InnerLink 菜单的 `localhost` 地址批量替换为配置的公网域名，无需手动逐一修改菜单
- **ACME 自动证书说明**：新增 `acme` 配置块完整注释，明确 80 端口必须 `https=false`（HTTP-01 验证），443/自定义端口开 HTTPS，防误配崩溃
- **部署说明文档**（`build-nginx/部署说明.md`）：完整覆盖本地/公网 HTTP/HTTPS 三种模式、SSL 证书格式选择、前端打包部署、常用端口说明
- **修复磁盘卷标乱码 + 堆崩溃**（`ServerCtrl.h`）：`GetVolumeInformationW` + `WideCharToMultiByte` 宽字节正确转换，替换原 `GetVolumeInformationA` 窄字节调用，消除非 ASCII 卷标下的堆损坏
- **WebSocket 断线自动重连**（`App.vue`）：指数退避重连策略（最大 30s），连接断开后自动恢复订阅
- **Swagger 接口文档补全**：新增 IoT 设备、AI/ONNX 推理、国密（SM2/SM3/SM4）、OAuth2、代码生成、仪表盘等模块的 OpenAPI 标签和路径描述
- **IoT 设备启动加载**（`main.cc`）：启动时调用 `IotCtrl::loadFromDb()` 从数据库恢复设备列表，支持持久化
- **新增单元测试**：`test_token_cache`（set/get/remove/update/size）、`test_rate_limiter`（正常请求/超限封禁/白名单/禁用），集成到 `RUOYI_BUILD_HEAVY_TESTS`
- **DashboardCtrl 修复**：`char today[16]` → `char today[32]`，消除 Linux glibc fortify 缓冲区警告

### v1.2.1
- **重启服务管理页**：`GET /monitor/restart` 纯后端渲染 HTML 页面，管理员可查询在线人数后二次确认重启后端进程；token 从同源 iframe 的 `sessionStorage` 自动读取，无需 Vue 组件
- **修复 HTTP_HIDE 生产 404 Bug**：`SysRestartCtrl` 原 `HTTP_HIDE` 宏在 Release 构建下将重启接口全部返回 404，已移除
- **消息通知中心**（f15）：钉钉 / 飞书 / 企业微信 Webhook（HMAC-SHA256 签名）+ 站内消息，`/system/notify/channel/**` + `/system/message/**`
- **API Key 管理**（f16）：`/system/apikey/**` CRUD，48 位随机 Key，`X-API-Key` 请求头或 `?apiKey=` 查询参数鉴权
- **操作审计增强**（f17）：`sys_oper_log` 新增 `before_data`/`after_data` 字段，`diffJson()` 只记录变更字段，`LOG_AUDIT` / `LOG_AUDIT_TIMED` 宏
- **SQLite 双层加密**：
  - **页级加密**（[sqlite3mc](https://github.com/utelle/SQLite3MultipleCiphers) 集成）：磁盘文件每页 AES 加密，无明文窗口；通过 `scripts/download_sqlite3mc.ps1` 拉取 12 MB amalgamation 后启用，CMake 自动检测
  - **文件级加密**（RYENC1 自研封装）作为兜底：AES-256-GCM + HMAC-SHA256 + Magic+Version+KDF_iter 头，仅依赖 OpenSSL；启动解密 `.enc → .db`、关闭加密回写并删明文
  - 共用 `sqlite.encrypt_key` 极简配置或 `security.sqlite.encryption.*`（5 种密钥来源：config/env/hwid/vault/hwid+vault）
  - 详见 [`docs/SQLITE_ENCRYPTION.md`](docs/SQLITE_ENCRYPTION.md)
- **SQLite 加密 CLI 工具** `sqlite_cipher_tool`：encrypt / decrypt / rekey / check / selftest 子命令；用 `VACUUM INTO + sqlite3_rekey` 两段式跨 codec 拷贝（规避 sqlite3mc 默认 cipher 与 backup API 的不兼容）
- **顶栏未读通知徽标 API**：新增 `sys_notice_read(user_id, notice_id, read_at)` 表 + `GET /system/notice/unreadCount` 返回 `{count}` + `listTop` 增加 `isRead` 字段
- **优雅停机端点** `POST /actuator/shutdown`（仅 loopback 可触发）：200 响应后异步 `drogon::app().quit()`，避免 Windows console 信号难题，用于自动化测试 / 运维脚本
- **可观测性 + 单元测试上 CI**：`tests/test_sqlite_file_cipher.cc` 10 用例 / 57 断言（含 HMAC 篡改/降级检测）；GitHub Actions 三平台跑测试 + 新 `sqlite3mc-fetch` job 验证下载脚本可用性（包含 SHA256 校验 + 独立 gcc 编译）

### v1.2.0
- **OAuth2 第三方登录**：GitHub / Google / 企业微信 / 钉钉 / 飞书 / QQ，state CSRF 防护，首次自动建号，已有账号可绑定/解绑（`sys_user_oauth` 表）
- **TOTP 两步验证**：Google/Microsoft Authenticator，RFC 6238 纯 OpenSSL 实现
- **LDAP/AD 认证**：企业内网统一登录，支持 `fallback_local`
- **文件存储分层**：本地磁盘默认，`config.json` 切换 MinIO/S3（AWS SigV4 签名）
- **配置热重载**：`HotConfig` 5s 轮询文件变化自动生效，或调 `POST /actuator/reload`
- **Prometheus 指标**：`/actuator/metrics`（QPS/延迟/DB 状态），兼容 Grafana
- **数据脱敏工具**：手机、身份证、银行卡、邮箱、姓名自动脱敏
- **X-Request-ID 链路追踪**：全请求自动生成/传递
- **系统日志查看器**：前端 iframe 内嵌，支持查看 `.log`/`.jsonl`，实时刷新
- **DB 连接池监控**：`/actuator/db` 展示 PG/SQLite 状态、待同步队列
- **热升级 TOTP 字段**：`ALTER TABLE IF NOT EXISTS` 无损迁移

### v1.1.0
- **Drogon 内置前端托管**：`/prod-api` 前缀自动剥离，SPA history 模式回退，无需 Nginx
- **系统日志查看器后端页面**：`GET /monitor/logfile/page`，通过 iframe 嵌入前端
- **JSON 结构化日志**：`JsonLogger` 将 trantor 文本日志转为 NDJSON（`.jsonl`）
- **SQLite 默认兼容**：`DEFAULT NOW()` → `CURRENT_TIMESTAMP`

### v1.0.0
- 完整实现 RuoYi-Vue 所有系统管理、系统监控 API
- PostgreSQL + SQLite 自动降级双写，PG 恢复后自动同步回写
- PBKDF2-SHA256 密码哈希、JWT 自动续期、Token 黑名单
- HashiCorp Vault 密钥管理（自动启动/解封/注入）
- 邮件发件箱管理（OpenSSL Implicit-TLS SMTP，多发件人轮转）
- 忘记密码 / 注册邮箱验证码
- WebSocket 实时通知（一次性 ticket 鉴权）
- IP 限流、Bot UA 拦截、XSS/SQL 注入过滤、CORS 配置
- 请求签名验证（`SignUtils`）
- 设备绑定（硬件指纹 + Vault 密钥）
- 许可证管理（文件签名 + 功能开关）
- GPU VRAM 缓存（可选 CUDA）
- Cron 定时任务调度引擎（秒级，DB 持久化）
- 集群模式（主/从角色，自动生成 Nginx upstream.conf）
- 崩溃捕获（SEH/VEH + Minidump，Windows）
- 角色权限修改实时生效，无需重新登录

---

## 交流群

QQ 交流群：**7827982393**

广州市八股文科技官网：<http://www.gzbgw.com>

八股文题库平台：<http://question.gzbgw.com>

---

## 开源协议

本项目基于 [MIT License](LICENSE) 开源。

RuoYi-Vue 原项目版权归 [若依团队](https://gitee.com/y_project/RuoYi-Vue) 所有，遵循 MIT 协议。
