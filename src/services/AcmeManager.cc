#include "AcmeManager.h"
#include "../common/NginxEmbedded.h"
#include <trantor/utils/Logger.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/asn1.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

AcmeManager& AcmeManager::instance() {
    static AcmeManager inst;
    return inst;
}

// ── 读 X509 证书的 notAfter 字段，返回距今天数（< 0 = 失败 / 已过期）
int AcmeManager::daysUntilExpiry(const std::string& certPath) {
    if (certPath.empty() || !std::filesystem::exists(certPath)) return -1;
    FILE* fp = std::fopen(certPath.c_str(), "rb");
    if (!fp) return -1;
    X509* cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    std::fclose(fp);
    if (!cert) return -1;

    const ASN1_TIME* notAfter = X509_get0_notAfter(cert);
    int days = 0, secs = 0;
    int ok = ASN1_TIME_diff(&days, &secs, nullptr /* now */, notAfter);
    X509_free(cert);
    if (!ok) return -1;
    return days;
}

bool AcmeManager::start(const Config& cfg) {
    std::lock_guard<std::mutex> lk(mu_);
    if (running_.load()) return true;
    cfg_ = cfg;
    if (!cfg_.enabled) {
        LOG_INFO << "[ACME] disabled in config";
        return false;
    }
    if (cfg_.domains.empty() || cfg_.email.empty()) {
        LOG_WARN << "[ACME] disabled: domains/email is required";
        return false;
    }
    if (!std::filesystem::exists(cfg_.acmeBin)) {
        LOG_WARN << "[ACME] acme_bin not found: " << cfg_.acmeBin
                 << "（请下载 win-acme.exe 放到该路径，否则跳过自动续期）";
        std::cout << "[ACME] acme_bin not found, auto-renew disabled: "
                  << cfg_.acmeBin << std::endl;
        return false;
    }

    // 启动初始检查 + 后台调度循环
    stopRequested_.store(false);
    running_.store(true);
    worker_ = std::thread([this]() { schedulerLoop(); });
    LOG_INFO << "[ACME] scheduler started, domains=" << cfg_.domains.size()
             << " renew_before=" << cfg_.renewDaysBefore << "d"
             << " interval=" << cfg_.checkIntervalHours << "h";
    std::cout << "[ACME] scheduler started, domains=" << cfg_.domains.size() << std::endl;
    return true;
}

void AcmeManager::stop() {
    stopRequested_.store(true);
    if (worker_.joinable()) worker_.join();
    running_.store(false);
}

bool AcmeManager::checkAndRenewOnce() {
    std::lock_guard<std::mutex> lk(mu_);
    int days = daysUntilExpiry(cfg_.certOut);
    if (days < 0) {
        LOG_INFO << "[ACME] cert not found or unreadable, will request initial cert";
        return runAcmeRenew();
    }
    if (days > cfg_.renewDaysBefore) {
        LOG_INFO << "[ACME] cert valid for " << days
                 << " days, no renewal needed (threshold="
                 << cfg_.renewDaysBefore << ")";
        return true;
    }
    LOG_WARN << "[ACME] cert expires in " << days << " days, renewing now";
    std::cout << "[ACME] cert expires in " << days << " days, renewing..." << std::endl;
    return runAcmeRenew();
}

void AcmeManager::schedulerLoop() {
    // 启动后立即检查一次
    try { checkAndRenewOnce(); } catch (const std::exception& e) {
        LOG_WARN << "[ACME] initial check failed: " << e.what();
    }
    auto interval = std::chrono::hours(std::max(1, cfg_.checkIntervalHours));
    while (!stopRequested_.load()) {
        // 分片睡眠以便快速响应 stop
        for (int i = 0; i < 60 * cfg_.checkIntervalHours && !stopRequested_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::minutes(1));
        }
        if (stopRequested_.load()) break;
        try { checkAndRenewOnce(); } catch (const std::exception& e) {
            LOG_WARN << "[ACME] check failed: " << e.what();
        }
    }
    LOG_INFO << "[ACME] scheduler stopped";
}

// ── 调用 win-acme.exe（unattended 模式） ──────────────────────
//   wacs.exe --target manual --host a.com,b.com --emailaddress x@y
//            --accepttos --baseuri https://acme-... --validation http
//            --store pemfiles --pemfilespath nginx/ssl/  --pemfilesname server
// 输出：nginx/ssl/server-crt.pem  + server-key.pem
// 我们在续期完成后将其拷贝/重命名为 cert_out / key_out
bool AcmeManager::runAcmeRenew() {
    std::filesystem::create_directories(cfg_.workDir);
    std::filesystem::path certDir = std::filesystem::path(cfg_.certOut).parent_path();
    if (!certDir.empty()) std::filesystem::create_directories(certDir);

    std::string hostList;
    for (size_t i = 0; i < cfg_.domains.size(); ++i) {
        if (i) hostList += ",";
        hostList += cfg_.domains[i];
    }
    std::string baseUri = cfg_.staging
        ? "https://acme-staging-v02.api.letsencrypt.org/"
        : "https://acme-v02.api.letsencrypt.org/";

    std::string cmd;
#ifdef _WIN32
    // 注意：路径反斜杠 + 引号
    auto winify = [](std::string s) {
        for (auto& c : s) if (c == '/') c = '\\';
        return s;
    };
    std::string exe   = winify(cfg_.acmeBin);
    std::string wd    = winify(cfg_.workDir);
    std::string pemDir = winify(certDir.string());
    cmd = "\"" + exe + "\""
        + " --target manual --host \"" + hostList + "\""
        + " --emailaddress \"" + cfg_.email + "\""
        + " --accepttos"
        + " --baseuri \"" + baseUri + "\""
        + " --validation http --validationport 80"
        + " --store pemfiles"
        + " --pemfilespath \"" + pemDir + "\""
        + " --pemfilesname server"
        + " --configbasepath \"" + wd + "\"";
#else
    // Linux 用 acme.sh：acme.sh --issue -d a.com -d b.com --standalone
    cmd = "\"" + cfg_.acmeBin + "\" --issue --standalone --accountemail \""
        + cfg_.email + "\"";
    for (auto& d : cfg_.domains) cmd += " -d \"" + d + "\"";
    if (cfg_.staging) cmd += " --staging";
    // 然后 install-cert 复制到目标位置
    cmd += " && \"" + cfg_.acmeBin + "\" --install-cert -d \"" + cfg_.domains[0] + "\""
        + " --cert-file \"" + cfg_.certOut + "\""
        + " --key-file  \"" + cfg_.keyOut + "\"";
#endif

    LOG_INFO << "[ACME] running: " << cmd.substr(0, 200) << "...";
    std::cout << "[ACME] running renewal..." << std::endl;
#ifdef _WIN32
    int rc = std::system(("cmd /c \"" + cmd + "\"").c_str());
#else
    int rc = std::system(cmd.c_str());
#endif
    if (rc != 0) {
        LOG_ERROR << "[ACME] command failed rc=" << rc;
        std::cout << "[ACME] renewal failed rc=" << rc << std::endl;
        return false;
    }

#ifdef _WIN32
    // win-acme pemfiles store 输出：server-crt.pem / server-key.pem / server-chain.pem
    auto srcCrt = certDir / "server-crt.pem";
    auto srcKey = certDir / "server-key.pem";
    if (!std::filesystem::exists(srcCrt) || !std::filesystem::exists(srcKey)) {
        LOG_ERROR << "[ACME] pem files not produced: " << srcCrt.string();
        return false;
    }
    std::error_code ec;
    std::filesystem::copy_file(srcCrt, cfg_.certOut,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) { LOG_ERROR << "[ACME] copy crt failed: " << ec.message(); return false; }
    std::filesystem::copy_file(srcKey, cfg_.keyOut,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) { LOG_ERROR << "[ACME] copy key failed: " << ec.message(); return false; }
#endif

    int newDays = daysUntilExpiry(cfg_.certOut);
    LOG_INFO << "[ACME] renewal succeeded, new cert valid for " << newDays << " days";
    std::cout << "[ACME] renewal succeeded, valid for " << newDays << " days" << std::endl;

    // 触发 nginx reload 让新证书生效
    if (cfg_.reloadNginxAfter && NginxEmbedded::instance().isRunning()) {
        LOG_INFO << "[ACME] triggering nginx reload";
        NginxEmbedded::instance().reload();
    }
    return true;
}
