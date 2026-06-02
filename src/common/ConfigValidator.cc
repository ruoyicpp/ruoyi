#include "ConfigValidator.h"
#include <fstream>
#include <thread>
#include <mutex>
#include <chrono>
#include <json/json.h>

namespace config {

// ─────────────────────────────────────────────────────────────────────────────
// ConfigValidator 实现
// ─────────────────────────────────────────────────────────────────────────────
void ConfigValidator::registerConfig(const ConfigValue& cfg) {
    schema_.push_back(cfg);
}

void ConfigValidator::registerDefaultSchema() {
    // 数据库配置
    registerConfig({"database.host", "127.0.0.1", "RUOYI_DB_HOST", "数据库主机"});
    registerConfig({"database.port", 5432, "RUOYI_DB_PORT", "数据库端口"});
    registerConfig({"database.dbname", "ruoyi", "RUOYI_DB_NAME", "数据库名"});
    registerConfig({"database.user", "postgres", "RUOYI_DB_USER", "数据库用户名"});
    registerConfig({"database.passwd", "", "RUOYI_DB_PASSWORD", "数据库密码", false, true});
    registerConfig({"database.maxConnections", 20, "RUOYI_DB_MAX_CONN", "最大连接数", false, false, "", 1, 1000});

    // Redis配置
    registerConfig({"redis.enabled", false, "RUOYI_REDIS_ENABLED", "启用Redis"});
    registerConfig({"redis.host", "127.0.0.1", "RUOYI_REDIS_HOST", "Redis主机"});
    registerConfig({"redis.port", 6379, "RUOYI_REDIS_PORT", "Redis端口"});
    registerConfig({"redis.password", "", "RUOYI_REDIS_PASSWORD", "Redis密码", false, true});

    // JWT配置
    registerConfig({"jwt.secret", "ruoyi-cpp-secret", "RUOYI_JWT_SECRET", "JWT密钥", true, true, "", 16});
    registerConfig({"jwt.expire_minutes", 30, "RUOYI_JWT_EXPIRE", "Token过期时间(分钟)"});

    // 服务器配置
    registerConfig({"server.port", 8080, "RUOYI_SERVER_PORT", "服务端口", false, false, "", 1, 65535});
    registerConfig({"server.threads", 4, "RUOYI_SERVER_THREADS", "工作线程数", false, false, "", 1, 64});

    // 安全配置
    registerConfig({"security.admin_whitelist", "", "RUOYI_ADMIN_WHITELIST", "管理员IP白名单"});
    registerConfig({"security.rate_limit", true, "RUOYI_RATE_LIMIT", "启用限流"});
    registerConfig({"security.cors_origins", "", "RUOYI_CORS_ORIGINS", "CORS允许的源"});
}

ValidationResult ConfigValidator::validate(const Json::Value& config, const std::string& env) const {
    ValidationResult result;

    for (const auto& cfg : schema_) {
        std::string value;

        // 1. 检查环境变量（优先级最高）
        if (!cfg.envVar.empty()) {
            const char* envVal = std::getenv(cfg.envVar.c_str());
            if (envVal && envVal[0] != '\0') {
                value = envVal;
            }
        }

        // 2. 从配置获取
        if (value.empty()) {
            std::istringstream ss(cfg.key);
            std::string segment;
            Json::Value current = config;
            bool found = true;

            while (std::getline(ss, segment, '.')) {
                if (!current.isObject() || !current.isMember(segment)) {
                    found = false;
                    break;
                }
                current = current[segment];
            }

            if (found && current.isString()) {
                value = current.asString();
            } else if (found && current.isInt()) {
                value = std::to_string(current.asInt());
            } else if (found && current.isBool()) {
                value = current.asBool() ? "true" : "false";
            }
        }

        // 3. 验证
        if (cfg.required && value.empty()) {
            result.addError("必需配置项未设置: " + cfg.key +
                " (可通过环境变量 " + cfg.envVar + " 设置)");
            continue;
        }

        if (value.empty()) continue;

        // 类型验证
        if (std::holds_alternative<int>(cfg.defaultValue)) {
            try {
                int v = std::stoi(value);
                if (v < cfg.minInt || v > cfg.maxInt) {
                    result.addError("配置项 " + cfg.key + " 的值 " + value +
                        " 超出范围 [" + std::to_string(cfg.minInt) + ", " +
                        std::to_string(cfg.maxInt) + "]");
                }
            } catch (...) {
                result.addError("配置项 " + cfg.key + " 需要整数类型");
            }
        }

        // 敏感信息警告
        if (cfg.sensitive && !value.empty()) {
            if (value == "password" || value == "123456" || value == "admin") {
                result.addWarning("配置项 " + cfg.key + " 使用了不安全的默认值");
            }
        }

        // 密码复杂度检查
        if (cfg.key.find("password") != std::string::npos && !value.empty()) {
            if (value.size() < 8) {
                result.addWarning("配置项 " + cfg.key + " 建议使用至少8位的密码");
            }
        }
    }

    // 生产环境额外检查
    if (env == "production") {
        // 检查JWT密钥
        auto jwtSecret = get<std::string>(config, "jwt.secret", "");
        if (jwtSecret == "ruoyi-cpp-secret" || jwtSecret.size() < 32) {
            result.addError("生产环境必须设置强JWT密钥（至少32字符）");
        }

        // 检查数据库密码
        auto dbPass = get<std::string>(config, "database.passwd", "");
        if (dbPass.empty()) {
            result.addError("生产环境必须设置数据库密码");
        }

        // 检查是否启用限流
        bool rateLimit = get<bool>(config, "security.rate_limit", true);
        if (!rateLimit) {
            result.addWarning("生产环境建议启用请求限流");
        }
    }

    return result;
}

bool ConfigValidator::checkRequired(const Json::Value& config) const {
    for (const auto& cfg : schema_) {
        if (!cfg.required) continue;

        std::istringstream ss(cfg.key);
        std::string segment;
        Json::Value current = config;
        bool found = true;

        while (std::getline(ss, segment, '.')) {
            if (!current.isObject() || !current.isMember(segment)) {
                found = false;
                break;
            }
            current = current[segment];
        }

        if (!found || current.empty()) {
            return false;
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// EnvConfig 实现
// ─────────────────────────────────────────────────────────────────────────────
EnvConfig::Env EnvConfig::detectEnv() const {
    // 1. 检查 RUOYI_ENV 环境变量
    const char* env = std::getenv("RUOYI_ENV");
    if (env) {
        std::string e(env);
        if (e == "dev" || e == "development") return Env::Development;
        if (e == "staging") return Env::Staging;
        if (e == "prod" || e == "production") return Env::Production;
        if (e == "test") return Env::Test;
    }

    // 2. 检查 NODE_ENV（前端常用）
    env = std::getenv("NODE_ENV");
    if (env) {
        std::string e(env);
        if (e == "development") return Env::Development;
        if (e == "production") return Env::Production;
    }

    // 3. 检查环境特征
    if (std::getenv("DEBUG")) return Env::Development;
    if (std::getenv("CI")) return Env::Test;

    return Env::Production;
}

std::string EnvConfig::currentEnvName() const {
    switch (currentEnv_) {
        case Env::Development: return "development";
        case Env::Staging: return "staging";
        case Env::Production: return "production";
        case Env::Test: return "test";
        default: return "unknown";
    }
}

Json::Value EnvConfig::loadJsonFile(const std::filesystem::path& path) const {
    Json::Value root;
    std::ifstream f(path);
    if (!f.is_open()) return root;

    Json::CharReaderBuilder rb;
    std::string errs;
    if (!Json::parseFromStream(rb, f, &root, &errs)) {
        throw std::runtime_error("解析配置文件失败: " + path.string() + " - " + errs);
    }
    return root;
}

Json::Value EnvConfig::mergeConfig(const std::vector<Json::Value>& configs) const {
    Json::Value result;

    for (const auto& cfg : configs) {
        // 递归合并
        std::function<void(Json::Value&, const Json::Value&)> merge;
        merge = [&](Json::Value& target, const Json::Value& source) {
            if (source.isObject()) {
                for (const auto& key : source.getMemberNames()) {
                    if (!target.isMember(key)) {
                        target[key] = source[key];
                    } else if (target[key].isObject() && source[key].isObject()) {
                        merge(target[key], source[key]);
                    } else {
                        target[key] = source[key];
                    }
                }
            }
        };
        merge(result, cfg);
    }

    return result;
}

std::string EnvConfig::getEnvVar(const std::string& key) const {
    const char* val = std::getenv(key.c_str());
    return val ? val : "";
}

void EnvConfig::init(const std::string& configDir) {
    currentEnv_ = detectEnv();
    std::filesystem::path dir(configDir);

    std::vector<Json::Value> configs;

    // 1. 加载默认配置
    auto defaultPath = dir / "config.default.json";
    if (std::filesystem::exists(defaultPath)) {
        configs.push_back(loadJsonFile(defaultPath));
    }

    // 2. 加载环境特定配置
    auto envName = currentEnvName();
    auto envPath = dir / ("config." + envName + ".json");
    if (std::filesystem::exists(envPath)) {
        configs.push_back(loadJsonFile(envPath));
    }

    // 3. 加载主配置文件
    auto mainPath = dir / "config.json";
    if (std::filesystem::exists(mainPath)) {
        configs.push_back(loadJsonFile(mainPath));
    }

    // 合并
    mergedConfig_ = mergeConfig(configs);

    // 注册默认schema
    ConfigValidator::instance().registerDefaultSchema();
}

Json::Value EnvConfig::loadConfig(const std::string& path) {
    return loadJsonFile(std::filesystem::path(path));
}

std::filesystem::path EnvConfig::envConfigDir() const {
    return std::filesystem::current_path() / "config" / currentEnvName();
}

void EnvConfig::watchConfig(const std::string& path, ConfigChangeCallback callback) {
    watchCallbacks_.push_back(callback);

    if (!watchRunning_) {
        watchRunning_ = true;
        watchThread_ = std::thread([this]() {
            std::map<std::string, std::filesystem::file_time_type> lastWrite;

            while (watchRunning_) {
                std::this_thread::sleep_for(std::chrono::seconds(5));

                for (const auto& [key, filePath] : configFiles_) {
                    try {
                        auto currentTime = std::filesystem::last_write_time(filePath);
                        if (lastWrite[key] != currentTime) {
                            lastWrite[key] = currentTime;
                            auto newConfig = loadJsonFile(filePath);
                            // 通知所有回调
                            for (auto& cb : watchCallbacks_) {
                                cb(key, mergedConfig_, newConfig);
                            }
                            mergedConfig_ = newConfig;
                        }
                    } catch (...) {}
                }
            }
        });
    }

    configFiles_[path] = std::filesystem::path(path);
}

void EnvConfig::stopWatching() {
    watchRunning_ = false;
    if (watchThread_.joinable()) {
        watchThread_.join();
    }
}

ValidationResult EnvConfig::validateConfig(const Json::Value& config) const {
    return ConfigValidator::instance().validate(config, currentEnvName());
}

// ─────────────────────────────────────────────────────────────────────────────
// ConfigChangeNotifier 实现
// ─────────────────────────────────────────────────────────────────────────────
size_t ConfigChangeNotifier::onChange(Handler handler) {
    std::lock_guard<std::mutex> lk(mutex_);
    size_t id = nextId_++;
    handlers_[id] = handler;
    return id;
}

void ConfigChangeNotifier::unsubscribe(size_t id) {
    std::lock_guard<std::mutex> lk(mutex_);
    handlers_.erase(id);
}

void ConfigChangeNotifier::notify(const std::string& path, const Json::Value& oldVal, const Json::Value& newVal) {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto& [id, handler] : handlers_) {
        try {
            handler(path, oldVal, newVal);
        } catch (...) {}
    }
}

} // namespace config
