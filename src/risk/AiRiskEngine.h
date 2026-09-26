/**
 * @file AiRiskEngine.h
 * @brief AI 网关风控 — 请求特征异步评分，高风险 IP 自动封禁
 *
 * 功能概述：
 *   - 异步评分：请求链路只入队特征（O(1)），后台批量调 AI 接口打分
 *   - 采样控制：sample_rate 控制送检比例（1.0=全量，0.1=10%）
 *   - 高风险处置：score >= threshold → RiskStore 封禁 + nftables 内核 DROP
 *   - 审计联动：评分结果入 AuditQueue（event_type=ai_risk）
 *   - 优雅降级：AI 接口不可用时静默跳过，不影响业务
 *
 * AI 接口契约（POST {endpoint}/score）：
 *   请求: {"samples":[{"ip":"1.2.3.4","method":"GET","uri":"/api/x",
 *          "ua":"...","args_len":12,"body_len":0,"headers_n":8}]}
 *   响应: {"scores":[{"ip":"1.2.3.4","score":0.92,"reason":"sqlmap pattern"}]}
 *   score ∈ [0,1]，>= threshold 判定高风险
 *
 * 接入任意 AI 服务：Python FastAPI/Flask 包装模型即可，协议无关。
 *
 * 配置项（config.json → security.ai_risk）：
 *   - enabled: 总开关（默认 false）
 *   - endpoint: AI 评分服务地址（默认 http://127.0.0.1:9100）
 *   - threshold: 高风险阈值（默认 0.85）
 *   - sample_rate: 采样率 0~1（默认 1.0）
 *   - batch_size: 批量送检条数（默认 50）
 *   - ban_seconds: 高风险封禁时长（默认 7200）
 *   - queue_capacity: 特征队列上限（默认 5000）
 */

#pragma once
#include <deque>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <random>
#include <sstream>
#include <ctime>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include "../common/HttpCaller.h"
#include "../common/IpUtils.h"
#include "../waf/RiskStore.h"
#include "../waf/NftBan.h"
#include "../audit/AuditQueue.h"

/**
 * @class AiRiskEngine
 * @brief AI 风控引擎单例
 *
 * 请求链路调用 inspect() 仅做采样入队（纳秒级），
 * 实际评分由 flush() 批量异步完成，高风险结果回写 RiskStore。
 */
class AiRiskEngine {
public:
    struct Config {
        bool        enabled       = false;
        std::string endpoint      = "http://127.0.0.1:9100";
        double      threshold     = 0.85;
        double      sampleRate    = 1.0;
        int         batchSize     = 50;
        int         banSeconds    = 7200;
        int         queueCapacity = 5000;
    };

    static AiRiskEngine& instance() {
        static AiRiskEngine inst;
        return inst;
    }

    /**
     * @brief 初始化
     * @param cfg config.json 的 security.ai_risk 节点
     * @note 仅 Linux 版本启用；其他平台直接跳过
     */
    void init(const Config& cfg) {
#ifndef __linux__
        (void)cfg;
        LOG_INFO << "[AiRisk] skipped: feature only available on Linux build";
        return;
#endif
        cfg_ = cfg;
        if (!cfg_.enabled) { LOG_INFO << "[AiRisk] disabled"; return; }
        LOG_INFO << "[AiRisk] enabled -> " << cfg_.endpoint
                 << " threshold=" << cfg_.threshold
                 << " sample=" << cfg_.sampleRate
                 << " batch=" << cfg_.batchSize;
    }

    /**
     * @brief 请求链路入口：采样 + 特征提取 + 入队（不阻塞）
     * @param req HTTP 请求
     * @note 在 WAF advice 之后调用，已通过 WAF 的请求才送检
     */
    void inspect(const drogon::HttpRequestPtr& req) {
        if (!cfg_.enabled) return;
        // 采样
        if (cfg_.sampleRate < 1.0) {
            std::uniform_real_distribution<double> d(0.0, 1.0);
            if (d(rng_) >= cfg_.sampleRate) return;
        }
        Json::Value f;
        f["ip"]        = IpUtils::getIpAddr(req);
        f["method"]    = req->methodString();
        f["uri"]       = req->path();
        f["ua"]        = req->getHeader("User-Agent").substr(0, 256);
        f["args_len"]  = (int)req->query().size();
        f["body_len"]  = (int)req->body().size();
        f["headers_n"] = (int)req->headers().size();
        f["ts"]        = (Json::Int64)std::time(nullptr);

        std::lock_guard<std::mutex> lk(mu_);
        if ((int)queue_.size() >= cfg_.queueCapacity) {
            queue_.pop_front();   // 背压：丢最旧
            dropped_++;
            return;
        }
        queue_.push_back(std::move(f));
        sampled_++;
        if ((int)queue_.size() >= cfg_.batchSize) flushLocked();
    }

    /// 定时批量送检（主循环 runEvery 驱动）
    void flush() {
        if (!cfg_.enabled) return;
        std::lock_guard<std::mutex> lk(mu_);
        flushLocked();
    }

    /// 统计
    uint64_t sampled()  const { return sampled_; }
    uint64_t banned()   const { return banned_; }
    uint64_t dropped()  const { return dropped_; }
    size_t   pending()  const {
        std::lock_guard<std::mutex> lk(mu_);
        return queue_.size();
    }
    bool isEnabled() const { return cfg_.enabled; }

private:
    AiRiskEngine() : rng_(std::random_device{}()) {}

    /// 取一批送 AI 评分（调用方须持锁）
    void flushLocked() {
        if (queue_.empty() || inFlight_) return;
        size_t n = std::min((size_t)cfg_.batchSize, queue_.size());
        std::vector<Json::Value> batch(queue_.begin(), queue_.begin() + n);
        queue_.erase(queue_.begin(), queue_.begin() + n);
        inFlight_ = true;

        Json::Value req;
        for (auto& b : batch) req["samples"].append(b);
        std::string body = Json::writeString(Json::StreamWriterBuilder(), req);
        std::string url  = cfg_.endpoint + "/score";
        double threshold = cfg_.threshold;
        int    banSecs   = cfg_.banSeconds;

        HttpCaller::asyncPost(url, body, "application/json",
            [this, threshold, banSecs](bool ok, int status,
                                       const std::string& respBody) {
                inFlight_ = false;
                if (!ok || status != 200) {
                    LOG_WARN << "[AiRisk] score request failed: HTTP " << status;
                    return;
                }
                Json::Value j; std::string err;
                Json::CharReaderBuilder rb;
                std::istringstream ss(respBody);
                if (!Json::parseFromStream(rb, ss, &j, &err)) return;
                for (auto& s : j["scores"]) {
                    double score = s.get("score", 0.0).asDouble();
                    std::string ip = s.get("ip", "").asString();
                    if (ip.empty() || score < threshold) continue;
                    // 高风险：风控封禁 + 内核 DROP + 审计
                    std::string reason = "ai_risk:" +
                        s.get("reason", "high_score").asString();
                    RiskStore::instance().ban(ip, banSecs, reason);
                    NftBan::instance().banIp(ip, banSecs);
                    banned_++;
                    LOG_WARN << "[AiRisk] banned " << ip
                             << " score=" << score
                             << " reason=" << reason;
                    Json::Value ev;
                    ev["ts"]         = (Json::Int64)std::time(nullptr);
                    ev["ip"]         = ip;
                    ev["event_type"] = "ai_risk";
                    ev["action"]     = "ban";
                    ev["rule"]       = "ai_score";
                    ev["matched"]    = reason;
                    ev["detail"]     = "score=" + std::to_string(score);
                    AuditQueue::instance().enqueue(std::move(ev));
                }
            });
    }

    Config              cfg_;
    std::deque<Json::Value> queue_;
    mutable std::mutex  mu_;
    std::mt19937        rng_;
    std::atomic<bool>   inFlight_{false};
    std::atomic<uint64_t> sampled_{0};
    std::atomic<uint64_t> banned_{0};
    std::atomic<uint64_t> dropped_{0};
};
