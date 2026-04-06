<div align="center">

English | [中文](README.md)

# RuoYi-Cpp

**A High-Performance C++ Version of the RuoYi Management Framework**

Based on [Drogon](https://github.com/drogonframework/drogon) + PostgreSQL, 100% compatible with the RuoYi-Vue frontend

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
[![OAuth2](https://img.shields.io/badge/OAuth2-GitHub%20%7C%20Google%20%7C%20DingTalk%20%7C%20Feishu-4A90E2.svg)]()

</div>

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

---

## Quick Start

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

## Compatibility with RuoYi-Vue

- ✅ All `/system/**` and `/monitor/**` API routes are identical to the original
- ✅ JWT token format, `getInfo`, `getRouters` response structures are fully compatible
- ✅ Clone the [official RuoYi frontend](https://gitee.com/y_project/RuoYi-Vue), change only the backend URL, and run
- ➕ Added: email sender management, forgot password, registration email verification

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

### v1.2.0 (current)
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
