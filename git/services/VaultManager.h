#pragma once
#include <string>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

struct VaultManagerConfig {
    bool        enabled       = false;
    std::string exePath;                // vault-with-ui.exe 完整路径
    std::string configFile;             // vault-postgresql-simple.hcl 路径
    std::string addr          = "http://127.0.0.1:8200";
    std::string token;
    std::string unsealKey;              // 自动解封密钥
    std::string secretPath    = "secret/ruoyi-cpp";
    bool        autoStart     = true;   // 拉起 Vault 子进程
    bool        showWindow    = false;
    int         startTimeoutS = 60;     // 等待就绪超时秒
    std::string psqlExe;                // psql 可执行路径（空则自动探测）
    bool        autoInit      = true;   // Vault 未初始化时自动执行 operator init
    std::string initKeysFile;           // 保存新 token/unseal-key 的文件路径
    // 初始化后自动写入 KV： key=字段名(section_key), value=字段值
    std::map<std::string,std::string> seedSecrets;
};

class VaultManager {
public:
    static VaultManager& instance();

    // 启动 Vault（若已在运行则跳过），等待就绪后自动解封
    // 返回 true 表示 Vault 已就绪（已解封）
    bool start(const VaultManagerConfig& cfg);
    void stop();
    bool isRunning() const;
    // doInit() 后获取最新 token / unseal_key（供 main.cc 注入 ConfigLoader）
    std::string getToken()     const { return cfg_.token; }
    std::string getUnsealKey() const { return cfg_.unsealKey; }

private:
    VaultManager();
    ~VaultManager();
    VaultManager(const VaultManager&) = delete;
    VaultManager& operator=(const VaultManager&) = delete;

    bool spawnProcess();
    bool prepareStorage();  // 建 vault_kv_store 表（解析 HCL 取连接串）
    bool waitReady();       // TCP + vault status 轮询直到就绪
    bool isSealed();        // 运行 vault status 判断是否已封印
    bool doInit();          // 运行 vault operator init，解析并保存新密钥
    bool doSeedSecrets();   // 初始化后写入 seedSecrets 到 Vault KV
    bool doUnseal();        // 运行 vault operator unseal

    VaultManagerConfig cfg_;
    std::atomic<bool>  running_{false};
    mutable std::mutex mu_;

#ifdef _WIN32
    HANDLE hProc_ = nullptr;
    HANDLE hJob_  = nullptr;
    DWORD  pid_   = 0;
#else
    pid_t  pid_   = 0;
#endif
};
