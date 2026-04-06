#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef htonll
#define htonll(x) ((1==htonl(1))?(x):((uint64_t)htonl((x)&0xFFFFFFFF)<<32)|htonl((x)>>32))
#endif
#endif

#include "VaultManager.h"
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <fstream>
#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

VaultManager& VaultManager::instance() {
    static VaultManager inst;
    return inst;
}

VaultManager::VaultManager() {
#ifdef _WIN32
    hJob_ = CreateJobObjectA(nullptr, nullptr);
    if (hJob_) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob_, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }
#endif
}

VaultManager::~VaultManager() { stop(); }

// ── TCP 连通检测（无需 HTTP）────────────────────────────────────
static bool tcpConnectable(const std::string& host, uint16_t port) {
#ifdef _WIN32
    WSADATA wd; WSAStartup(MAKEWORD(2,2), &wd);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    // 非阻塞
    u_long mode = 1; ioctlsocket(s, FIONBIO, &mode);
    sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &sa.sin_addr);
    connect(s, (sockaddr*)&sa, sizeof(sa));
    fd_set fds; FD_ZERO(&fds); FD_SET(s, &fds);
    timeval tv{1, 0};
    bool ok = select(0, nullptr, &fds, nullptr, &tv) > 0;
    closesocket(s);
    return ok;
#else
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return false;
    sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &sa.sin_addr);
    bool ok = (connect(s, (sockaddr*)&sa, sizeof(sa)) == 0);
    close(s);
    return ok;
#endif
}

// ── 运行一条 vault 命令，返回 stdout ──────────────────────────
static std::string runVaultCmd(const std::string& exePath,
                                const std::string& addr,
                                const std::string& token,
                                const std::string& args) {
#ifdef _WIN32
    std::string cmd = "cmd /C \"set VAULT_ADDR=" + addr
        + " & set VAULT_TOKEN=" + token
        + " & \"\"" + exePath + "\"\" " + args + " 2>nul\"";
    FILE* p = _popen(cmd.c_str(), "r");
#else
    std::string cmd = "VAULT_ADDR=\"" + addr + "\" VAULT_TOKEN=\"" + token
        + "\" \"" + exePath + "\" " + args + " 2>/dev/null";
    FILE* p = popen(cmd.c_str(), "r");
#endif
    if (!p) return "";
    std::string out;
    char buf[256];
    while (fgets(buf, sizeof(buf), p)) out += buf;
#ifdef _WIN32
    _pclose(p);
#else
    pclose(p);
#endif
    return out;
}

bool VaultManager::start(const VaultManagerConfig& cfg) {
    std::lock_guard<std::mutex> lk(mu_);
    cfg_ = cfg;
    if (!cfg_.enabled) return true;

    // 检查 Vault 是否已在运行
    bool alreadyUp = tcpConnectable("127.0.0.1", 8200);

    if (!alreadyUp) {
        if (!cfg_.autoStart) {
            LOG_WARN << "[Vault] 未运行且 auto_start=false，跳过";
            return false;
        }
        if (!std::ifstream(cfg_.exePath).good()) {
            LOG_ERROR << "[Vault] 找不到 exe: " << cfg_.exePath;
            return false;
        }
        prepareStorage(); // 建 vault_kv_store 表（如尚不存在）
        if (!spawnProcess()) return false;
    } else {
        std::cout << "[Vault] 检测到 Vault 已在运行，跳过启动" << std::endl;
    }

    // 等待就绪
    if (!waitReady()) {
        LOG_ERROR << "[Vault] 等待超时，Vault 未就绪";
        return false;
    }

    // 自动解封
    if (!cfg_.unsealKey.empty()) {
        auto statusOut = runVaultCmd(cfg_.exePath, cfg_.addr, "", "status");
        bool initialized = statusOut.find("Initialized        true") != std::string::npos;
        bool sealed      = statusOut.find("Sealed             true") != std::string::npos;
        if (!initialized) {
            if (cfg_.autoInit) {
                std::cout << "[Vault] Vault 尚未初始化，执行自动初始化..." << std::endl;
                if (!doInit()) {
                    LOG_WARN << "[Vault] 自动初始化失败";
                    return true; // 不阻断后端启动
                }
                // 初始化后重新读取状态
                initialized = true; sealed = true;
            } else {
                std::cout << "[Vault] Vault 尚未初始化，请手动执行 vault operator init" << std::endl;
            }
        }
        if (initialized && sealed) {
            std::cout << "[Vault] 检测到已封印，执行自动解封..." << std::endl;
            if (doUnseal())
                std::cout << "[Vault] ✅ 解封成功" << std::endl;
            else
                LOG_WARN << "[Vault] ⚠️ 解封失败，请手动解封";
        } else if (!sealed) {
            std::cout << "[Vault] ✅ Vault 已处于解封状态" << std::endl;
        }
    }
    return true;
}

bool VaultManager::spawnProcess() {
#ifdef _WIN32
    std::string cmdStr = "\"" + cfg_.exePath + "\" server -config=\"" + cfg_.configFile + "\"";
    LOG_INFO << "[Vault] Launching: " << cmdStr;

    STARTUPINFOA si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = cfg_.showWindow ? SW_SHOW : SW_HIDE;
    DWORD flags    = CREATE_SUSPENDED | (cfg_.showWindow ? 0 : CREATE_NO_WINDOW);
    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(nullptr, const_cast<char*>(cmdStr.c_str()),
                        nullptr, nullptr, FALSE, flags, nullptr, nullptr, &si, &pi)) {
        LOG_ERROR << "[Vault] CreateProcess failed: " << GetLastError();
        return false;
    }
    if (hJob_) AssignProcessToJobObject(hJob_, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    hProc_ = pi.hProcess;
    pid_   = pi.dwProcessId;
    running_ = true;
    std::cout << "[Vault] Started pid=" << pid_ << "  " << cfg_.addr << std::endl;
    LOG_INFO << "[Vault] Started pid=" << pid_;
    return true;
#else
    pid_ = fork();
    if (pid_ == 0) {
        const char* args[] = { cfg_.exePath.c_str(), "server",
                                "-config", cfg_.configFile.c_str(), nullptr };
        execv(cfg_.exePath.c_str(), const_cast<char* const*>(args));
        _exit(1);
    }
    if (pid_ < 0) { LOG_ERROR << "[Vault] fork failed"; return false; }
    running_ = true;
    LOG_INFO << "[Vault] Started pid=" << pid_;
    return true;
#endif
}

// 从 HCL 文件提取指定 key 的字符串值
static std::string parseHcl(const std::string& content, const std::string& key) {
    auto pos = content.find(key + " = \"");
    if (pos == std::string::npos) return "";
    pos += key.size() + 4;
    auto end = content.find('"', pos);
    return end == std::string::npos ? "" : content.substr(pos, end - pos);
}

// 探测可用的 psql 可执行文件
static std::string findPsql(const std::string& hint) {
    if (!hint.empty() && std::ifstream(hint).good()) return hint;
    const char* candidates[] = {
        "H:\\msys64\\msys64\\mingw64\\bin\\psql.exe",
        "H:\\msys64\\mingw64\\bin\\psql.exe",
        "C:\\Program Files\\PostgreSQL\\17\\bin\\psql.exe",
        "C:\\Program Files\\PostgreSQL\\16\\bin\\psql.exe",
        "C:\\Program Files\\PostgreSQL\\15\\bin\\psql.exe",
        nullptr
    };
    for (int i = 0; candidates[i]; ++i)
        if (std::ifstream(candidates[i]).good()) return candidates[i];
    return "psql"; // PATH 里找
}

bool VaultManager::prepareStorage() {
    if (cfg_.configFile.empty()) return false;
    std::ifstream hclF(cfg_.configFile);
    if (!hclF.is_open()) { LOG_WARN << "[Vault] 找不到 HCL: " << cfg_.configFile; return false; }
    std::string hcl((std::istreambuf_iterator<char>(hclF)), {});

    std::string connUrl = parseHcl(hcl, "connection_url");
    if (connUrl.empty()) { LOG_WARN << "[Vault] HCL 中未找到 connection_url"; return false; }

    std::string psql = findPsql(cfg_.psqlExe);
    // vault_kv_store 表定义（来自 Vault PostgreSQL storage 文档）
    const char* sql =
        "CREATE TABLE IF NOT EXISTS vault_kv_store ("
        "parent_path TEXT COLLATE \"C\" NOT NULL,"
        "path TEXT COLLATE \"C\","
        "key TEXT COLLATE \"C\","
        "value BYTEA,"
        "CONSTRAINT pkey PRIMARY KEY (path, key));"
        "CREATE INDEX IF NOT EXISTS parent_path_idx "
        "ON vault_kv_store (parent_path);";

#ifdef _WIN32
    std::string cmd = "\"" + psql + "\" \"" + connUrl + "\" -c \"" + sql + "\" 2>&1";
    FILE* p = _popen(cmd.c_str(), "r");
#else
    std::string cmd = "\"" + psql + "\" \"" + connUrl + "\" -c '" + sql + "' 2>&1";
    FILE* p = popen(cmd.c_str(), "r");
#endif
    if (!p) { LOG_WARN << "[Vault] 无法调用 psql"; return false; }
    std::string out; char buf[256];
    while (fgets(buf, sizeof(buf), p)) out += buf;
#ifdef _WIN32
    _pclose(p);
#else
    pclose(p);
#endif
    if (out.find("ERROR") != std::string::npos && out.find("already exists") == std::string::npos) {
        LOG_WARN << "[Vault] 建表失败: " << out.substr(0, 200);
        return false;
    }
    std::cout << "[Vault] vault_kv_store 已就绪" << std::endl;
    return true;
}

bool VaultManager::waitReady() {
    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::seconds(cfg_.startTimeoutS);
    int elapsed = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        // 先用 TCP 快速检测端口是否可达
        if (tcpConnectable("127.0.0.1", 8200)) {
            // 端口可达后再用 vault status 确认进程就绪
            auto out = runVaultCmd(cfg_.exePath, cfg_.addr, "", "status");
            if (!out.empty() && out.find("Initialized") != std::string::npos) {
                std::cout << "[Vault] ✅ 就绪（" << elapsed << "s）" << std::endl;
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
        elapsed += 2;
        if (elapsed % 10 == 0)
            std::cout << "[Vault] 等待就绪... " << elapsed << "s / "
                      << cfg_.startTimeoutS << "s" << std::endl;
    }
    return false;
}

bool VaultManager::doInit() {
    // 使用 -format=json 方便解析
    auto out = runVaultCmd(cfg_.exePath, cfg_.addr, "",
                           "operator init -key-shares=1 -key-threshold=1 -format=json");
    if (out.empty()) { LOG_WARN << "[Vault] operator init 返回为空"; return false; }

    // 解析 JSON
    Json::Value root; Json::CharReaderBuilder rb; std::string err;
    std::istringstream ss(out);
    if (!Json::parseFromStream(rb, ss, &root, &err)) {
        LOG_WARN << "[Vault] 解析 init 输出失败: " << err;
        return false;
    }
    std::string unsealKey = root["unseal_keys_b64"][0].asString();
    std::string rootToken = root["root_token"].asString();
    if (unsealKey.empty() || rootToken.empty()) {
        LOG_WARN << "[Vault] 初始化输出缺少字段";
        return false;
    }

    // 更新内存配置，供后续解封和 VaultClient 使用
    cfg_.unsealKey = unsealKey;
    cfg_.token     = rootToken;

    // 保存到文件（方便用户更新 config.json）
    std::string keysPath = cfg_.initKeysFile.empty()
        ? "vault-init-keys.json" : cfg_.initKeysFile;
    std::ofstream kf(keysPath);
    if (kf.is_open()) {
        Json::Value kj;
        kj["unseal_key"] = unsealKey;
        kj["root_token"] = rootToken;
        kf << kj.toStyledString();
        kf.close();
    }

    std::cout << "\n[Vault] ✅ 初始化完成！请更新 config.json:"
              << "\n  unseal_key : " << unsealKey
              << "\n  token      : " << rootToken
              << "\n  已保存至: " << keysPath << "\n" << std::endl;

    // 解封后再写 KV：先解封再 seed
    if (doUnseal() && !cfg_.seedSecrets.empty())
        doSeedSecrets();
    return true;
}

bool VaultManager::doSeedSecrets() {
    if (cfg_.seedSecrets.empty()) return true;

    // 启用 KV 密展（已存在则忽略）
    runVaultCmd(cfg_.exePath, cfg_.addr, cfg_.token,
                "secrets enable -path=secret kv");

    // 将 secrets 写入临时 JSON 文件（避免 shell 转义特殊字符）
    std::string tmpFile;
#ifdef _WIN32
    char buf[MAX_PATH]; GetTempPathA(MAX_PATH, buf);
    tmpFile = std::string(buf) + "vault_seed.json";
#else
    tmpFile = "/tmp/vault_seed.json";
#endif
    {
        Json::Value obj;
        for (auto& kv : cfg_.seedSecrets) obj[kv.first] = kv.second;
        std::ofstream f(tmpFile);
        if (!f.is_open()) { LOG_WARN << "[Vault] 无法写临时文件"; return false; }
        Json::StreamWriterBuilder wb; wb["indentation"] = "";
        f << Json::writeString(wb, obj);
    }

    auto out = runVaultCmd(cfg_.exePath, cfg_.addr, cfg_.token,
                           "kv put " + cfg_.secretPath + " @" + tmpFile);
    std::remove(tmpFile.c_str());

    bool ok = out.find("Success") != std::string::npos
           || out.find("created_time") != std::string::npos
           || out.empty(); // empty = no error on older KV
    if (ok)
        std::cout << "[Vault] ✅ " << cfg_.seedSecrets.size()
                  << " 个密钥已写入 " << cfg_.secretPath << std::endl;
    else
        LOG_WARN << "[Vault] KV 写入失败: " << out.substr(0, 200);
    return ok;
}

bool VaultManager::isSealed() {
    auto out = runVaultCmd(cfg_.exePath, cfg_.addr, cfg_.token, "status");
    return out.find("Sealed             true") != std::string::npos;
}

bool VaultManager::doUnseal() {
    auto out = runVaultCmd(cfg_.exePath, cfg_.addr, cfg_.token,
                           "operator unseal " + cfg_.unsealKey);
    // 已解封或成功解封
    if (out.find("already unsealed") != std::string::npos) return true;
    if (out.find("Sealed             false") != std::string::npos) return true;
    if (out.find("Sealed: false")     != std::string::npos) return true;
    // 通用：在 Sealed 这一行里找 false
    auto pos = out.find("Sealed");
    if (pos != std::string::npos) {
        auto eol = out.find('\n', pos);
        auto line = out.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
        if (line.find("false") != std::string::npos) return true;
    }
    return false;
}

void VaultManager::stop() {
    std::lock_guard<std::mutex> lk(mu_);
    if (!running_) return;
#ifdef _WIN32
    if (hProc_) {
        TerminateProcess(hProc_, 0);
        WaitForSingleObject(hProc_, 3000);
        CloseHandle(hProc_); hProc_ = nullptr; pid_ = 0;
    }
#else
    if (pid_ > 0) { kill(pid_, SIGTERM); pid_ = 0; }
#endif
    running_ = false;
    LOG_INFO << "[Vault] Stopped";
}

bool VaultManager::isRunning() const {
    if (!running_) return false;
#ifdef _WIN32
    if (!hProc_) return false;
    DWORD code = STILL_ACTIVE;
    GetExitCodeProcess(hProc_, &code);
    return code == STILL_ACTIVE;
#else
    return pid_ > 0 && waitpid(pid_, nullptr, WNOHANG) == 0;
#endif
}
