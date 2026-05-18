#pragma once
#include "AppIncludes.h"
#include "services/AcmeManager.h"
#include "services/CertManagerDriver.h"

inline void startAcme(const std::string& configFile) {
    // ── ACME 证书自动续期（仅 worker[0] 或单进程时启动）────────────────
    try {
        int wkIdx = WorkerOrchestrator::currentWorkerIndex();
        if (wkIdx == -1 || wkIdx == 0) {
            std::ifstream af(configFile);
            if (af.is_open()) {
                Json::Value aroot;
                Json::CharReaderBuilder arb;
                std::string aerrs;
                if (Json::parseFromStream(arb, af, &aroot, &aerrs)
                    && aroot.isMember("acme")) {
                    auto cmc = CertManagerAcme::Config::fromJson(aroot["acme"]);
                    if (cmc.domains.empty() && aroot.isMember("domain")
                        && !aroot["domain"].asString().empty())
                        cmc.domains.push_back(aroot["domain"].asString());
                    bool usedCertManager = false;
                    if (cmc.enabled && !cmc.dnsProvider.empty())
                        usedCertManager = CertManagerAcme::instance().start(cmc);
                    if (!usedCertManager) {
                        auto ac = AcmeManager::Config::fromJson(aroot["acme"]);
                        if (ac.enabled) AcmeManager::instance().start(ac);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_WARN << "[ACME] 初始化失败: " << e.what();
    }
}
