/**
 * @file DefaultConfig.h
 * @brief 默认配置生成器 — 首次启动时自动创建 config.json
 *
 * 功能概述：
 *   - 配置文件不存在时，从内嵌模板生成一份最小可用配置
 *   - 默认走 SQLite（零外部依赖，开箱即用）
 *   - JWT secret 留空，由 JwtUtils secret_file 机制自动生成随机密钥落盘
 *   - 远程许可证 url 留空，由 LicenseManager::deriveSelfUrl 自动推导本机
 *
 * 使用示例：
 *   DefaultConfig::ensure(configFile);   // 不存在则生成，返回是否成功
 *
 * 集成点：
 *   - src/main.cc：--config 解析之后、许可证校验之前调用
 */

#pragma once

#include <string>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace DefaultConfig {

/// 内嵌默认配置模板（最小可用集：SQLite + 本地回环监听）
/// 注意：保持纯 JSON，键名与 config.json 正式版一致
inline const char* kTemplate = R"JSON({
    "listeners": [
        {
            "address": "127.0.0.1",
            "port": 18080,
            "https": false
        }
    ],
    "app": {
        "threads_num": 4,
        "enable_session": false,
        "session_timeout": 0,
        "document_root": "./",
        "upload_path": "./upload",
        "enable_server_header": false,
        "enable_date_header": true,
        "client_max_body_size": "20M",
        "client_max_memory_body_size": "1M",
        "keepalive_requests_number": 0,
        "pipelining_requests_number": 0,
        "log": {
            "log_path": "",
            "logfile_base_name": "ruoyi",
            "log_size_limit": 104857600,
            "log_keep_files": 5,
            "log_level": "INFO"
        }
    },
    "storage": {
        "type": "local",
        "local_path": "./upload"
    },
    "database": {
        "host": "127.0.0.1",
        "port": 5432,
        "dbname": "ruoyi",
        "user": "postgres",
        "passwd": "",
        "sqlite_path": ""
    },
    "sqlite": {
        "enabled": true,
        "path": ""
    },
    "log": {
        "enabled": true,
        "path": "./logs",
        "max_files": 20,
        "max_results": 500
    },
    "ruoyi": {
        "name": "RuoYi-Cpp",
        "version": "1.0.0",
        "demo_enabled": false,
        "profile": "./upload",
        "address_enabled": false
    },
    "jwt": {
        "secret_file": "./data/.jwt_secret",
        "secret": "",
        "issuer": "ruoyi.cpp.issuer",
        "audience": "ruoyi.cpp.audience",
        "expire_minutes": 30,
        "jwt_expire_days": 7
    },
    "user": {
        "max_retry_count": 5,
        "lock_time_minutes": 15,
        "password_min_length": 5,
        "password_max_length": 20,
        "username_min_length": 2,
        "username_max_length": 20
    },
    "captcha": {
        "enabled": true,
        "expire_seconds": 120
    },
    "redis": {
        "enabled": false,
        "host": "127.0.0.1",
        "port": 6379,
        "password": "",
        "db": 0,
        "key_prefix": ""
    },
    "cache": {
        "enabled": true,
        "local_ttl_seconds": 60,
        "redis_ttl_seconds": 3600
    },
    "frontend": {
        "enabled": true,
        "dist_path": "./web",
        "spa_mode": true,
        "api_prefix": "/prod-api",
        "cache_seconds": 3600
    },
    "menu": {
        "api_base_url": "http://127.0.0.1:18080"
    },
    "cors": {
        "allow_origins": ["*"],
        "allow_methods": ["GET","POST","PUT","DELETE","OPTIONS"],
        "allow_headers": ["Authorization","Content-Type","X-App-Id","X-Challenge-Token","X-Requested-With"],
        "expose_headers": ["access-token","x-access-token"],
        "max_age": 3600
    }
}
)JSON";

/**
 * @brief 确保配置文件存在；不存在则按模板生成
 * @param path 配置文件路径（如 "config.json" 或 --config 指定路径）
 * @return true=文件已存在或生成成功；false=生成失败
 * @note 已存在的文件绝不覆盖
 */
inline bool ensure(const std::string& path) {
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) return true;

    // 路径含目录前缀时先建目录（如 --config conf/prod.json）
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) {
            std::cerr << "[Config] 无法创建目录 " << p.parent_path()
                      << ": " << ec.message() << std::endl;
            return false;
        }
    }

    std::ofstream f(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f.is_open()) {
        std::cerr << "[Config] 无法创建 " << path << std::endl;
        return false;
    }
    f << kTemplate;
    f.close();
    std::cout << "[Config] 首次启动：已生成默认配置 " << path
              << "（SQLite 模式，零外部依赖；JWT 密钥将自动生成）" << std::endl;
    return true;
}

} // namespace DefaultConfig
