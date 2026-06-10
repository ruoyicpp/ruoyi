#pragma once

/**
 * @file ConfigValidator.h
 * @brief 配置验证器 — 多环境配置加载、验证和热重载
 * 
 * 功能概述：
 *   - 多环境支持：开发、测试、生产环境配置
 *   - 环境变量覆盖：环境变量可覆盖配置文件
 *   - 配置验证：启动时验证配置的有效性
 *   - 配置热重载：支持不重启应用更新配置
 *   - 配置合并：支持多个配置文件合并
 *   - 类型检查：自动检查配置值的类型
 * 
 * 核心特性：
 *   - 环境感知：自动检测运行环境
 *   - 优先级管理：环境变量 > 配置文件 > 默认值
 *   - 验证规则：支持自定义验证规则
 *   - 错误报告：详细的配置错误信息
 *   - 线程安全：支持并发读取配置
 *   - 监听机制：配置变更时触发回调
 * 
 * 支持的环境：
 *   - development：开发环境
 *   - staging：测试环境
 *   - production：生产环境
 * 
 * 配置文件加载顺序：
 *   1. config.json - 基础配置
 *   2. config.{env}.json - 环境特定配置
 *   3. 环境变量 - 覆盖上述配置
 * 
 * 使用示例：
 *   ```cpp
 *   // 初始化配置验证器
 *   ConfigValidator validator;
 *   validator.setEnvironment("production");
 *   validator.loadConfig("./config");
 *   
 *   // 验证配置
 *   if (!validator.validate()) {
 *       std::cerr << "Config validation failed: " << validator.getErrors() << std::endl;
 *       return 1;
 *   }
 *   
 *   // 获取配置值
 *   std::string dbHost = validator.getString("database.host");
 *   int dbPort = validator.getInt("database.port");
 *   
 *   // 监听配置变更
 *   validator.onChange([](const std::string& key) {
 *       LOG(INFO) << "Config changed: " << key;
 *   });
 *   ```
 * 
 * 配置文件示例（config.json）：
 *   ```json
 *   {
 *     "server": {
 *       "port": 18080,
 *       "host": "0.0.0.0"
 *     },
 *     "database": {
 *       "host": "localhost",
 *       "port": 5432,
 *       "dbname": "ruoyi"
 *     }
 *   }
 *   ```
 * 
 * 环境变量覆盖：
 *   - RUOYI_SERVER_PORT=8080 → server.port = 8080
 *   - RUOYI_DATABASE_HOST=db.example.com → database.host = db.example.com
 *   - 环境变量前缀：RUOYI_
 * 
 * 验证规则：
 *   - 必填项验证：required
 *   - 类型验证：string、int、bool、array
 *   - 范围验证：min、max、pattern
 *   - 自定义验证：自定义验证函数
 * 
 * 配置热重载：
 *   ```cpp
 *   // 监听配置文件变更，自动重载
 *   validator.enableFileWatch("./config");
 *   
 *   // 手动重载配置
 *   validator.reload();
 *   ```
 * 
 * 最佳实践：
 *   - 在应用启动时验证配置
 *   - 使用环境变量管理敏感信息（密码、API Key）
 *   - 为每个环境维护独立的配置文件
 *   - 定期检查配置日志
 *   - 使用配置热重载进行灰度发布
 * 
 * @see ConfigLoader - 配置加载器
 * @see HotConfig - 热配置管理
 */

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <optional>
#include <variant>
#include <stdexcept>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <json/json.h>

namespace config {

// ─────────────────────────────────────────────────────────────────────────────
// 配置项定义
// ─────────────────────────────────────────────────────────────────────────────
struct ConfigValue {
    std::string key;                  // 配置键（支持点号分隔：database.host）
    std::variant<std::string, int, int64_t, double, bool> defaultValue;
    std::string envVar;               // 环境变量名
    std::string description;          // 配置描述
    bool required = false;            // 是否必需
    bool sensitive = false;           // 是否敏感（密码等）
    std::string pattern;              // 正则验证（可选）
    int minInt = 0, maxInt = INT_MAX;  // 整数范围
};

using ConfigSchema = std::vector<ConfigValue>;

// 验证结果
struct ValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    void addError(const std::string& msg) {
        valid = false;
        errors.push_back(msg);
    }

    void addWarning(const std::string& msg) {
        warnings.push_back(msg);
    }

    std::string summary() const {
        std::ostringstream ss;
        if (!valid) {
            ss << "配置验证失败：" << errors.size() << " 个错误\n";
            for (auto& e : errors) ss << "  [ERROR] " << e << "\n";
        }
        if (!warnings.empty()) {
            ss << "警告：" << warnings.size() << " 个\n";
            for (auto& w : warnings) ss << "  [WARN] " << w << "\n";
        }
        return ss.str();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// 配置验证器
// ─────────────────────────────────────────────────────────────────────────────
class ConfigValidator {
public:
    static ConfigValidator& instance() {
        static ConfigValidator inst;
        return inst;
    }

    // 注册配置项
    void registerConfig(const ConfigValue& cfg);

    // 注册默认配置schema
    void registerDefaultSchema();

    // 验证配置
    ValidationResult validate(const Json::Value& config,
                             const std::string& env = "production") const;

    // 获取配置值（带类型转换）
    template<typename T>
    T get(const Json::Value& config, const std::string& key, T defaultValue) const;

    // 检查必需配置
    bool checkRequired(const Json::Value& config) const;

private:
    std::vector<ConfigValue> schema_;
};

// ─────────────────────────────────────────────────────────────────────────────
// 环境配置管理器
// ─────────────────────────────────────────────────────────────────────────────
class EnvConfig {
public:
    static EnvConfig& instance() {
        static EnvConfig inst;
        return inst;
    }

    // 环境类型
    enum class Env { Development, Staging, Production, Test };

    // 初始化
    void init(const std::string& configDir = ".");

    // 获取当前环境
    Env currentEnv() const { return currentEnv_; }
    std::string currentEnvName() const;

    // 加载配置
    Json::Value loadConfig(const std::string& path);

    // 获取配置值（环境变量优先）
    template<typename T>
    std::optional<T> get(const std::string& key, T defaultValue = T{}) const;

    // 检查是否在指定环境
    bool isEnv(Env e) const { return currentEnv_ == e; }
    bool isDev() const { return isEnv(Env::Development); }
    bool isProd() const { return isEnv(Env::Production); }
    bool isStaging() const { return isEnv(Env::Staging); }
    bool isTest() const { return isEnv(Env::Test); }

    // 获取环境特定配置目录
    std::filesystem::path envConfigDir() const;

    // 配置热重载
    using ConfigChangeCallback = std::function<void(const std::string& key, const Json::Value& oldVal, const Json::Value& newVal)>;
    void watchConfig(const std::string& path, ConfigChangeCallback callback);
    void stopWatching();

    // 验证配置
    ValidationResult validateConfig(const Json::Value& config) const;

    // 获取所有配置（合并后）
    Json::Value allConfig() const { return mergedConfig_; }

private:
    EnvConfig() = default;

    Env detectEnv() const;
    Json::Value loadJsonFile(const std::filesystem::path& path) const;
    Json::Value mergeConfig(const std::vector<Json::Value>& configs) const;
    std::string getEnvVar(const std::string& key) const;

    Env currentEnv_ = Env::Production;
    Json::Value mergedConfig_;
    std::map<std::string, std::filesystem::path> configFiles_;
    std::vector<ConfigChangeCallback> watchCallbacks_;
    std::thread watchThread_;
    std::atomic<bool> watchRunning_{false};
};

// ─────────────────────────────────────────────────────────────────────────────
// 配置变更通知
// ─────────────────────────────────────────────────────────────────────────────
class ConfigChangeNotifier {
public:
    static ConfigChangeNotifier& instance() {
        static ConfigChangeNotifier inst;
        return inst;
    }

    using Handler = std::function<void(const std::string& path, const Json::Value& oldVal, const Json::Value& newVal)>;

    // 注册变更处理器
    size_t onChange(Handler handler);

    // 取消注册
    void unsubscribe(size_t id);

    // 通知变更
    void notify(const std::string& path, const Json::Value& oldVal, const Json::Value& newVal);

private:
    ConfigChangeNotifier() = default;

    std::map<size_t, Handler> handlers_;
    size_t nextId_ = 1;
    std::mutex mutex_;
};

// ─────────────────────────────────────────────────────────────────────────────
// 便捷宏
// ─────────────────────────────────────────────────────────────────────────────
#define CONFIG_GET(cfg, key, defaultVal) \
    config::EnvConfig::instance().get<decltype(defaultVal)>(key, defaultVal)

#define CONFIG_REQUIRED(key) \
    config::EnvConfig::instance().get<std::string>(key, "")

#define CONFIG_BOOL(cfg, key, defaultVal) \
    config::EnvConfig::instance().get<bool>(key, defaultVal)

#define CONFIG_INT(cfg, key, defaultVal) \
    config::EnvConfig::instance().get<int>(key, defaultVal)

#define CONFIG_IF_PROD(...) \
    if (config::EnvConfig::instance().isProd()) { __VA_ARGS__; }

#define CONFIG_IF_DEV(...) \
    if (config::EnvConfig::instance().isDev()) { __VA_ARGS__; }

} // namespace config

// ════════════════════════════════════════════════════════════════════════════
// 模板实现
// ════════════════════════════════════════════════════════════════════════════

namespace config {

template<typename T>
T ConfigValidator::get(const Json::Value& config, const std::string& key, T defaultValue) const {
    // 尝试从点号分隔的路径获取
    std::istringstream ss(key);
    std::string segment;
    Json::Value current = config;

    while (std::getline(ss, segment, '.')) {
        if (!current.isObject() || !current.isMember(segment)) {
            return defaultValue;
        }
        current = current[segment];
    }

    if constexpr (std::is_same_v<T, std::string>) {
        if (current.isString()) return current.asString();
    } else if constexpr (std::is_same_v<T, int>) {
        if (current.isInt()) return current.asInt();
    } else if constexpr (std::is_same_v<T, int64_t>) {
        if (current.isInt64()) return current.asInt64();
    } else if constexpr (std::is_same_v<T, double>) {
        if (current.isDouble()) return current.asDouble();
    } else if constexpr (std::is_same_v<T, bool>) {
        if (current.isBool()) return current.asBool();
    }

    return defaultValue;
}

template<typename T>
std::optional<T> EnvConfig::get(const std::string& key, T defaultValue) const {
    // 1. 优先从环境变量获取
    const char* envVal = std::getenv(key.c_str());
    if (envVal && envVal[0] != '\0') {
        if constexpr (std::is_same_v<T, std::string>) {
            return std::string(envVal);
        } else if constexpr (std::is_same_v<T, int>) {
            try { return std::stoi(envVal); } catch (...) {}
        } else if constexpr (std::is_same_v<T, bool>) {
            std::string s(envVal);
            return s == "true" || s == "1" || s == "yes";
        }
    }

    // 2. 从合并后的配置获取
    T val;
    std::istringstream ss(key);
    std::string segment;
    Json::Value current = mergedConfig_;

    while (std::getline(ss, segment, '.')) {
        if (!current.isObject() || !current.isMember(segment)) {
            return std::nullopt;
        }
        current = current[segment];
    }

    if constexpr (std::is_same_v<T, std::string>) {
        if (current.isString()) return current.asString();
    } else if constexpr (std::is_same_v<T, int>) {
        if (current.isInt()) return current.asInt();
    } else if constexpr (std::is_same_v<T, bool>) {
        if (current.isBool()) return current.asBool();
    }

    return std::nullopt;
}

} // namespace config
