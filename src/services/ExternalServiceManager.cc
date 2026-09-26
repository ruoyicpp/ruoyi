#include "ExternalServiceManager.h"
#include <trantor/utils/Logger.h>
#include <algorithm>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace ruoyi {

ExternalServiceManager& ExternalServiceManager::instance() {
    static ExternalServiceManager inst;
    return inst;
}

ExternalServiceManager::ExternalServiceManager() = default;

ExternalServiceManager::~ExternalServiceManager() {
    stop();
}

void ExternalServiceManager::init(const std::vector<ExternalServiceConfig>& services) {
    std::lock_guard<std::mutex> lk(mu_);
    configs_ = services;

    statuses_.clear();
    for (const auto& cfg : configs_) {
        ExternalServiceStatus st;
        st.name = cfg.name;
        st.displayName = cfg.displayName;
        st.port = cfg.port;
        st.pathPrefix = cfg.pathPrefix;
        st.enabled = cfg.enabled;
        st.running = false;
        st.pid = checkProcess(cfg.exeName);
        statuses_.push_back(st);
    }

    inited_ = true;
    LOG_INFO << "[ExternalService] Initialized with " << configs_.size() << " services";
}

void ExternalServiceManager::start() {
    if (!inited_) {
        LOG_WARN << "[ExternalService] Not initialized, call init() first";
        return;
    }
    if (running_.exchange(true)) return;

    monThread_ = std::thread([this]() { monitorLoop(); });
    LOG_INFO << "[ExternalService] Monitoring started";
}

void ExternalServiceManager::stop() {
    if (!running_.exchange(false)) return;
    if (monThread_.joinable()) monThread_.join();
    LOG_INFO << "[ExternalService] Monitoring stopped";
}

std::vector<ExternalServiceStatus> ExternalServiceManager::getAllStatus() const {
    std::lock_guard<std::mutex> lk(mu_);
    return statuses_;
}

ExternalServiceStatus ExternalServiceManager::getStatus(const std::string& name) const {
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& st : statuses_) {
        if (st.name == name) return st;
    }
    return {};
}

std::vector<ExternalServiceConfig> ExternalServiceManager::getEnabledServices() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<ExternalServiceConfig> enabled;
    for (const auto& cfg : configs_) {
        if (cfg.enabled) enabled.push_back(cfg);
    }
    return enabled;
}

int ExternalServiceManager::checkProcess(const std::string& exeName) {
#ifdef _WIN32
    std::string searchName = exeName;
    std::transform(searchName.begin(), searchName.end(), searchName.begin(),
                  [](unsigned char c) { return std::tolower(c); });

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool ok = Process32FirstW(snapshot, &pe);
    while (ok) {
        std::wstring wname(pe.szExeFile);
        std::string name(wname.begin(), wname.end());
        std::transform(name.begin(), name.end(), name.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        if (name == searchName) {
            CloseHandle(snapshot);
            return static_cast<int>(pe.th32ProcessID);
        }
        ok = Process32NextW(snapshot, &pe);
    }
    CloseHandle(snapshot);
    return 0;
#else
    // Linux: use pgrep
    std::string cmd = "pgrep -x \"" + exeName + "\"";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return 0;
    int pid = 0;
    if (fscanf(fp, "%d", &pid) == 1) {}
    pclose(fp);
    return pid;
#endif
}

void ExternalServiceManager::monitorLoop() {
    while (running_) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            for (auto& st : statuses_) {
                int newPid = checkProcess(st.enabled ? configs_[&st - &statuses_[0]].exeName : "");
                if (newPid != st.pid) {
                    st.pid = newPid;
                    st.running = (newPid != 0);
                    LOG_INFO << "[ExternalService] " << st.displayName
                             << " status changed: " << (st.running ? "running" : "stopped")
                             << (newPid ? " (pid=" + std::to_string(newPid) + ")" : "");
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(checkIntervalMs_));
    }
}

} // namespace ruoyi
