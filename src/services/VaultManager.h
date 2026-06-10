/**
 * @file VaultManager.h
 * @brief HashiCorp Vault 管理器 — 密钥管理和配置加密
 * 
 * 功能概述：
 *   - Vault 进程管理：启动、停止、监控 Vault 服务
 *   - 自动初始化：首次启动时自动初始化 Vault
 *   - 自动解封：启动时自动解封 Vault
 *   - 密钥管理：存储和检索敏感配置信息
 *   - 数据库存储：支持 PostgreSQL 作为存储后端
 * 
 * 工作流程：
 *   1. 启动 Vault 子进程
 *   2. 等待 Vault 就绪（TCP 连接 + status 检查）
 *   3. 如果未初始化，执行 operator init 生成密钥
 *   4. 执行 operator unseal 解封 Vault
 *   5. 写入初始密钥到 KV 存储
 * 
 * 配置示例（config.json）：
 *   {
 *     "vault": {
 *       "enabled": true,
 *       "exe_path": "vault/vault.exe",
 *       "config_file": "vault/vault-config.hcl",
 *       "addr": "http://127.0.0.1:8200",
 *       "token": "s.xxxxx",
 *       "unseal_keys": ["key1", "key2", "key3"],
 *       "secret_path": "secret/ruoyi-cpp",
 *       "auto_start": true,
 *       "auto_init": true,
 *       "start_timeout_s": 60,
 *       "init_keys_file": "vault/init-keys.json",
 *       "seed_secrets": {
 *         "db_password": "secret_password",
 *         "api_key": "secret_key"
 *       }
 *     }
 *   }
 * 
 * 解封密钥说明：
 *   - 单密钥模式：unsealKey 为单个密钥
 *   - 多密钥模式：unsealKeys 为密钥列表（threshold > 1）
 *   - 兼容性：unsealKey 等价于 unsealKeys[0]
 * 
 * @see ConfigLoader - 配置加载器
 * @see DatabaseService - 数据库服务
 */

#pragma once
#include <string>
#include <map>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <json/json.h>
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

/**
 * @struct VaultManagerConfig
 * @brief Vault 管理器配置
 */
struct VaultManagerConfig {
    bool        enabled       = false;                    ///< 是否启用 Vault
    std::string exePath;                                 ///< Vault 可执行文件路径
    std::string configFile;                              ///< Vault 配置文件（JSON 或 HCL）
    std::string addr          = "http://127.0.0.1:8200"; ///< Vault 地址
    std::string token;                                   ///< Vault 访问令牌
    std::string              unsealKey;                  ///< 单个解封密钥
    std::vector<std::string> unsealKeys;                 ///< 多个解封密钥列表
    std::string secretPath    = "secret/ruoyi-cpp";      ///< 密钥存储路径
    bool        autoStart     = true;                    ///< 是否自动启动 Vault 进程
    bool        showWindow    = false;                   ///< 是否显示 Vault 窗口（Windows）
    int         startTimeoutS = 60;                      ///< 启动超时时间（秒）
    std::string psqlExe;                                 ///< psql 可执行路径（自动探测）
    bool        autoInit      = true;                    ///< 是否自动初始化 Vault
    std::string initKeysFile;                            ///< 保存初始化密钥的文件路径
    std::map<std::string,std::string> seedSecrets;       ///< 初始化后写入的密钥

    /**
     * @brief 从 JSON 配置解析
     * @param c JSON 配置对象
     * @return 解析后的配置
     */
    static VaultManagerConfig fromJson(const Json::Value& c) {
        VaultManagerConfig r;
        r.enabled       = c.get("enabled", false).asBool();
        r.exePath       = c.get("exe_path", "").asString();
        r.configFile    = c.get("config_file", "").asString();
        r.addr          = c.get("addr", "http://127.0.0.1:8200").asString();
        r.token         = c.get("token", "").asString();
        r.unsealKey     = c.get("unseal_key", "").asString();
        if (c.isMember("unseal_keys"))
            for (auto& k : c["unseal_keys"]) r.unsealKeys.push_back(k.asString());
        r.secretPath    = c.get("secret_path", "secret/ruoyi-cpp").asString();
        r.autoStart     = c.get("auto_start", true).asBool();
        r.showWindow    = c.get("show_window", false).asBool();
        r.startTimeoutS = c.get("start_timeout_s", 60).asInt();
        r.psqlExe       = c.get("psql_exe", "").asString();
        r.autoInit      = c.get("auto_init", true).asBool();
        r.initKeysFile  = c.get("init_keys_file", "").asString();
        if (c.isMember("seed_secrets"))
            for (auto& key : c["seed_secrets"].getMemberNames())
                r.seedSecrets[key] = c["seed_secrets"][key].asString();
        return r;
    }
};

/**
 * @class VaultManager
 * @brief Vault 管理器单例
 * 
 * 管理 HashiCorp Vault 的启动、初始化和密钥管理。
 * 采用单例模式，全局唯一实例。
 */
class VaultManager {
public:
    /**
     * @brief 获取单例实例
     * @return VaultManager 单例引用
     */
    static VaultManager& instance();

    /**
     * @brief 启动 Vault
     * 
     * 启动 Vault 进程，等待就绪后自动解封。
     * 如果 Vault 已在运行，则跳过启动步骤。
     * 
     * 流程：
     *   1. 启动 Vault 子进程
     *   2. 等待 Vault 就绪（TCP + status 检查）
     *   3. 如果未初始化，执行 operator init
     *   4. 执行 operator unseal 解封
     *   5. 写入初始密钥到 KV 存储
     * 
     * @param cfg Vault 管理器配置
     * @return 是否启动成功（Vault 已就绪）
     */
    bool start(const VaultManagerConfig& cfg);

    /**
     * @brief 停止 Vault
     */
    void stop();

    /**
     * @brief 检查 Vault 是否运行中
     * @return 是否运行中
     */
    bool isRunning() const;

    /**
     * @brief 获取 Vault 访问令牌
     * 
     * 在 doInit() 后返回最新的令牌。
     * 供 main.cc 注入 ConfigLoader。
     * 
     * @return Vault 访问令牌
     */
    std::string getToken()     const { return cfg_.token; }

    /**
     * @brief 获取 Vault 解封密钥
     * 
     * 在 doInit() 后返回最新的解封密钥。
     * 供 main.cc 注入 ConfigLoader。
     * 
     * @return Vault 解封密钥
     */
    std::string getUnsealKey() const { return cfg_.unsealKey; }

private:
    VaultManager();
    ~VaultManager();
    VaultManager(const VaultManager&) = delete;
    VaultManager& operator=(const VaultManager&) = delete;

    /**
     * @brief 启动 Vault 子进程
     * @return 是否启动成功
     */
    bool spawnProcess();

    /**
     * @brief 准备存储后端
     * 
     * 建立 vault_kv_store 表（解析 HCL 配置获取数据库连接串）。
     * 
     * @return 是否准备成功
     */
    bool prepareStorage();

    /**
     * @brief 等待 Vault 就绪
     * 
     * TCP 连接 + vault status 轮询直到就绪。
     * 
     * @return 是否就绪
     */
    bool waitReady();

    /**
     * @brief 检查 Vault 是否被封印
     * 
     * 运行 vault status 判断是否已封印。
     * 
     * @return 是否被封印
     */
    bool isSealed();

    /**
     * @brief 初始化 Vault
     * 
     * 运行 vault operator init，解析并保存新密钥。
     * 
     * @return 是否初始化成功
     */
    bool doInit();

    /**
     * @brief 写入初始密钥到 KV 存储
     * 
     * 初始化后写入 seedSecrets 到 Vault KV。
     * 
     * @return 是否写入成功
     */
    bool doSeedSecrets();

    /**
     * @brief 解封 Vault
     * 
     * 运行 vault operator unseal。
     * 
     * @return 是否解封成功
     */
    bool doUnseal();

    VaultManagerConfig cfg_;                   ///< Vault 配置
    std::atomic<bool>  running_{false};        ///< 是否运行中
    mutable std::mutex mu_;                    ///< 互斥锁

#ifdef _WIN32
    HANDLE hProc_ = nullptr;                   ///< 进程句柄
    HANDLE hJob_  = nullptr;                   ///< Job 对象
    DWORD  pid_   = 0;                         ///< 进程 ID
#else
    pid_t  pid_   = 0;                         ///< 进程 ID
#endif
};
