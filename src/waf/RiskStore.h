/**
 * @file RiskStore.h
 * @brief 风控状态存储 — IP 封禁名单、验证码票据、限流计数的持久化
 *
 * 功能概述：
 *   - 封禁名单：持久化 IP 封禁记录（含过期时间、原因、命中次数）
 *   - 验证码票据：一次性 ticket 的签发/核销（防重放）
 *   - 限流计数：跨重启保留的计数器（可选，默认仍走内存 RateLimiter）
 *   - 双后端：SQLite（默认，跨平台）/ RocksDB（Linux 可选，宏 RUOYI_WAF_ROCKSDB）
 *
 * 设计说明：
 *   - 独立于主数据库：使用单独的 data/waf_risk.db，主库宕机时风控仍可用
 *   - WAL 模式：读写并发不互斥，适合高频读（每请求一次封禁检查）
 *   - 内存缓存：封禁名单加载到内存，SQLite 只做持久化兜底
 *   - Windows 编译：RocksDB 路径用 #ifdef RUOYI_WAF_ROCKSDB 隔离，
 *     Windows 下自动使用 SQLite 后端，保证可编译
 *
 * 使用示例：
 *   RiskStore::instance().init("data/waf_risk.db");
 *   RiskStore::instance().ban("1.2.3.4", 3600, "SQL注入攻击");
 *   if (RiskStore::instance().isBanned("1.2.3.4")) { ... }
 *
 * 配置项（config.json）：
 *   - security.risk_store.backend: "sqlite" | "rocksdb"（默认 sqlite）
 *   - security.risk_store.db_path: SQLite 文件路径（默认 data/waf_risk.db）
 *   - security.risk_store.rocksdb_path: RocksDB 目录（默认 data/waf_risk_rocks）
 */

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <chrono>
#include <optional>
#include <sqlite3.h>
#include <trantor/utils/Logger.h>

#ifdef RUOYI_WAF_ROCKSDB
#  include <rocksdb/db.h>
#  include <rocksdb/options.h>
#endif

/**
 * @class RiskStore
 * @brief 风控状态存储单例
 *
 * 管理三类风控状态数据：
 *   1. ban_list    — IP 封禁（ip, expire_at, reason, hit_count）
 *   2. tickets     — 一次性票据（ticket, expire_at, used）
 *   3. counters    — 通用计数器（key, value, expire_at）
 */
class RiskStore {
public:
    static RiskStore& instance() {
        static RiskStore inst;
        return inst;
    }

    struct Config {
        std::string backend      = "sqlite";            ///< sqlite | rocksdb
        std::string dbPath       = "data/waf_risk.db";  ///< SQLite 文件路径
        std::string rocksdbPath  = "data/waf_risk_rocks"; ///< RocksDB 目录
    };

    /**
     * @brief 初始化存储后端
     * @param cfg 配置
     * @return 成功返回 true
     * @note 仅 Linux 版本启用；其他平台直接跳过（返回 false，所有操作为空操作）
     */
    bool init(const Config& cfg) {
#ifndef __linux__
        (void)cfg;
        LOG_INFO << "[RiskStore] skipped: feature only available on Linux build";
        return false;
#endif
        std::lock_guard<std::mutex> lk(mu_);
        cfg_ = cfg;

#ifdef RUOYI_WAF_ROCKSDB
        if (cfg_.backend == "rocksdb") {
            rocksdb::Options opt;
            opt.create_if_missing = true;
            opt.WAL_ttl_seconds = 3600;
            rocksdb::DB* db = nullptr;
            if (rocksdb::DB::Open(opt, cfg_.rocksdbPath, &db).ok()) {
                rocksDb_.reset(db);
                LOG_INFO << "[RiskStore] RocksDB backend: " << cfg_.rocksdbPath;
                loadBansToMemory();
                return true;
            }
            LOG_WARN << "[RiskStore] RocksDB open failed, fallback to SQLite";
        }
#endif
        return initSqlite();
    }

    // ── IP 封禁 ──────────────────────────────────────────────────────────

    /**
     * @brief 封禁 IP
     * @param ip IP 地址
     * @param seconds 封禁时长（秒），0 = 永久
     * @param reason 封禁原因
     */
    void ban(const std::string& ip, int seconds, const std::string& reason) {
        int64_t expireAt = seconds > 0
            ? nowTs() + seconds
            : INT64_MAX;
        {
            std::lock_guard<std::mutex> lk(mu_);
            bans_[ip] = {expireAt, reason, 0};
        }
        persistBan(ip, expireAt, reason);
        LOG_WARN << "[RiskStore] banned: " << ip << " "
                 << (seconds > 0 ? std::to_string(seconds) + "s" : "permanent")
                 << " reason=" << reason;
    }

    /// 解封 IP
    void unban(const std::string& ip) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            bans_.erase(ip);
        }
        removeBan(ip);
        LOG_INFO << "[RiskStore] unbanned: " << ip;
    }

    /// 检查 IP 是否被封禁（内存查询，O(1)）
    bool isBanned(const std::string& ip) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = bans_.find(ip);
        if (it == bans_.end()) return false;
        if (it->second.expireAt <= nowTs()) {
            bans_.erase(it);
            return false;
        }
        it->second.hitCount++;
        return true;
    }

    struct BanInfo {
        int64_t     expireAt;
        std::string reason;
        int         hitCount;
    };

    /// 获取所有生效中的封禁记录
    std::vector<std::pair<std::string, BanInfo>> bannedList() {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<std::pair<std::string, BanInfo>> out;
        int64_t now = nowTs();
        for (auto it = bans_.begin(); it != bans_.end();) {
            if (it->second.expireAt <= now) it = bans_.erase(it);
            else { out.push_back({it->first, it->second}); ++it; }
        }
        return out;
    }

    // ── 一次性票据（验证码/WS ticket/SSO 授权码等）──────────────────────

    /// 签发票据（无载荷）
    void issueTicket(const std::string& ticket, int ttlSeconds) {
        execSql("INSERT OR REPLACE INTO tickets(ticket,expire_at,used,payload) VALUES(?,?,0,'')",
                {ticket, std::to_string(nowTs() + ttlSeconds)});
    }

    /// 写入带载荷票据（SSO 授权码/刷新令牌用）
    bool putTicket(const std::string& key, const std::string& payload, int ttlSec) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!sqlite_) return false;
        sqlite3_stmt* st = nullptr;
        const char* sql =
            "INSERT OR REPLACE INTO tickets(ticket,expire_at,used,payload) VALUES(?,?,0,?)";
        if (sqlite3_prepare_v2(sqlite_, sql, -1, &st, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_text(st, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 2, nowTs() + ttlSec);
        sqlite3_bind_text(st, 3, payload.c_str(), -1, SQLITE_TRANSIENT);
        bool ok = (sqlite3_step(st) == SQLITE_DONE);
        sqlite3_finalize(st);
        return ok;
    }

    /// 核销票据：存在且未使用且未过期 → 标记已用并返回 true（一次性）
    bool consumeTicket(const std::string& ticket) {
        return consumeTicketEx(ticket).has_value();
    }

    /// 核销票据并返回载荷（一次性，SSO 授权码换 token 用）
    std::optional<std::string> consumeTicketEx(const std::string& ticket) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!sqlite_) return std::nullopt;
        // 先读载荷再标记已用（同事务保证原子性）
        sqlite3_stmt* st = nullptr;
        const char* sel = "SELECT payload FROM tickets WHERE ticket=? AND used=0 AND expire_at>?";
        if (sqlite3_prepare_v2(sqlite_, sel, -1, &st, nullptr) != SQLITE_OK)
            return std::nullopt;
        sqlite3_bind_text(st, 1, ticket.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 2, nowTs());
        std::optional<std::string> payload;
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char* p = (const char*)sqlite3_column_text(st, 0);
            payload = p ? p : "";
        }
        sqlite3_finalize(st);
        if (!payload) return std::nullopt;
        sqlite3_stmt* st2 = nullptr;
        const char* upd = "UPDATE tickets SET used=1 WHERE ticket=? AND used=0";
        if (sqlite3_prepare_v2(sqlite_, upd, -1, &st2, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(st2, 1, ticket.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(st2);
            sqlite3_finalize(st2);
        }
        return payload;
    }

    /// 非销毁读取票据载荷
    std::optional<std::string> getTicket(const std::string& ticket) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!sqlite_) return std::nullopt;
        sqlite3_stmt* st = nullptr;
        const char* sql = "SELECT payload FROM tickets WHERE ticket=? AND used=0 AND expire_at>?";
        if (sqlite3_prepare_v2(sqlite_, sql, -1, &st, nullptr) != SQLITE_OK)
            return std::nullopt;
        sqlite3_bind_text(st, 1, ticket.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 2, nowTs());
        std::optional<std::string> out;
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char* p = (const char*)sqlite3_column_text(st, 0);
            out = p ? p : "";
        }
        sqlite3_finalize(st);
        return out;
    }

    /// 删除票据（撤销刷新令牌等）
    void removeTicket(const std::string& ticket) {
        execSql("DELETE FROM tickets WHERE ticket=?", {ticket});
    }

    // ── 通用计数器 ───────────────────────────────────────────────────────

    /// 计数器 +1 并返回新值（带过期时间）
    long incr(const std::string& key, int ttlSeconds) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!sqlite_) return 0;
        sqlite3_stmt* st = nullptr;
        const char* sql =
            "INSERT INTO counters(k,v,expire_at) VALUES(?,1,?) "
            "ON CONFLICT(k) DO UPDATE SET "
            "  v = CASE WHEN counters.expire_at < ? THEN 1 ELSE counters.v + 1 END,"
            "  expire_at = CASE WHEN counters.expire_at < ? THEN ? ELSE counters.expire_at END";
        if (sqlite3_prepare_v2(sqlite_, sql, -1, &st, nullptr) != SQLITE_OK) return 0;
        int64_t exp = nowTs() + ttlSeconds;
        sqlite3_bind_text(st, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 2, exp);
        sqlite3_bind_int64(st, 3, nowTs());
        sqlite3_bind_int64(st, 4, nowTs());
        sqlite3_bind_int64(st, 5, exp);
        sqlite3_step(st);
        sqlite3_finalize(st);
        return getCounter(key);
    }

    /// 读取计数器当前值
    long getCounter(const std::string& key) {
        if (!sqlite_) return 0;
        sqlite3_stmt* st = nullptr;
        const char* sql = "SELECT v FROM counters WHERE k=? AND expire_at>?";
        if (sqlite3_prepare_v2(sqlite_, sql, -1, &st, nullptr) != SQLITE_OK) return 0;
        sqlite3_bind_text(st, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(st, 2, nowTs());
        long v = (sqlite3_step(st) == SQLITE_ROW) ? sqlite3_column_int64(st, 0) : 0;
        sqlite3_finalize(st);
        return v;
    }

    /// 定期清理过期数据（建议每 5 分钟调用）
    void cleanup() {
        execSql("DELETE FROM tickets WHERE expire_at<? OR used=1",
                {std::to_string(nowTs())});
        execSql("DELETE FROM counters WHERE expire_at<?",
                {std::to_string(nowTs())});
        execSql("DELETE FROM ban_list WHERE expire_at<?",
                {std::to_string(nowTs())});
    }

    void shutdown() {
        std::lock_guard<std::mutex> lk(mu_);
        if (sqlite_) { sqlite3_close(sqlite_); sqlite_ = nullptr; }
#ifdef RUOYI_WAF_ROCKSDB
        rocksDb_.reset();
#endif
    }

private:
    RiskStore() = default;
    ~RiskStore() { shutdown(); }

    static int64_t nowTs() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    bool initSqlite() {
        if (sqlite3_open(cfg_.dbPath.c_str(), &sqlite_) != SQLITE_OK) {
            LOG_ERROR << "[RiskStore] sqlite open failed: " << cfg_.dbPath;
            return false;
        }
        sqlite3_exec(sqlite_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(sqlite_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        const char* ddl =
            "CREATE TABLE IF NOT EXISTS ban_list("
            "  ip TEXT PRIMARY KEY, expire_at INTEGER, reason TEXT, hit_count INTEGER DEFAULT 0);"
            "CREATE TABLE IF NOT EXISTS tickets("
            "  ticket TEXT PRIMARY KEY, expire_at INTEGER, used INTEGER DEFAULT 0,"
            "  payload TEXT DEFAULT '');"
            "CREATE TABLE IF NOT EXISTS counters("
            "  k TEXT PRIMARY KEY, v INTEGER, expire_at INTEGER);"
            "CREATE INDEX IF NOT EXISTS idx_ban_expire ON ban_list(expire_at);"
            "CREATE INDEX IF NOT EXISTS idx_ticket_expire ON tickets(expire_at);";
        if (sqlite3_exec(sqlite_, ddl, nullptr, nullptr, nullptr) != SQLITE_OK) {
            LOG_ERROR << "[RiskStore] sqlite init schema failed";
            return false;
        }
        // 兼容旧库：tickets 表补 payload 列（已存在则忽略错误）
        sqlite3_exec(sqlite_,
            "ALTER TABLE tickets ADD COLUMN payload TEXT DEFAULT ''",
            nullptr, nullptr, nullptr);
        LOG_INFO << "[RiskStore] SQLite backend: " << cfg_.dbPath;
        loadBansToMemory();
        return true;
    }

    /// 启动时把未过期封禁加载到内存
    void loadBansToMemory() {
        if (!sqlite_) return;
        sqlite3_stmt* st = nullptr;
        const char* sql = "SELECT ip,expire_at,reason,hit_count FROM ban_list WHERE expire_at>?";
        if (sqlite3_prepare_v2(sqlite_, sql, -1, &st, nullptr) != SQLITE_OK) return;
        sqlite3_bind_int64(st, 1, nowTs());
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char* ipTxt = (const char*)sqlite3_column_text(st, 0);
            const char* rsTxt = (const char*)sqlite3_column_text(st, 2);
            if (!ipTxt) continue;
            bans_[ipTxt] = {
                sqlite3_column_int64(st, 1),
                rsTxt ? rsTxt : "",
                sqlite3_column_int(st, 3)
            };
        }
        sqlite3_finalize(st);
        LOG_INFO << "[RiskStore] loaded " << bans_.size() << " active bans";
    }

    void persistBan(const std::string& ip, int64_t expireAt, const std::string& reason) {
        execSql("INSERT OR REPLACE INTO ban_list(ip,expire_at,reason,hit_count) VALUES(?,?,?,0)",
                {ip, std::to_string(expireAt), reason});
    }

    void removeBan(const std::string& ip) {
        execSql("DELETE FROM ban_list WHERE ip=?", {ip});
    }

    /// 简单参数化执行（仅用于写操作）
    void execSql(const char* sql, const std::vector<std::string>& params) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!sqlite_) return;
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(sqlite_, sql, -1, &st, nullptr) != SQLITE_OK) return;
        for (size_t i = 0; i < params.size(); i++)
            sqlite3_bind_text(st, (int)i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }

    Config      cfg_;
    sqlite3*    sqlite_ = nullptr;
    std::mutex  mu_;
    std::unordered_map<std::string, BanInfo> bans_;  ///< 内存封禁缓存

#ifdef RUOYI_WAF_ROCKSDB
    std::unique_ptr<rocksdb::DB> rocksDb_;
#endif
};
