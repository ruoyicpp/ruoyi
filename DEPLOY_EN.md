# RuoYi-Cpp Deployment Guide

English | [中文](DEPLOY.md)

This guide explains how to deploy RuoYi-Cpp using pre-compiled binaries — no compilation required.

---

## 1. Download Release Package

Download the package for your platform from the [Releases](../../releases) page:

| File | Platform |
|------|----------|
| `ruoyi-cpp-windows-x64.zip` | Windows 10/11 x64 |
| `ruoyi-cpp-linux.tar.gz` | Linux x64, glibc ≥ 2.39 (Ubuntu 24.04 / Debian 13 / RHEL 9.4+) |

---

## 2. Linux Deployment

### Extract

```bash
tar -xzf ruoyi-cpp-linux.tar.gz
cd ruoyi-cpp
```

Package structure:

```
ruoyi-cpp/
├── ruoyi-cpp          # executable
├── config.json        # configuration file
├── web/               # frontend static files
└── upload/            # file upload directory (auto-created)
```

### Configure

Edit `config.json` and update at minimum:

```json
{
    "app": {
        "base_url": "https://your-domain.com"
    },
    "database": {
        "host": "127.0.0.1",
        "port": 5432,
        "dbname": "ruoyi",
        "user": "postgres",
        "passwd": "your-password"
    }
}
```

> **Note**: PostgreSQL is optional. Without it, the system automatically uses the embedded SQLite database.

### Start

```bash
chmod +x ruoyi-cpp
nohup ./ruoyi-cpp > ruoyi.log 2>&1 &
```

Default listens on `0.0.0.0:18080`. Open `http://your-server-ip:18080` in your browser.

### Nginx Reverse Proxy (Recommended)

**Baota Panel**: Website → Reverse Proxy → Add, set target URL to `http://127.0.0.1:18080`.

**Manual config**:

```nginx
location / {
    proxy_pass http://127.0.0.1:18080;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
}
```

### Stop / Restart

```bash
# Stop
pkill ruoyi-cpp

# Restart
pkill ruoyi-cpp && sleep 1 && nohup ./ruoyi-cpp > ruoyi.log 2>&1 &

# View logs
tail -f ruoyi.log
```

---

## 3. Windows Deployment

### Extract

Unzip `ruoyi-cpp-windows-x64.zip` to any directory.

### Configure

Edit `config.json` with your database and other settings.

### Start

Double-click `ruoyi-cpp.exe`, or run from command line:

```cmd
ruoyi-cpp.exe
```

Open `http://localhost:18080` in your browser.

---

## 4. Default Credentials

| Username | Password |
|----------|----------|
| `admin` | `admin123` |

> **Change your password immediately after first login.**

---

## 5. Database Modes

| Scenario | Database | Notes |
|----------|----------|-------|
| No PostgreSQL installed | SQLite (embedded) | Zero dependencies, data stored in `ruoyi.db` |
| PostgreSQL installed | PostgreSQL | Recommended for production |
| Dual-write mode | PostgreSQL + SQLite | Auto-failover to SQLite, syncs back when PG recovers |

---

## 6. FAQ

**Q: 502 Bad Gateway after startup?**  
A: Check that Nginx reverse proxy points to port `18080`, or access `http://IP:18080` directly to verify the backend is running.

**Q: Database connection failed?**  
A: Check database settings in `config.json`, or set `sqlite.enabled` to `true` to run with SQLite first.

**Q: How to view backend logs?**  
```bash
tail -f ruoyi.log
# or
tail -f logs/ruoyi.*.log
```

**Q: Menu pages won't open (localhost refused)?**  
A: Set `app.base_url` in `config.json` to your actual domain and restart — it will auto-fix all InnerLink menu paths.
