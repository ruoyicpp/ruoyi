/**
 * @file AcmeStart.h
 * @brief ACME 证书自动续期启动 — Let's Encrypt 证书的自动化管理
 * 
 * 功能概述：
 *   - ACME 证书申请：通过 ACME 协议自动申请 Let's Encrypt 证书
 *   - 证书自动续期：在证书过期前自动续期
 *   - DNS 验证：支持 DNS-01 验证方式
 *   - 多域名支持：支持单个证书绑定多个域名
 *   - 双引擎支持：支持 CertManager 和 AcmeManager 两种实现
 * 
 * 启动时机：
 *   - 仅在 worker[0] 或单进程模式下启动
 *   - 防止多进程环境下重复申请证书
 * 
 * 配置示例（config.json）：
 *   {
 *     "acme": {
 *       "enabled": true,
 *       "dnsProvider": "cloudflare",
 *       "email": "admin@example.com",
 *       "domains": ["example.com", "www.example.com"]
 *     },
 *     "domain": "example.com"
 *   }
 * 
 * 支持的 DNS 提供商：
 *   - cloudflare - Cloudflare DNS
 *   - aliyun - 阿里云 DNS
 *   - dnspod - DNSPod
 *   - 其他 ACME 兼容提供商
 * 
 * @see AcmeManager - ACME 管理器
 * @see CertManagerDriver - 证书管理驱动
 */

#pragma once
#include "AppIncludes.h"
#include "services/AcmeManager.h"
#include "services/CertManagerDriver.h"

/**
 * @brief 启动 ACME 证书自动续期
 * 
 * 从配置文件读取 ACME 配置，启动证书自动申请和续期服务。
 * 仅在 worker[0] 或单进程模式下启动，防止多进程重复申请。
 * 
 * 流程：
 *   1. 检查当前是否为 worker[0] 或单进程模式
 *   2. 读取配置文件中的 ACME 配置
 *   3. 如果配置了 DNS 提供商，优先使用 CertManager
 *   4. 否则使用 AcmeManager
 *   5. 捕获异常并记录日志
 * 
 * @param configFile 配置文件路径（通常为 config.json）
 */
inline void startAcme(const std::string& configFile) {
    // ── ACME 证书自动续期（仅 worker[0] 或单进程时启动）────────────────
    try {
        // 获取当前 worker 索引，-1 表示单进程模式
        int wkIdx = WorkerOrchestrator::currentWorkerIndex();
        
        // 仅在 worker[0] 或单进程模式下启动
        if (wkIdx == -1 || wkIdx == 0) {
            // 打开配置文件
            std::ifstream af(configFile);
            if (af.is_open()) {
                Json::Value aroot;
                Json::CharReaderBuilder arb;
                std::string aerrs;
                
                // 解析 JSON 配置
                if (Json::parseFromStream(arb, af, &aroot, &aerrs)
                    && aroot.isMember("acme")) {
                    
                    // 解析 CertManager 配置
                    auto cmc = CertManagerAcme::Config::fromJson(aroot["acme"]);
                    
                    // 如果没有配置域名，从顶级 domain 字段读取
                    if (cmc.domains.empty() && aroot.isMember("domain")
                        && !aroot["domain"].asString().empty())
                        cmc.domains.push_back(aroot["domain"].asString());
                    
                    // 优先使用 CertManager（如果配置了 DNS 提供商）
                    bool usedCertManager = false;
                    if (cmc.enabled && !cmc.dnsProvider.empty())
                        usedCertManager = CertManagerAcme::instance().start(cmc);
                    
                    // 如果 CertManager 未启动，使用 AcmeManager
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
