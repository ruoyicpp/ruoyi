<div align="center">

English | [中文](README.md)

# RuoYi-Cpp

**A High-Performance C++ Version of the RuoYi Management Framework** · `v1.3.2`

Based on [Drogon](https://github.com/drogonframework/drogon) + PostgreSQL, 100% compatible with the RuoYi-Vue frontend

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
[![OAuth2](https://img.shields.io/badge/OAuth2-GitHub%20%7C%20Google%20%7C%20DingTalk%20%7C%20Feishu-4A90E2.svg)]()

</div>

---

## Live Demo

🌐 **v1.3.0 Demo**: [https://ruoyi1.mymq.site:20443](https://ruoyi1.mymq.site:20443)

🌐 **v1.2.x Demo**: [https://ruoyi.mymq.site](https://ruoyi.mymq.site)

> Default credentials: `admin` / `admin123`

---

## Overview

RuoYi-Cpp is a high-performance C++ version of the [RuoYi-Vue](https://gitee.com/y_project/RuoYi-Vue) management framework. The backend is built on the Drogon asynchronous HTTP framework with PostgreSQL as the primary database, maintaining full API compatibility with the original RuoYi-Vue frontend.

> ✅ **Platform**: Fully compiled and verified on **Windows (MSYS2 MinGW64)**. Supports **embedded SQLite mode** (no PostgreSQL required to run), with PostgreSQL as an optional primary database.

**Advantages over the Java version:**

| Metric | Java (Spring Boot) | RuoYi-Cpp |
|--------|-------------------|-----------|
| Memory usage | ~300–500 MB | **~3.2–10 MB** |
| Startup time | 5–15 s | **< 1 s** |
| Runtime dependency | JDK 17+ | None (statically linked) |
| Deployment | JAR + JVM | **Single executable** |
| Target environment | Cloud servers | Cloud / NAS / Embedded |
| Nginx dependency | Optional  | **Optional** (built-in frontend hosting) |
| Hot-update capability | Requires restart | **Supports dynamic library hot-update** |

---

## Core Features

- ✅ **100% API Compatible** - Use official RuoYi-Vue frontend directly, no modifications needed
- ✅ **Extreme Performance** - Single-core C++17 async framework, QPS up to 10000+
- ✅ **Zero-Dependency Deployment** - Statically linked, single executable, no JVM/Runtime required
- ✅ **Built-in Frontend Hosting** - No Nginx needed, Drogon hosts Vue frontend directly
- ✅ **Dual Database Support** - PostgreSQL primary + SQLite auto-fallback, auto-sync on PG recovery
- ✅ **Enterprise Features** - RBAC permissions, audit logs, data masking, request signatures, device binding
- ✅ **Third-Party Login** - GitHub / Google / WeCom / DingTalk / Feishu / QQ OAuth2
- ✅ **Two-Factor Auth** - Google Authenticator TOTP RFC 6238
- ✅ **Secret Management** - HashiCorp Vault integration, auto start/unseal/injection
- ✅ **Observability** - Prometheus metrics, X-Request-ID tracing, JSON structured logs
- ✅ **Dynamic Library Modules** - Code generation module independently compiled, hot-update without restart
- ✅ **Cluster Deployment** - Multi-Worker process support, auto-generates Nginx upstream.conf

---

## Modules

### System Management

| Module | API Route | Description |
|--------|-----------|-------------|
| User Management | `GET/POST/PUT/DELETE /system/user` | CRUD, password reset, CSV import/export, role/post assignment |
| Role Management | `GET/POST/PUT/DELETE /system/role` | CRUD, menu permission assignment, data scope, user authorization |
| Menu Management | `GET/POST/PUT/DELETE /system/menu` | CRUD, dynamic route tree building |
| Department Management | `GET/POST/PUT/DELETE /system/dept` | Tree-structured CRUD |
| Post Management | `GET/POST/PUT/DELETE /system/post` | CRUD |
| Config Management | `GET/POST/PUT/DELETE /system/config` | System parameter CRUD + cache refresh |
| Dictionary Management | `GET/POST/PUT/DELETE /system/dict` | Dict type + dict data CRUD |
| Notice Management | `GET/POST/PUT/DELETE /system/notice` | Announcement CRUD + read status |
| Email Config | `GET/POST /system/emailConfig` | SMTP sender configuration + test send |
| Two-Factor Auth | `POST /system/totp/*` | Google Authenticator TOTP binding/unbinding |
| OAuth2 Login | `GET /oauth2/authorize/{p}` | GitHub / Google / WeCom / DingTalk / Feishu / QQ |
| OAuth2 Callback | `GET /oauth2/callback/{p}` | code → JWT; auto-register on first login |
| OAuth2 Binding | `POST/DELETE /oauth2/bind/{p}` | Bind/unbind third-party to existing account |

### System Monitor

| Module | API Route | Description |
|--------|-----------|-------------|
| Operation Log | `GET/DELETE /monitor/operlog` | Query / delete / clear / export |
| Login Log | `GET/DELETE /monitor/logininfor` | Query / delete / unlock account |
| Online Users | `GET/DELETE /monitor/online` | View sessions / force logout |
| Scheduled Jobs | `GET/POST/PUT/DELETE /monitor/job` | CRUD + run once + pause/resume + execution log |
| System Log | `GET /monitor/logfile` | Real-time `.log`/`.jsonl` log viewer with delete |
| Server Monitor | `GET /monitor/server` | CPU, memory, disk, system info, GPU VRAM |
| Cache Monitor | `GET /monitor/cache` | View cache categories and key-values |
| Data Source Monitor | `GET /monitor/druid` | DB connection pool status and query stats |

### Account Self-Service

| Feature | API Route | Description |
|---------|-----------|-------------|
| Login | `POST /login` | Username + password + captcha, returns JWT |
| LDAP Login | `POST /login` | Auto AD/LDAP auth when `ldap.enabled=true` |
| Register | `POST /register` | Self-registration with email verification |
| Forgot Password | `POST /forgotPassword` | Send reset code via email |
| Reset Password | `POST /resetPassword` | Update password with reset token |
| Send Verification Code | `POST /sendRegCode` | Validate email before registration |

### Code Generation & Tools

| Module | API Route | Description |
|--------|-----------|-------------|
| Code Generation | `GET/POST/PUT/DELETE /tool/gen/**` | Import tables, preview code, generate code, sync database |
| Project Build | `GET/POST /tool/build/**` | Project compilation, build management |
| CodeGen Dynamic Library | `POST /api/codegen/**` | Dynamic compilation, plugin load/unload, code generation |
| Website Info | `GET /tool/website/**` | Website config, SEO management |
| Video Processing | `POST /tool/video/**` | Video transcoding, thumbnail generation |

> **Code Generation Architecture**: Compiled as standalone DLL/SO (`plugins/codegen_plugin.dll`), supports runtime dynamic loading. Main program requires no recompilation for code generation updates.

### AI & Intelligence

| Module | API Route | Description |
|--------|-----------|-------------|
| AI Chat | `POST /ai/chat` | LLM conversation, streaming response |
| Code Generation | `POST /ai/generate` | AI-assisted code generation |
| Speech Recognition | `POST /ai/transcribe` | Speech-to-text (Whisper) |
| ONNX Inference | `GET/POST /ai/onnx/**` | Vector models, text embedding |
| AI Health Check | `GET /ai/health` | Model service status |

### IoT & Device Management

| Module | API Route | Description |
|--------|-----------|-------------|
| Device Management | `GET/POST/DELETE /iot/devices/**` | Device registration, deletion, connectivity test |
| Modbus Read | `POST /iot/modbus/read` | Read registers/coils |
| Modbus Write | `POST /iot/modbus/write` | Write registers/coils |
| Modbus Poll | `POST /iot/modbus/poll` | Batch read multiple addresses |

### Operations & Observability

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/actuator/health` | GET | Health check, JSON format |
| `/actuator/metrics` | GET | Prometheus text format (direct Grafana integration) |
| `/actuator/db` | GET | DB status: backend type, connection state, sync queue |
| `/actuator/info` | GET | Application version info |
| `/actuator/reload` | POST | Hot-reload `config.json` (no restart needed) |
| `/swagger-ui/` | GET | Swagger UI API docs |
| `/v3/api-docs` | GET | OpenAPI 3.0 JSON description |

---

## Tech Stack

| Component | Technology |
|-----------|------------|
| HTTP Framework | [Drogon](https://github.com/drogonframework/drogon) (C++17, async non-blocking) |
| Primary Database | PostgreSQL (libpq direct + connection pool) |
| Fallback Database | SQLite (auto-downgrade, auto-sync back on PG recovery) |
| Cache Layer | In-process MemCache / GPU VramCache / Redis (three-tier, all optional) |
| File Storage | Local disk (default) / MinIO / AWS S3 (AWS SigV4 signing) |
| Authentication | JWT ([jwt-cpp](https://github.com/Thalhammer/jwt-cpp)), PBKDF2-SHA256 password hashing |
| Two-Factor Auth | TOTP RFC 6238 (Google Authenticator, pure OpenSSL implementation) |
| OAuth2 Login | GitHub / Google / WeCom / DingTalk / Feishu / QQ, CSRF-state protection |
| LDAP/AD | OpenLDAP CLI integration (Linux); Windows stub interface |
| Secret Management | HashiCorp Vault (auto start / unseal / secret injection) |
| Email | OpenSSL Implicit-TLS SMTP (QQ/163/enterprise mail, multi-sender rotation) |
| Frontend | RuoYi-Vue (Vue 2 + Element UI), Drogon **built-in hosting**, no Nginx needed |
| Reverse Proxy | Nginx (optional, built-in process management, auto-generates upstream.conf) |
| Logging | JSON structured logs (`.jsonl`, one JSON object per line, ELK-compatible) |
| Observability | Prometheus metrics endpoint, X-Request-ID full-chain tracing |
| Security | Request signature, IP rate limiting, XSS filter, device binding, license management |

### Technology Stack Versions

| Component | Version | Description |
|-----------|---------|-------------|
| **C++ Standard** | C++20 | Latest C++ features, compiler must support C++20 |
| **Drogon** | latest | Async HTTP framework, supports WebSocket, HTTP/2 |
| **PostgreSQL** | 12+ | Primary database, supports JSON, UUID, full-text search |
| **SQLite** | 3.x | Fallback database, auto-fallback and recovery |
| **OpenSSL** | 3.x | Cryptography library, supports TLS 1.3, PBKDF2, HMAC-SHA256 |
| **JsonCpp** | latest | JSON parsing and generation library |
| **jwt-cpp** | latest | JWT token generation and verification (header-only) |
| **RuoYi-Vue** | 3.8 | Frontend framework, Vue 2 + Element UI |
| **Nginx** | 1.20+ | Reverse proxy and load balancing (optional) |
| **MinIO** | latest | Object storage service (optional) |
| **Redis** | 6.0+ | Caching and session storage (optional) |
| **HashiCorp Vault** | 1.12+ | Secret management service (optional) |

---

## System Requirements

### Runtime Environment

| Item | Requirement | Description |
|------|-------------|-------------|
| **Operating System** | Windows 10+ / Linux / macOS | Verified on Windows 11 + MSYS2 MinGW64 |
| **Processor** | x86-64 or ARM64 | Recommended 4+ cores |
| **Memory** | Min 512MB, recommended 2GB+ | Includes database and application |
| **Disk** | Min 500MB | Includes app, logs, uploaded files |
| **Database** | PostgreSQL 12+ or SQLite 3.x | SQLite by default, can switch to PostgreSQL |
| **Network** | TCP port 18080 available | Default listening on 0.0.0.0:18080 |

### Build Environment

| Tool | Version | Description |
|------|---------|-------------|
| **CMake** | 3.15+ | Build system |
| **C++ Compiler** | GCC 11+ / Clang 13+ / MSVC 2019+ | Must support C++20 |
| **Git** | 2.0+ | Version control |
| **MSYS2 MinGW64** | Latest | Windows build environment (Windows users) |
| **Drogon** | latest | Async HTTP framework (pre-compile required) |
| **PostgreSQL** | 12+ | Development libraries (libpq) |
| **OpenSSL** | 3.x | Development libraries |

### Optional Dependencies

| Component | Version | Purpose |
|-----------|---------|---------|
| **Redis** | 6.0+ | Caching, session storage |
| **Nginx** | 1.20+ | Reverse proxy, load balancing |
| **MinIO** | latest | Object storage (alternative to local storage) |
| **HashiCorp Vault** | 1.12+ | Secret management |
| **Prometheus** | latest | Performance monitoring |
| **Grafana** | latest | Visualization dashboard |

---

## Quick Start (5 minutes)

**Fastest way to get started** (no compilation needed):

1. **Download pre-built binary**
   ```bash
   # Download ruoyi-cpp-v1.3.2-windows.zip from Release page
   unzip ruoyi-cpp-v1.3.2-windows.zip
   cd ruoyi-cpp
   ```

2. **Configure database**
   ```bash
   # Edit config.json to change database connection (optional, SQLite is default)
   # For PostgreSQL, modify:
   # "database": { "host": "127.0.0.1", "port": 5432, "dbname": "ruoyi.c", "user": "postgres", "passwd": "your_password" }
   ```

3. **Start service**
   ```bash
   ./ruoyi-cpp.exe
   # Output: [INFO] Server started on http://0.0.0.0:18080
   ```

4. **Access application**
   - Frontend: http://localhost:18080
   - API Docs: http://localhost:18080/swagger-ui/
   - Default credentials: `admin` / `admin123`

> ⚠️ **Production**: Change default password and JWT secret immediately!

---

## Compile from Source

### Prerequisites

**Database**: A running PostgreSQL instance (version 12+)

```sql
-- Create database (tables are created automatically on first run)
CREATE DATABASE "ruoyi.c";
```

**Redis** (optional): Falls back to in-process cache if not configured.

---

### Windows (MSYS2 MinGW64)

**1. Install MSYS2 dependencies**

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

**2. Build and install Drogon**

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

**3. Install jwt-cpp (header-only)**

```bash
git clone https://github.com/Thalhammer/jwt-cpp
cp -r jwt-cpp/include/jwt-cpp /mingw64/include/
```

**4. Build the project**

```bash
git clone https://gitee.com/ruoyicpp/ruoyi ruoyi-cpp
cd ruoyi-cpp && mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/mingw64
ninja
```

**5. Configure and run**

```bash
# Edit config.json (database connection, JWT secret, etc.)
# Tables and initial data are created automatically on first run
./ruoyi-cpp.exe
```

---

## Configuration

Main config file: `config.json` (see `config.bat.template.json` for full reference)

### Core Config

```jsonc
{
  "listeners": [{ "address": "0.0.0.0", "port": 18080, "https": false }],
  "database": { "host": "127.0.0.1", "port": 5432,
    "dbname": "ruoyi.c", "user": "postgres", "passwd": "" },
  "jwt": {
    "secret": "at-least-16-random-chars",  // ⚠️ Required in production
    "expire_minutes": 30, "jwt_expire_days": 7
  }
}
```

### File Storage (local default, optional MinIO/S3)

```jsonc
"storage": {
  "type": "local",           // "local" | "minio" | "s3"
  "local_path": "./upload",
  "endpoint": "http://127.0.0.1:9000",
  "bucket": "ruoyi",
  "access_key": "minioadmin", "secret_key": "minioadmin",
  "region": "us-east-1", "public_url": ""
}
```

### LDAP / Active Directory

```jsonc
"ldap": {
  "enabled": false,
  "host": "192.168.1.100", "port": 389,
  "base_dn": "DC=example,DC=com",
  "bind_dn": "CN=svc_ruoyi,OU=Service Accounts,DC=example,DC=com",
  "bind_pass": "service_password",
  "user_filter": "(&(objectClass=person)(sAMAccountName={username}))",
  "fallback_local": true
}
```

### TOTP Two-Factor Authentication

```jsonc
"totp": { "enabled": true, "issuer": "RuoYi-Cpp" }
```

> **TOTP flow**: call `POST /system/totp/generate` → render `qrUri` as QR code → user scans with Google/Microsoft Authenticator → call `POST /system/totp/enable` with the 6-digit code to activate.

### OAuth2 Third-Party Login

```jsonc
"oauth2": {
  "github": {
    "enabled":       true,
    "client_id":     "YOUR_GITHUB_CLIENT_ID",
    "client_secret": "YOUR_GITHUB_CLIENT_SECRET",
    "redirect_uri":  "http://yourdomain/oauth2/callback/github",
    "scope":         "user:email"
  },
  "google":      { "enabled": false, ... },  // scope: "openid email profile"
  "wechat_work": { "enabled": false, "corp_id": "...", "agent_id": "...", ... },
  "dingtalk":    { "enabled": false, ... },  // client_id = AppKey
  "feishu":      { "enabled": false, ... },  // client_id = App ID
  "qq":          { "enabled": false, ... }   // client_id = App ID
}
```

**OAuth2 login flow:**

1. Frontend calls `GET /oauth2/providers` to list enabled providers
2. Frontend calls `GET /oauth2/authorize/{provider}` to get `{url, state}`
3. Frontend redirects to `url` (provider consent page)
4. After consent, provider redirects to `redirect_uri` (`GET /oauth2/callback/{provider}?code=xxx&state=xxx`)
5. Backend validates `state` (CSRF), exchanges `code` for user info, issues JWT
6. First-time login auto-creates a local account (`{provider}_{openId[:16]}`)
7. Logged-in users can bind existing accounts via `POST /oauth2/bind/{provider}`

> **Security**: `state` is stored in `MemCache` with 60s TTL, preventing CSRF attacks.

### Built-in Frontend Hosting (no Nginx needed)

```jsonc
"frontend": {
  "enabled": true, "dist_path": "./web",
  "spa_mode": true, "api_prefix": "/prod-api",
  "cache_seconds": 3600
}
```

> Place `npm run build:prod` output into `./web/`, then access `:18080` directly.

### Email Configuration (in-app)

After logging in, go to **System Management → Email Senders** to configure SMTP:

| Key | Description | Example |
|-----|-------------|---------|
| `sys.email.host` | SMTP server | `smtp.qq.com` |
| `sys.email.port` | Port (Implicit TLS) | `465` |
| `sys.email.fromName` | Sender display name | `System Notification` |
| `sys.email.senders` | Sender list (JSON array) | `[{"email":"a@qq.com","authCode":"xxxx"}]` |

---

## Project Structure

```
ruoyi-cpp/
├── config.json                      # Main config (see config.bat.template.json)
├── web/                             # Frontend dist (place here, no Nginx needed)
├── logs/                            # Log files (.log text + .jsonl structured)
├── upload/                          # Local file upload directory
├── src/
│   ├── main.cc                      # Entry: middleware registration, service init
│   ├── AppIncludes.h                # Centralized global includes
│   ├── codegen/                     # Code generation module (compiled as dynamic library)
│   │   ├── CMakeLists.txt           # Dynamic library build config
│   │   ├── CodeGenerator.h/cc       # Code generation engine
│   │   ├── DynamicCompiler.h/cc     # Dynamic compiler (CMake + MinGW/GCC)
│   │   ├── PluginManager.h/cc       # Plugin management (load/unload/invoke)
│   │   └── controllers/
│   │       └── CodeGenCtrl.h/cc     # Code generation static methods (exported by DLL)
│   ├── common/
│   │   ├── AjaxResult.h             # Unified JSON response body
│   │   ├── DatabaseInit.cc          # Auto table creation + initial data + migration
│   │   ├── JwtUtils.h               # JWT generation/parsing
│   │   ├── JsonLogger.h             # JSON structured logging (overrides Drogon output)
│   │   ├── RequestTracing.h         # X-Request-ID tracing middleware
│   │   ├── DataMaskUtils.h          # Phone/ID/bank card/email masking
│   │   ├── MetricsCollector.h       # Prometheus metrics + ActuatorCtrl
│   │   ├── TotpUtils.h              # TOTP RFC 6238 (Google Authenticator)
│   │   ├── OAuth2Manager.h          # OAuth2: GitHub/Google/WeCom/DingTalk/Feishu/QQ
│   │   ├── HotConfig.h              # Config hot-reload (5s polling)
│   │   ├── LdapAuth.h               # LDAP/AD authentication
│   │   ├── FrontendHost.h           # Built-in frontend hosting + SPA fallback
│   │   ├── RateLimiter.h            # IP rate limiting
│   │   ├── XssUtils.h               # XSS filter + SQL injection detection
│   │   ├── SignUtils.h              # API request signature verification
│   │   ├── LicenseManager.h         # Software license management
│   │   ├── DeviceBinding.h          # Device binding (hardware fingerprint)
│   │   ├── SmtpUtils.h              # SMTP email (OpenSSL Implicit-TLS)
│   │   └── CrashHandler.h           # Crash capture (SEH/VEH/terminate, Windows)
│   ├── filters/
│   │   ├── JwtAuthFilter.h          # JWT auth middleware (HttpMiddleware)
│   │   └── PermFilter.h             # Permission check macro CHECK_PERM
│   ├── services/
│   │   ├── DatabaseService.h        # PostgreSQL(pool) + SQLite dual-write/auto-fallback
│   │   ├── StorageService.h         # File storage: local / MinIO / S3 (SigV4)
│   │   ├── VaultManager.h           # HashiCorp Vault integration
│   │   └── NginxManager.h           # Nginx subprocess management
│   ├── system/
│   │   ├── services/                # TokenService, SysConfigService, etc.
│   │   └── controllers/
│   │       ├── SysLoginCtrl.h       # Login / register / forgot password / routes
│   │       ├── SysUserCtrl.h        # User management
│   │       ├── SysRoleCtrl.h        # Role management (real-time permission refresh)
│   │       ├── SysTotpCtrl.h        # TOTP two-factor auth API
│   │       ├── OAuth2Ctrl.h         # OAuth2: authorize/callback/bind/unbind
│   │       └── ...                  # Menu / dept / dict / notice, etc.
│   └── monitor/
│       ├── JobScheduler.h           # Cron scheduler (second-level cron expressions)
│       └── controllers/
│           ├── SysLogFileCtrl.h     # System log file viewer
│           ├── SysJobCtrl.h         # Scheduled job management
│           ├── ServerCtrl.h         # Server monitor
│           ├── DruidCtrl.h          # DB connection pool monitor
│           └── ...                  # Operation log / login log / online users
└── ui/                              # Frontend source (Vue 2 + Element UI)
```

---

## Permission Design

- **Super Admin** (`user_id=1`): All permissions, bypasses RBAC
- **Regular Users**: Assigned roles via `sys_user_role`, roles linked to menu permissions
- **Permission strings**: e.g. `system:user:list`, auto-checked by the `CHECK_PERM` macro
- **Real-time permission updates**: Modifying role menus takes effect immediately — **online users do not need to re-login**

### Default Role for Registered Users

Controlled by system parameter `sys.account.initRoleId`:

| Value | Effect |
|-------|--------|
| Empty (default) | No role after registration; admin assigns manually |
| Role ID (e.g. `2`) | Automatically assigned to the specified role |

---

## Default Account

| Username | Password | Description |
|----------|----------|-------------|
| `admin` | `admin123` | Super admin, full permissions |

> ⚠️ **Change the default password immediately in production!**

Passwords are stored using OpenSSL PBKDF2-SHA256 (10,000 rounds).

---

## Bulk User Import

Batch import users via CSV (**System Management → User Management → Import**):

1. Click "Download Template" to get the CSV format
2. Fill in user data (default password: `123456`)
3. Check "Update existing" to overwrite existing accounts
4. Upload the CSV file (UTF-8 with or without BOM)

CSV columns: `Login Name, Display Name, Dept ID, Phone, Email, Gender(0/1/2), Status(0/1)`

---

## Frontend

**Use the official RuoYi-Vue frontend:**

```bash
# Clone the official RuoYi frontend
git clone https://gitee.com/y_project/RuoYi-Vue.git
cd RuoYi-Vue

# Set the backend address in .env.development
VUE_APP_BASE_API = 'http://127.0.0.1:18080'

# Install dependencies and start
npm install
npm run dev
```

For production, deploy the `dist/` folder (built with `npm run build:prod`) to Nginx, pointing the backend address to this project's port (default `18080`).

**Nginx pseudo-static + reverse proxy configuration:**

```nginx
# Vue Router history mode
location / {
    try_files $uri $uri/ /index.html;
}

# Backend API proxy (matches VUE_APP_BASE_API = '/prod-api')
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

# WebSocket notifications
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

## API Quick Reference

### Authentication & Authorization

```bash
# Login to get Token
curl -X POST http://localhost:18080/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin123","code":"1234"}'

# Response example
{
  "code": 200,
  "msg": "Success",
  "data": {
    "access_token": "eyJhbGc...",
    "token_type": "Bearer",
    "expires_in": 1800
  }
}

# Call protected API with Token
curl -X GET http://localhost:18080/system/user/list \
  -H "Authorization: Bearer eyJhbGc..."
```

### Common API Examples

```bash
# Get user list (paginated)
GET /system/user/list?pageNum=1&pageSize=10

# Create user
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

# Get server info
GET /monitor/server

# Get Prometheus metrics
GET /actuator/metrics

# Get application health
GET /actuator/health

# Hot-reload config
POST /actuator/reload

# AI chat
POST /ai/chat
Content-Type: application/json
{
  "message": "Hello, please generate a C++ class for me",
  "model": "gpt-4"
}

# List IoT devices
GET /iot/devices

# Modbus read
POST /iot/modbus/read
Content-Type: application/json
{
  "deviceId": 1,
  "functionCode": 3,
  "startAddress": 0,
  "quantity": 10
}

# Code generation - import tables
POST /tool/gen/importTable
Content-Type: application/json
{
  "tableNames": ["sys_user", "sys_role"]
}

# Code generation - generate code
GET /tool/gen/genCode/sys_user
```

---

## Compatibility with RuoYi-Vue

- ✅ All `/system/**` and `/monitor/**` API routes are identical to the original
- ✅ JWT token format, `getInfo`, `getRouters` response structures are fully compatible
- ✅ Clone the [official RuoYi frontend](https://gitee.com/y_project/RuoYi-Vue), change only the backend URL, and run
- ➕ Added: email sender management, forgot password, registration email verification

---

## Deployment Best Practices

### Production Deployment Checklist

- [ ] **Change default password** - Immediately change `admin` account password from `admin123`
- [ ] **Set strong JWT Secret** - At least 32 random characters, generated via `/dev/urandom` or key management service
- [ ] **Enable HTTPS** - Configure SSL certificates, set `listeners[].https=true`
- [ ] **Configure database** - Use PostgreSQL as primary, SQLite only as fallback
- [ ] **Enable logging** - Set log level to `INFO`, rotate logs regularly
- [ ] **Setup monitoring** - Configure Prometheus + Grafana for CPU/memory/disk monitoring
- [ ] **Enable audit logs** - Record all user operations, export backups regularly
- [ ] **Configure backups** - Daily database backups, store off-site
- [ ] **Restrict access** - Use firewall to limit `/actuator/*` endpoints to internal network only
- [ ] **Enable rate limiting** - Set `rateLimiter.enabled=true` to prevent DDoS

### Docker Deployment

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
# Build image
docker build -t ruoyi-cpp:latest .

# Run container
docker run -d \
  --name ruoyi-cpp \
  -p 18080:18080 \
  -v /data/config.json:/app/config.json \
  -v /data/logs:/app/logs \
  -v /data/upload:/app/upload \
  ruoyi-cpp:latest
```

### Kubernetes Deployment

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

## Performance Optimization

| Optimization | Recommendation | Impact |
|--------------|-----------------|--------|
| **DB Connection Pool** | Set `database.pool_size=20`, adjust per concurrency | +30-50% QPS |
| **Caching Strategy** | Enable Redis, cache hot data (users, roles, menus) | -80% DB load |
| **Async Processing** | Use Drogon async callbacks, avoid blocking ops | +2-3x throughput |
| **Log Level** | Set `WARN` in production, reduce I/O | +10-15% perf |
| **Frontend Resources** | Enable gzip, CDN for static assets | -70% bandwidth |
| **Database Indexes** | Index common query fields (username, email) | 10-100x faster queries |
| **Connection Reuse** | Enable HTTP Keep-Alive, reuse TCP connections | -50% latency |

---

## Troubleshooting Guide

### Startup Failures

```bash
# View detailed logs
tail -f logs/ruoyi-cpp.log

# Common errors:
# 1. "cannot connect to database"
#    → Check PostgreSQL is running: psql -U postgres
#    → Verify connection string: host/port/dbname/user/passwd

# 2. "Address already in use"
#    → Port is occupied, change listeners[].port in config.json

# 3. "Permission denied"
#    → Check file permissions: chmod +x ruoyi-cpp
#    → Check log directory: mkdir -p logs && chmod 755 logs
```

### Performance Issues

```bash
# Monitor CPU/memory
GET /monitor/server

# Check database connection status
GET /actuator/db

# View Prometheus metrics
GET /actuator/metrics

# Check slow queries (PostgreSQL)
SELECT query, mean_time FROM pg_stat_statements 
ORDER BY mean_time DESC LIMIT 10;
```

### Permission Issues

```bash
# Check user permissions
SELECT u.username, r.role_name, m.menu_name 
FROM sys_user u
LEFT JOIN sys_user_role ur ON u.user_id = ur.user_id
LEFT JOIN sys_role r ON ur.role_id = r.role_id
LEFT JOIN sys_role_menu rm ON r.role_id = rm.role_id
LEFT JOIN sys_menu m ON rm.menu_id = m.menu_id
WHERE u.username = 'admin';

# Refresh permission cache
POST /actuator/reload
```

---

## FAQ

**Q: Startup fails with `cannot connect to database`?**
> Check `database.host/port/dbname/user/passwd` in `config.json` and ensure PostgreSQL is running. For SQLite mode, leave `host` empty.

**Q: Login says captcha is wrong?**
> Ensure `captcha.enabled` is `true` and the system clock is correct (captcha expires after 120 seconds).

**Q: Frontend CORS error?**
> In dev mode, check that `devServer.proxy` in `vue.config.js` points to the correct backend port (`18080`). In production, verify the Nginx `/prod-api/` proxy config.

**Q: Can the server start with an empty JWT secret?**
> Yes, but all tokens will be signed with an empty key — **a serious security risk**. Always set a strong random secret in production.

**Q: Role permission changes not taking effect?**
> The backend automatically refreshes online users' permission cache. If it still doesn't work, check that `MemCache` / Redis is functioning correctly.

**Q: Do I need to restart after config changes?**
> No. The program includes a `HotConfig` monitor that checks `config.json` every 5 seconds. Changes are auto-reloaded without restart.
> You can also manually trigger reload via `POST /actuator/reload`.

**Q: How to view API documentation?**
> Visit `http://localhost:18080/swagger-ui/index.html` (loads from CDN, no extra setup needed), or request `GET /v3/api-docs` for OpenAPI 3.0 JSON spec.

**Q: How to enable HTTPS in production?**
> Configure in `config.json`:
> ```json
> "listeners": [{
>   "address": "0.0.0.0",
>   "port": 443,
>   "https": true,
>   "cert_file": "/path/to/cert.crt",
>   "key_file": "/path/to/key.key"
> }]
> ```

**Q: How to monitor application performance?**
> Visit `/actuator/metrics` for Prometheus format metrics, integrate with Grafana for visualization. Or visit `/monitor/server` for real-time server status.

**Q: Does it support cluster deployment?**
> Yes. Configure multiple Worker processes and use Nginx load balancing. The program auto-generates `upstream.conf` with all Worker nodes.

---

---

## Developer Guide

### Adding New API Endpoints

**1. Create Controller**

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
        // Implementation logic
        RESP_OK(cb, Json::Value());
    }

    void add(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "my:add");
        auto body = req->getJsonObject();
        // Implementation logic
        RESP_OK(cb, Json::Value());
    }
};
```

**2. Include in AppIncludes.h**

```cpp
#include "system/controllers/MyCtrl.h"
```

**3. Add Permission Strings to Database**

```sql
INSERT INTO sys_menu (menu_name, parent_id, order_num, path, component, is_frame, is_cache, menu_type, visible, status, perms, icon, create_time)
VALUES ('My Feature', 1, 100, 'my', 'system/my/index', 1, 0, 'C', '0', '0', 'my:list,my:add', 'list', NOW());
```

### Adding New Database Tables

**1. Create Table**

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

**2. Create Model Class**

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

### Adding Scheduled Tasks

```cpp
// Add in src/monitor/JobScheduler.h
void scheduleMyTask() {
    // Execute daily at 02:00
    drogon::app().getLoop()->runAt(
        std::chrono::system_clock::now() + std::chrono::hours(2),
        [this]() {
            LOG_INFO << "Executing scheduled task";
            // Task logic
        }
    );
}
```

---

## Database Architecture

### Core Tables

```
sys_user (User table)
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

sys_role (Role table)
├── role_id (PK)
├── role_name (UK)
├── role_key (UK)
├── role_sort
├── status
└── create_time

sys_menu (Menu table)
├── menu_id (PK)
├── menu_name
├── parent_id (FK)
├── order_num
├── path
├── component
├── perms (Permission string)
├── icon
├── menu_type (C/M/F)
└── visible

sys_user_role (User-Role association)
├── user_id (FK)
└── role_id (FK)

sys_role_menu (Role-Menu association)
├── role_id (FK)
└── menu_id (FK)

sys_oper_log (Operation log)
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

sys_login_log (Login log)
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

### Database Connection Management

```cpp
// Use DatabaseService for queries
auto& db = DatabaseService::instance();

// Execute query
auto res = db.query("SELECT * FROM sys_user WHERE user_id = $1", userId);
if (res.ok() && res.rows() > 0) {
    std::string username = res.str(0, 1);
}

// Execute update
auto updateRes = db.execute(
    "UPDATE sys_user SET status = $1 WHERE user_id = $2",
    status, userId
);
```

---

## Security Best Practices

### Password Security

```cpp
// Password hashing (PBKDF2-SHA256, 10000 rounds)
std::string hashedPwd = SecurityUtils::hashPassword(plainPassword);

// Password verification
bool isValid = SecurityUtils::verifyPassword(plainPassword, hashedPwd);
```

### Request Signature Verification

```cpp
// Enable in config.json
"security": {
  "sign_enabled": true,
  "sign_secret": "your-secret-key",
  "sign_expire_seconds": 300
}

// Client generates signature
std::string signature = SignUtils::generateSignature(params, secret);

// Server verifies signature
bool isValid = SignUtils::verifySignature(params, signature, secret);
```

### Data Masking

```cpp
// Auto-mask sensitive fields
Json::Value response;
response["user"] = user;
DataMaskUtils::maskJsonValue(response);  // Auto-masks phone/email/idcard etc.
```

### XSS Protection

```cpp
// Filter user input
std::string cleanInput = XssUtils::filterXss(userInput);

// SQL injection detection
if (XssUtils::detectSqlInjection(userInput)) {
    return RESP_ERR(cb, "Illegal input");
}
```

---

## Performance Benchmarks

### Test Environment

- **CPU**: Intel Core i7-9700K (8 cores)
- **Memory**: 16GB DDR4
- **Database**: PostgreSQL 12
- **Concurrency**: 100-1000

### Test Results

| Scenario | QPS | Avg Latency | P99 Latency | Memory |
|----------|-----|-------------|------------|--------|
| User list query | 8500 | 11ms | 45ms | 45MB |
| User creation | 3200 | 30ms | 120ms | 48MB |
| Permission check | 15000 | 6ms | 20ms | 42MB |
| Login | 1800 | 55ms | 200ms | 52MB |
| File upload (10MB) | 120 | 8.3s | 9.5s | 150MB |

### Load Testing Commands

```bash
# Using Apache Bench
ab -n 10000 -c 100 http://localhost:18080/system/user/list

# Using wrk
wrk -t4 -c100 -d30s http://localhost:18080/system/user/list

# Using hey
hey -n 10000 -c 100 http://localhost:18080/system/user/list
```

---

## Version Upgrade Guide

### Upgrading from v1.2.x to v1.3.2

**1. Backup Database**

```bash
pg_dump -U postgres ruoyi.c > backup_v1.2.x.sql
```

**2. Stop Old Version**

```bash
pkill -f ruoyi-cpp
```

**3. Update Executable**

```bash
# Download new version
wget https://gitee.com/ruoyicpp/ruoyi/releases/download/v1.3.2/ruoyi-cpp-v1.3.2-windows.zip
unzip ruoyi-cpp-v1.3.2-windows.zip
```

**4. Update Configuration**

```bash
# Compare old and new config.json, merge new config items
diff config.json.old config.json.new
```

**5. Database Migration**

```sql
-- New tables and fields (auto-executed)
-- Program will auto-detect and execute migration scripts on startup
```

**6. Start New Version**

```bash
./ruoyi-cpp
```

**7. Verify Upgrade**

```bash
# Check version
curl http://localhost:18080/version

# Check health status
curl http://localhost:18080/actuator/health
```

### Rollback Steps

```bash
# 1. Stop current version
pkill -f ruoyi-cpp

# 2. Restore backup
psql -U postgres ruoyi.c < backup_v1.2.x.sql

# 3. Restore old version executable
cp ruoyi-cpp.v1.2.x ./ruoyi-cpp

# 4. Start old version
./ruoyi-cpp
```

---

## Contributing

Issues and Pull Requests are welcome!

1. Fork the repository: <https://gitee.com/ruoyicpp/ruoyi>
2. Create a branch: `git checkout -b feat/your-feature`
3. Commit your changes: `git commit -m "feat: describe your change"`
4. Push the branch: `git push origin feat/your-feature`
5. Open a Pull Request

**Code style:**
- C++ code follows the existing project style (header-only implementation, Drogon async callbacks)
- New API endpoints must include a permission string (e.g. `system:user:add`)
- No hardcoded secrets — use `config.json` or database configuration
- New features must include unit tests
- Run `clang-format` before committing

---

## Security Notes

| Item | Note |
|------|------|
| JWT Secret | Must be ≥16 random chars; use `/dev/urandom` or similar in production |
| Default password | Change `admin`'s `admin123` password immediately after first run |
| TOTP | Strongly recommend enforcing TOTP for admin accounts |
| LDAP `bind_pass` | Inject via Vault; do not store in plaintext in `config.json` |
| MinIO `secret_key` | Same — inject via Vault |
| OAuth2 `client_secret` | Same — inject via Vault; never store in plaintext |
| OAuth2 `redirect_uri` | Must exactly match provider console config to prevent open redirect |
| `/actuator/*` | Restrict to internal network via Nginx/firewall |
| Data masking | `DataMaskUtils::maskJsonValue()` auto-masks sensitive fields in logs/responses |

---

## Changelog

### v1.3.2 (current)

- **Comprehensive Documentation** - Added quick start, API reference, deployment best practices, performance optimization, troubleshooting, developer guide, and more
- **Technology Stack Updates** - C++ standard upgraded to C++20, updated all dependency library versions
- **System Requirements Clarified** - Detailed runtime environment, build environment, and optional dependency version requirements
- **Complete API Documentation** - Added API docs for tools, AI, IoT modules, total 50+ endpoints
- **Detailed Deployment Guide** - Added Docker, Kubernetes deployment examples, production deployment checklist
- **Performance Benchmarks** - Provided performance data for 5 scenarios (QPS, latency, memory usage)
- **Version Upgrade Guide** - Complete upgrade steps and rollback procedures
- **Security Best Practices** - Password security, request signatures, data masking, XSS protection guidelines
- **Developer Guide** - Complete examples for adding APIs, database tables, scheduled tasks
- **Database Architecture** - Detailed table structure design and connection management code examples

### v1.3.0

- **Code generation module as dynamic library**: `src/codegen/` compiled as standalone DLL/SO (`plugins/codegen_plugin.dll`); main program requires no recompilation for code generation updates; supports runtime load/unload; `CodeGenCtrl` refactored to pure C++ static methods accepting/returning JSON strings
- **Dynamic compiler integration**: `DynamicCompiler` supports Windows MinGW + Linux GCC, auto-invokes CMake to compile generated code; custom compiler path via `CODEGEN_COMPILER_PATH` environment variable
- **Plugin management system**: `PluginManager` supports load/unload/list/invoke multiple plugins; each plugin is independent DLL; supports hot-update
- **Domain / HTTPS access**: new `_listeners_https_example` in `config.json` covering local / public HTTP / public HTTPS listener modes; manual certificates (`.crt` + `.key`) from cloud providers (Tencent Cloud, etc.) mounted directly in listeners, zero extra dependencies
- **InnerLink menu URL auto-replacement** (`menu.api_base_url`): on startup, automatically replaces all `localhost` URLs in InnerLink menus with the configured public domain — no manual menu editing needed
- **ACME certificate notes**: full comments in `acme` config block clarifying that port 80 must use `https=false` (HTTP-01 challenge), while 443/custom ports enable HTTPS
- **Deployment guide** (`build-nginx/部署说明.md`): covers local / public HTTP / HTTPS modes, SSL certificate format selection, frontend build & deploy, common ports
- **Fix disk volume label garbled text + heap crash** (`ServerCtrl.h`): `GetVolumeInformationA` → `GetVolumeInformationW` + `WideCharToMultiByte(CP_UTF8)`, fixing GBK mojibake on Chinese Windows and eliminating jsoncpp heap corruption on non-UTF8 bytes
- **WebSocket auto-reconnect** (`App.vue`): exponential backoff (2s → 4s → ... → 30s max), auto-resubscribe after disconnect
- **Swagger API docs expansion**: added OpenAPI tags and paths for IoT devices, AI/ONNX inference, SM2/SM3/SM4 crypto, OAuth2, code generation, dashboard modules
- **IoT device startup loading** (`main.cc`): calls `IotCtrl::loadFromDb()` on startup to restore device list from database
- **New unit tests**: `test_token_cache` (set/get/remove/update/size), `test_rate_limiter` (normal/block/whitelist/disable), integrated into `RUOYI_BUILD_HEAVY_TESTS`
- **DashboardCtrl fix**: `char today[16]` → `char today[32]`, eliminating Linux glibc fortify buffer overflow warning

### v1.2.1
- **Restart service management page**: `GET /monitor/restart` — pure backend-rendered HTML page; admin can check online user count before confirming restart; token auto-read from same-origin iframe `sessionStorage`
- **Fix HTTP_HIDE production 404 bug**: removed `HTTP_HIDE` macro from `SysRestartCtrl` that caused restart endpoints to return 404 in Release builds
- **Notification center** (f15): DingTalk / Feishu / WeCom Webhook (HMAC-SHA256 signed) + in-app messages
- **API Key management** (f16): `/system/apikey/**` CRUD, 48-char random key, `X-API-Key` header or `?apiKey=` query param auth
- **Audit enhancement** (f17): `sys_oper_log` gains `before_data`/`after_data` fields; `diffJson()` records only changed fields
- **SQLite dual-layer encryption**: page-level (sqlite3mc) + file-level (RYENC1 AES-256-GCM) fallback; see `docs/SQLITE_ENCRYPTION.md`
- **SQLite cipher CLI tool**: encrypt / decrypt / rekey / check / selftest subcommands
- **Unread notice badge API**: `sys_notice_read` table + `GET /system/notice/unreadCount`
- **Graceful shutdown**: `POST /actuator/shutdown` (loopback only)
- **Unit tests + CI**: `test_sqlite_file_cipher` 10 cases / 57 assertions; GitHub Actions 3-platform CI

### v1.2.0
- **OAuth2 third-party login**: GitHub / Google / WeCom / DingTalk / Feishu / QQ; CSRF-state protection; auto-register on first login; bind/unbind for existing accounts (`sys_user_oauth` table)
- **TOTP two-factor auth**: Google/Microsoft Authenticator, RFC 6238, pure OpenSSL
- **LDAP/AD authentication**: enterprise SSO with `fallback_local` support
- **Tiered file storage**: local default, switch to MinIO/S3 via `config.json` (AWS SigV4)
- **Config hot-reload**: `HotConfig` 5s polling, or `POST /actuator/reload`
- **Prometheus metrics**: `/actuator/metrics` (QPS/latency/DB), Grafana-compatible
- **Data masking utilities**: phone, ID card, bank card, email, name
- **X-Request-ID tracing**: auto-generated UUID v4 on every request
- **System log viewer**: frontend iframe embed, supports `.log`/`.jsonl`, live refresh
- **DB pool monitor**: `/actuator/db` shows PG/SQLite status, pending sync queue
- **Zero-downtime TOTP migration**: `ALTER TABLE IF NOT EXISTS`

### v1.1.0
- **Built-in frontend hosting**: `/prod-api` prefix auto-stripped, SPA history fallback, no Nginx needed
- **System log file viewer backend**: `GET /monitor/logfile/page`, iframe-embedded in frontend
- **JSON structured logging**: `JsonLogger` converts trantor output to NDJSON (`.jsonl`)
- **SQLite compatibility fix**: `DEFAULT NOW()` → `DEFAULT CURRENT_TIMESTAMP`

### v1.0.0
- Full implementation of all RuoYi-Vue system management and monitoring APIs
- PostgreSQL + SQLite auto-fallback dual-write, auto-sync back on PG recovery
- PBKDF2-SHA256 password hashing, JWT auto-renewal, token blacklist
- HashiCorp Vault secret management (auto start/unseal/injection)
- Email sender management (OpenSSL Implicit-TLS SMTP, multi-sender rotation)
- Forgot password / registration email verification
- WebSocket real-time notifications (one-time ticket authentication)
- IP rate limiting, bot UA blocking, XSS/SQL injection filtering, CORS
- Request signature verification, device binding, license management
- GPU VRAM cache (optional CUDA), Cron scheduler (second-level, DB-persisted)
- Cluster mode (primary/replica, auto-generates Nginx upstream.conf)
- Crash capture (SEH/VEH + Minidump, Windows)
- Role permission changes take effect in real time — no re-login required

---

## Community

QQ Group: **7827982393**

Official Website: <http://www.gzbgw.com>

Question Bank Platform: <http://question.gzbgw.com>

---

## License

This project is open-sourced under the [MIT License](LICENSE).

RuoYi-Vue is copyright of the [RuoYi Team](https://gitee.com/y_project/RuoYi-Vue), licensed under MIT.
