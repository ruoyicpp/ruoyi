<div align="center">

[English](README_EN.md) | 中文

# RuoYi-Cpp

**RuoYi 管理框架的 C++ 高性能版本**

基于 [Drogon](https://github.com/drogonframework/drogon) + PostgreSQL，与 RuoYi-Vue 前端 100% 兼容

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
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
| Nginx 依赖 |  | **可选**（内置前端托管）|

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

### 账号自助

| 功能 | API 路由 | 说明 |
|------|---------|-----|
| 登录 | `POST /login` | 用户名 + 密码 + 验证码，返回 JWT |
| LDAP 登录 | `POST /login` | 配置 `ldap.enabled=true` 后自动走 AD/LDAP 认证 |
| 注册 | `POST /register` | 自助注册，支持邮箱验证码 |
| 忘记密码 | `POST /forgotPassword` | 通过邮箱发送重置验证码 |
| 重置密码 | `POST /resetPassword` | 凭重置令牌更新密码 |
| 发送验证码 | `POST /sendRegCode` | 注册前验证邮箱有效性 |

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

主配置文件：`config.json`（参考 `config.bat.template.json`）

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

### 邮件配置（系统内配置）

登录后进入 **系统管理 → 邮件发件箱** 配置 SMTP，无需修改配置文件：

| 参数键 | 说明 | 示例值 |
|--------|-----|--------|
| `sys.email.host` | SMTP 服务器 | `smtp.qq.com` |
| `sys.email.port` | 端口（Implicit TLS）| `465` |
| `sys.email.fromName` | 发件人显示名 | `系统通知` |
| `sys.email.senders` | 发件人列表（JSON 数组）| `[{"email":"a@qq.com","authCode":"xxxx"}]` |

---

## 项目结构

```
ruoyi-cpp/
├── config.json                      # 主配置文件（参考 config.bat.template.json）
├── web/                             # 前端 dist 目录（放这里即可，无需 Nginx）
├── logs/                            # 日志目录（.log 文本 + .jsonl 结构化）
├── upload/                          # 本地上传文件目录
├── src/
│   ├── main.cc                      # 入口：中间件注册、服务初始化
│   ├── AppIncludes.h                # 全局集中 include
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
- ➕ 新增：邮件发件箱管理、忘记密码、注册邮箱验证码等功能

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

**Q：角色权限修改后不生效？**
> 后端会自动刷新在线用户的权限缓存，若仍不生效请检查 `MemCache` / Redis 连接是否正常。

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

### v1.2.0（当前）
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
