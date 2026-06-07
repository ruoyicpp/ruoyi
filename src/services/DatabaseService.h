/**
 * @file DatabaseService.h
 * @brief 数据库服务层 — 统一的数据库访问接口
 * 
 * 功能概述：
 *   - 双驱动支持：支持 PostgreSQL 和 SQLite 两种数据库
 *   - 自动降级：PostgreSQL 不可用时自动切换到 SQLite
 *   - 连接池管理：支持连接池和单连接两种模式
 *   - 参数化查询：防止 SQL 注入，支持 $1、$2 等占位符
 *   - 事务支持：支持事务的开始、提交、回滚
 *   - 性能监控：集成性能指标收集
 * 
 * 核心特性：
 *   - 双驱动分发：根据编译选项选择 PostgreSQL 或 SQLite 驱动
 *   - libpq-lite 连接池：可选的轻量级连接池（运行时加载）
 *   - SQLite 加密：支持 SQLite3MultipleCiphers 和文件级加密
 *   - 自动故障转移：PG 故障时自动切换到 SQLite，恢复后自动同步
 * 
 * 编译选项：
 *   - RUOYI_USE_PG_DRIVER ON  → 使用自研 drogon_pg_driver（连接池 + 熔断）
 *   - RUOYI_USE_PG_DRIVER OFF → 使用官方 libpq（单连接或 libpq-lite 池）
 * 
 * 使用示例：
 *   // 查询
 *   auto res = DatabaseService::instance().queryParams(
 *       "SELECT * FROM sys_user WHERE user_id = $1", {userId});
 *   if (res.ok() && res.rows() > 0) {
 *       std::string name = res.str(0, 1);
 *   }
 *   
 *   // 执行
 *   DatabaseService::instance().execParams(
 *       "UPDATE sys_user SET status = $1 WHERE user_id = $2",
 *       {status, userId});
 * 
 * 配置项（config.json）：
 *   - database.type: "postgresql" 或 "sqlite"
 *   - database.host: PostgreSQL 主机
 *   - database.port: PostgreSQL 端口
 *   - database.dbname: 数据库名称
 *   - database.user: 用户名
 *   - database.passwd: 密码
 *   - database.sqlite_path: SQLite 文件路径
 */

#pragma once

// =============================================================
// 双驱动分发：
//   - RUOYI_USE_PG_DRIVER ON  → 走自研 drogon_pg_driver（连接池 + 熔断 + 独占事务）
//   - RUOYI_USE_PG_DRIVER OFF → 走官方 libpq 单连接 / libpq-lite pool（本文件下方实现）
// 控制器代码无需改动，永远 #include "DatabaseService.h"。
// =============================================================
#ifdef RUOYI_USE_PG_DRIVER
#  include "DatabaseServicePg.h"
#else

#include <string>
#include <vector>
#include <mutex>
#include <iostream>
#include <chrono>
#include <cctype>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <libpq-fe.h>
#include <sqlite3.h>
#include "common/SqliteCipher.h"
#include "common/SqliteFileCipher.h"

// 全局 DB 指标钩子（避免循环依赖：DatabaseService 不直接 include MetricsCollector）
// main.cc 启动时绑定到 MetricsCollector::onDbQuery
namespace DbMetricsHook {
    using Fn = std::function<void(long ms, bool ok, bool isWrite)>;
    inline Fn hook;
    inline void notify(long ms, bool ok, bool isWrite) {
        if (hook) {
            try { hook(ms, ok, isWrite); } catch (...) {}
        }
    }
}
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

// ── libpq-lite 运行时可选连接池（DLL 不存在时自动回退到官方 libpq）──
struct PqLiteDriver {
    void* hLib = nullptr;

    using fp_pool_create   = void*       (*)(const char*, int, int);
    using fp_pool_destroy  = void        (*)(void*);
    using fp_pool_acquire  = void*       (*)(void*, int);
    using fp_pool_release  = void        (*)(void*, void*);
    using fp_exec          = void*       (*)(void*, const char*);
    using fp_exec_params   = void*       (*)(void*, const char*, int, const char* const*);
    using fp_result_status = int         (*)(const void*);
    using fp_ntuples       = int         (*)(const void*);
    using fp_nfields       = int         (*)(const void*);
    using fp_getvalue      = const char* (*)(const void*, int, int);
    using fp_getisnull     = int         (*)(const void*, int, int);
    using fp_clear         = void        (*)(void*);

    fp_pool_create   pool_create  = nullptr;
    fp_pool_destroy  pool_destroy = nullptr;
    fp_pool_acquire  pool_acquire = nullptr;
    fp_pool_release  pool_release = nullptr;
    fp_exec          exec_fn      = nullptr;
    fp_exec_params   exec_params  = nullptr;
    fp_result_status res_status   = nullptr;
    fp_ntuples       ntuples      = nullptr;
    fp_nfields       nfields      = nullptr;
    fp_getvalue      getvalue     = nullptr;
    fp_getisnull     getisnull    = nullptr;
    fp_clear         clear_fn     = nullptr;

    bool loaded() const { return hLib != nullptr; }

    bool load() {
#ifdef _WIN32
        hLib = (void*)LoadLibraryA("libpqlite.dll");
#else
        hLib = dlopen("libpqlite.so", RTLD_LAZY | RTLD_LOCAL);
#endif
        if (!hLib) return false;
        auto sym = [&](const char* n) -> void* {
#ifdef _WIN32
            return (void*)GetProcAddress((HMODULE)hLib, n);
#else
            return dlsym(hLib, n);
#endif
        };
        pool_create  = (fp_pool_create)  sym("pqlite_pool_create");
        pool_destroy = (fp_pool_destroy) sym("pqlite_pool_destroy");
        pool_acquire = (fp_pool_acquire) sym("pqlite_pool_acquire");
        pool_release = (fp_pool_release) sym("pqlite_pool_release");
        exec_fn      = (fp_exec)         sym("pqlite_exec");
        exec_params  = (fp_exec_params)  sym("pqlite_exec_params");
        res_status   = (fp_result_status)sym("pqlite_result_status");
        ntuples      = (fp_ntuples)      sym("pqlite_ntuples");
        nfields      = (fp_nfields)      sym("pqlite_nfields");
        getvalue     = (fp_getvalue)     sym("pqlite_getvalue");
        getisnull    = (fp_getisnull)    sym("pqlite_getisnull");
        clear_fn     = (fp_clear)        sym("pqlite_clear");
        if (!pool_create || !pool_destroy || !pool_acquire || !pool_release ||
            !exec_fn || !exec_params || !res_status || !ntuples || !nfields ||
            !getvalue || !getisnull || !clear_fn) {
            unload(); return false;
        }
        return true;
    }

    void unload() {
        if (!hLib) return;
#ifdef _WIN32
        FreeLibrary((HMODULE)hLib);
#else
        dlclose(hLib);
#endif
        hLib = nullptr;
        pool_create = nullptr; pool_destroy = nullptr;
        pool_acquire = nullptr; pool_release = nullptr;
        exec_fn = nullptr; exec_params = nullptr;
        res_status = nullptr; ntuples = nullptr; nfields = nullptr;
        getvalue = nullptr; getisnull = nullptr; clear_fn = nullptr;
    }
};

// ============================================================
// DatabaseService：PostgreSQL 主库 + SQLite 自动回退
// 公开 API 与旧版完全兼容（QueryResult 接口不变）
// 新增：connectSqlite(), isUsingSqlite(), backendInfo()
// ============================================================
class DatabaseService {
public:
    // --------------------------------------------------------
    // 通用结果集（同时支持 PG 与 SQLite）
    // --------------------------------------------------------
    struct QueryResult {
        bool valid_ = false;
        std::vector<std::vector<std::string>> data_;
        std::vector<std::vector<bool>>        nulls_;
        int cols_ = 0;

        QueryResult() = default;

        bool ok()   const { return valid_; }
        int  rows() const { return (int)data_.size(); }
        int  cols() const { return cols_; }

        std::string str(int r, int c) const {
            if (r < 0 || r >= (int)data_.size()) return "";
            if (c < 0 || c >= (int)data_[r].size()) return "";
            return data_[r][c];
        }
        int  intVal(int r, int c)  const { auto s = str(r,c); return s.empty() ? 0  : std::atoi(s.c_str()); }
        long longVal(int r, int c) const { auto s = str(r,c); return s.empty() ? 0L : std::atol(s.c_str()); }
        bool boolVal(int r, int c) const {
            auto s = str(r,c);
            return s == "t" || s == "1" || s == "true" || s == "TRUE";
        }
        bool isNull(int r, int c) const {
            if (r < 0 || r >= (int)nulls_.size()) return true;
            if (c < 0 || c >= (int)nulls_[r].size()) return true;
            return nulls_[r][c];
        }
    };

    // --------------------------------------------------------
    // 事务 RAII
    // --------------------------------------------------------
    // ⚠️ 已知限制：在 libpq-lite 连接池模式下，每次 exec/query 从池中
    // 独立 acquire/release 连接，因此 BEGIN/INSERT/COMMIT 可能落在不同连接上，
    // 导致事务语义失效。仅在「单连接 conn_」模式或 SQLite 模式下可靠。
    // 当前代码并未直接使用 Transaction；多表写入依赖应用层的 ON CONFLICT
    // 与 del_flag/JOIN 兜底，少量不一致可由用户重新提交修复。
    // 后续如需可靠事务，应改造为：begin() 时 pin 一个连接，
    // 后续 exec/query 走该 pinned 连接，commit/rollback 时归还。
    class Transaction {
        DatabaseService& db_;
        bool committed_ = false;
    public:
        explicit Transaction(DatabaseService& db) : db_(db) { db_.begin(); }
        ~Transaction() { if (!committed_) db_.rollback(); }
        void commit() { db_.commit(); committed_ = true; }
    };

    // --------------------------------------------------------
    // 单例
    // --------------------------------------------------------
    static DatabaseService& instance() {
        static DatabaseService inst;
        return inst;
    }

    DatabaseService() = default;
    ~DatabaseService() {
        if (pgPool_) { pqLite_.pool_destroy(pgPool_); pgPool_ = nullptr; }
        pqLite_.unload();
        if (conn_) { PQfinish(conn_); conn_ = nullptr; }
        if (lite_) {
            // 文件级加密回写：先 WAL checkpoint，再加密落盘，最后清明文
            if (useFileCipher_ && !cipherKey_.empty()) {
                sqlite3_wal_checkpoint_v2(lite_, nullptr,
                                          SQLITE_CHECKPOINT_FULL, nullptr, nullptr);
            }
            sqlite3_close(lite_); lite_ = nullptr;
            if (useFileCipher_ && !cipherKey_.empty()) {
                const std::string encPath = sqlitePath_ + ".enc";
                if (SqliteFileCipher::encryptFile(sqlitePath_, encPath, cipherKey_)) {
                    std::error_code ec;
                    std::filesystem::remove(sqlitePath_, ec);                      // 删明文
                    std::filesystem::remove(sqlitePath_ + "-wal", ec);             // WAL
                    std::filesystem::remove(sqlitePath_ + "-shm", ec);             // SHM
                    std::cout << "[DB][FileCipher] 已加密回写 " << encPath << std::endl;
                } else {
                    std::cout << "[DB][FileCipher][ERR] 加密失败，保留明文以防数据丢失: "
                              << sqlitePath_ << std::endl;
                }
            }
        }
    }

    // --------------------------------------------------------
    // 连接
    // --------------------------------------------------------
    bool connect(const std::string& connStr) {
        connStr_ = connStr;
        // 已通过 pool 连接且不在 SQLite 模式，直接复用
        if (pgPool_ && !useSqlite_) {
            std::cout << "[DB] Already connected via libpq-lite pool, reusing." << std::endl;
            return true;
        }
        // 1. 先尝试加载 libpq-lite 连接池
        if (!pqLite_.loaded() && pqLite_.load()) {
            pgPool_ = pqLite_.pool_create(connStr.c_str(), 3, 20);
            if (pgPool_) {
                void* c = pqLite_.pool_acquire(pgPool_, 3000);
                if (c) {
                    pqLite_.pool_release(pgPool_, c);
                    useSqlite_ = false;
                    std::cout << "[DB] Connected via libpq-lite pool (3~20 connections)" << std::endl;
                    return true;
                }
                pqLite_.pool_destroy(pgPool_); pgPool_ = nullptr;
            }
            pqLite_.unload();
        }
        // 2. 回退到官方 libpq 单连接
        conn_ = PQconnectdb(connStr.c_str());
        if (PQstatus(conn_) != CONNECTION_OK) {
            std::cout << "[DB] PostgreSQL 连接失败: " << PQerrorMessage(conn_) << std::endl;
            PQfinish(conn_); conn_ = nullptr;
            return false;
        }
        PQsetClientEncoding(conn_, "UTF8");
        // 拦截 NOTICE/WARNING 通知，转为彩色 [DB] 标签输出
        // 用 std::cerr 而非 std::cout：stdout 可能被 freopen 重定向到 console.log
        PQsetNoticeProcessor(conn_, [](void*, const char* msg) {
            if (!msg || !*msg) return;
            std::string s(msg);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (!s.empty()) std::cerr << "[DB] " << s << std::endl;
        }, nullptr);
        useSqlite_ = false;
        std::cout << "[DB] Connected to PostgreSQL (official libpq)" << std::endl;
        return true;
    }

    bool connectSqlite(const std::string& path = "ruoyi.db") {
        sqlitePath_ = path;

        // ── 文件级加密回退路径（未编 sqlite3mc 时启用）──
        // 若已设置 cipherKey_ + path.enc 存在 → 先解密到明文 path
        // 仅在 HAVE_SQLCIPHER 未定义时启用（页级加密优先）
#ifndef HAVE_SQLCIPHER
        if (!cipherKey_.empty()) {
            const std::string encPath = path + ".enc";
            if (std::filesystem::exists(encPath)) {
                if (!SqliteFileCipher::decryptFile(encPath, path, cipherKey_)) {
                    std::cout << "[DB][FileCipher] 解密失败：密钥错误或文件损坏: "
                              << encPath << std::endl;
                    return false;
                }
                std::cout << "[DB][FileCipher] 已解密 " << encPath << " → " << path << std::endl;
            }
            useFileCipher_ = true;
        }
#endif

        if (sqlite3_open(path.c_str(), &lite_) != SQLITE_OK) {
            std::cout << "[DB] SQLite 打开失败: " << sqlite3_errmsg(lite_) << std::endl;
            lite_ = nullptr;
            return false;
        }
#ifdef HAVE_SQLCIPHER
        if (!cipherKey_.empty()) {
            if (sqlite3_key(lite_, cipherKey_.data(), (int)cipherKey_.size()) != SQLITE_OK) {
                std::cout << "[DB] sqlite3_key 应用失败: " << sqlite3_errmsg(lite_) << std::endl;
                sqlite3_close(lite_); lite_ = nullptr;
                return false;
            }
            // 验证密钥正确性（触发页解密，key 错会在此失败）
            char* zerr = nullptr;
            if (sqlite3_exec(lite_, "SELECT count(*) FROM sqlite_master;",
                             nullptr, nullptr, &zerr) != SQLITE_OK) {
                std::string msg = zerr ? zerr : "key 验证失败";
                if (zerr) sqlite3_free(zerr);
                std::cout << "[DB] SQLite 密钥验证失败: " << msg << std::endl;
                sqlite3_close(lite_); lite_ = nullptr;
                return false;
            }
            if (cipherCfg_.cipherPageSize > 0) {
                std::string ps = "PRAGMA cipher_page_size=" +
                                 std::to_string(cipherCfg_.cipherPageSize) + ";";
                sqlite3_exec(lite_, ps.c_str(), nullptr, nullptr, nullptr);
            }
            std::cout << "[DB] SQLite 加密连接已建立: " << path << std::endl;
        }
#endif
        sqlite3_exec(lite_, "PRAGMA journal_mode=WAL;",      nullptr, nullptr, nullptr);
        sqlite3_exec(lite_, "PRAGMA synchronous=NORMAL;",    nullptr, nullptr, nullptr);
        sqlite3_exec(lite_, "PRAGMA foreign_keys=ON;",       nullptr, nullptr, nullptr);
        sqlite3_busy_timeout(lite_, 5000);
        std::cout << "[DB] Connected to SQLite: " << path << std::endl;
        return true;
    }

    void setCipherKey(const std::string& key, const SqliteCipher::KeyConfig& cfg) {
        std::lock_guard<std::mutex> lock(mutex_);
        cipherKey_ = key;
        cipherCfg_ = cfg;
    }

    bool isConnected() const {
        if (useSqlite_) return lite_ != nullptr;
        if (pgPool_) return true;
        return conn_ && PQstatus(conn_) == CONNECTION_OK;
    }
    bool hasSqlite() const { return lite_ != nullptr; }
    bool isUsingSqlite() const { return useSqlite_; }
    PGconn* raw() { return conn_; }

    bool reconnect() {
        if (pgPool_) {
            void* c = pqLite_.pool_acquire(pgPool_, 3000);
            if (c) { pqLite_.pool_release(pgPool_, c); return true; }
            pqLite_.pool_destroy(pgPool_); pgPool_ = nullptr;
            pqLite_.unload();
        }
        if (conn_) { PQfinish(conn_); conn_ = nullptr; }
        return connect(connStr_);
    }
    bool ensureConnection() {
        if (isConnected()) return true;
        if (!useSqlite_) return reconnect();
        return lite_ != nullptr;
    }

    std::string backendInfo() const {
        if (!useSqlite_) return pgPool_ ? "postgresql(libpq-lite pool)" : "postgresql";
        return "sqlite(" + sqlitePath_ + ")";
    }
    size_t pendingCount() const {
        std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(mutex_));
        return pendingSync_.size();
    }

    // manually activate SQLite fallback when PG startup fails
    void activateSqliteFallback() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (lite_) {
            useSqlite_ = true;
            pgRetryAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(60);
            std::cout << "[DB] SQLite fallback activated" << std::endl;
        }
    }

    // directly execute SQL on SQLite (bypass routing, for schema init)
    bool execSqliteDirect(const std::string& sql) {
        std::lock_guard<std::mutex> lock(mutex_);
        return execSqliteLocked(sql, {});
    }

    // directly execute SQL on SQLite with parameters (for safe migration)
    bool execSqliteDirect(const std::string& sql,
                          const std::vector<std::string>& params) {
        std::lock_guard<std::mutex> lock(mutex_);
        return execSqliteLocked(sql, params);
    }

    // directly query SQLite with params (bypass routing, for migration)
    QueryResult querySqliteDirect(const std::string& sql, const std::vector<std::string>& params = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        return querySqliteLocked(sql, params);
    }

    // --------------------------------------------------------
    // exec：DML / DDL（无返回行）
    //   PG 可用时：写 PG；若为 DML 则同时写 SQLite（双写）
    //   PG 不可用：写 SQLite，并将 DML 存入 pendingSync_
    // --------------------------------------------------------
    bool exec(const std::string& sql) {
        auto t0 = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        ensurePgOrFallbackLocked();

        bool ret;
        if (!useSqlite_) {
            bool ok = execPgLocked(sql);
            if (!ok && (!conn_ || PQstatus(conn_) != CONNECTION_OK)) {
                switchToSqliteLocked();
                if (isDml(sql)) {
                    pendingSyncIfRoom({sql, {}});
                    ret = execSqliteLocked(toSqlite(sql), {});
                } else ret = false;
            } else {
                if (ok && isDml(sql) && lite_) execSqliteLocked(toSqlite(sql), {});
                ret = ok;
            }
        } else {
            tryRecoverPgLocked();
            if (isDml(sql)) pendingSyncIfRoom({sql, {}});
            ret = execSqliteLocked(toSqlite(sql), {});
        }
        long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        logSlow("exec", sql, ms);
        DbMetricsHook::notify(ms, ret, true);
        return ret;
    }

    bool execParams(const std::string& sql, const std::vector<std::string>& params) {
        auto t0 = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        ensurePgOrFallbackLocked();

        bool ret;
        if (!useSqlite_) {
            bool ok = execParamsPgLocked(sql, params);
            if (!ok && (!conn_ || PQstatus(conn_) != CONNECTION_OK)) {
                switchToSqliteLocked();
                pendingSyncIfRoom({sql, params});
                ret = execSqliteLocked(toSqlite(sql), params);
            } else {
                if (ok && isDml(sql) && lite_) execSqliteLocked(toSqlite(sql), params);
                ret = ok;
            }
        } else {
            tryRecoverPgLocked();
            pendingSyncIfRoom({sql, params});
            ret = execSqliteLocked(toSqlite(sql), params);
        }
        long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        logSlow("execParams", sql, ms);
        DbMetricsHook::notify(ms, ret, true);
        return ret;
    }

    // --------------------------------------------------------
    // query：SELECT（返回行集）
    //   优先用 PG；PG 失败时自动切换到 SQLite
    // --------------------------------------------------------
    QueryResult query(const std::string& sql) {
        auto t0 = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        ensurePgOrFallbackLocked();

        QueryResult ret;
        if (!useSqlite_) {
            auto r = queryPgLocked(sql);
            if (!r.ok() && (!conn_ || PQstatus(conn_) != CONNECTION_OK)) {
                switchToSqliteLocked();
                ret = querySqliteLocked(toSqlite(sql), {});
            } else ret = std::move(r);
        } else {
            tryRecoverPgLocked();
            ret = querySqliteLocked(toSqlite(sql), {});
        }
        long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        logSlow("query", sql, ms);
        DbMetricsHook::notify(ms, ret.ok(), false);
        return ret;
    }

    QueryResult queryParams(const std::string& sql, const std::vector<std::string>& params) {
        auto t0 = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        ensurePgOrFallbackLocked();

        QueryResult ret;
        if (!useSqlite_) {
            auto r = queryParamsPgLocked(sql, params);
            if (!r.ok() && (!conn_ || PQstatus(conn_) != CONNECTION_OK)) {
                switchToSqliteLocked();
                ret = querySqliteLocked(toSqlite(sql), params);
            } else ret = std::move(r);
        } else {
            tryRecoverPgLocked();
            ret = querySqliteLocked(toSqlite(sql), params);
        }
        long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        logSlow("queryParams", sql, ms);
        DbMetricsHook::notify(ms, ret.ok(), false);
        return ret;
    }

    bool begin()    { return exec("BEGIN"); }
    bool commit()   { return exec("COMMIT"); }
    bool rollback() { return exec("ROLLBACK"); }

private:
    // --------------------------------------------------------
    // 状态
    // --------------------------------------------------------
    PqLiteDriver pqLite_;
    void*        pgPool_    = nullptr;
    std::string connStr_;
    PGconn*     conn_       = nullptr;
    sqlite3*    lite_       = nullptr;
    std::string sqlitePath_ = "ruoyi.db";
    std::string cipherKey_;                       // 加密主密钥（为空则未加密）
    SqliteCipher::KeyConfig cipherCfg_;           // 加密参数（pageSize / kdfIter）
    bool        useFileCipher_ = false;           // 文件级 AES 加密回退（无 sqlite3mc 时启用）
    std::mutex  mutex_;
    bool        useSqlite_  = false;
    std::chrono::steady_clock::time_point pgRetryAt_{};

    // 慢查询阈值 (ms)，启动时可由 config.json 调整
    int slowQueryWarnMs_ = 200;
    int slowQueryErrMs_  = 1000;
public:
    void setSlowQueryThreshold(int warnMs, int errMs) {
        slowQueryWarnMs_ = warnMs > 0 ? warnMs : 200;
        slowQueryErrMs_  = errMs  > 0 ? errMs  : 1000;
    }
private:
    void logSlow(const char* op, const std::string& sql, long ms) const {
        if (ms < slowQueryWarnMs_) return;
        std::string snippet = sql.size() > 200 ? sql.substr(0, 200) + "..." : sql;
        if (ms >= slowQueryErrMs_) {
            std::cout << "[SlowSQL][ERR][" << op << "] " << ms << "ms SQL: " << snippet << std::endl;
        } else {
            std::cout << "[SlowSQL][WARN][" << op << "] " << ms << "ms SQL: " << snippet << std::endl;
        }
    }

    struct PendingWrite { std::string sql; std::vector<std::string> params; };
    std::vector<PendingWrite> pendingSync_;
    static constexpr size_t MAX_PENDING = 10000;

    // --------------------------------------------------------
    // SQL 工具
    // --------------------------------------------------------
    static bool isDml(const std::string& sql) {
        size_t i = 0;
        while (i < sql.size() && std::isspace((unsigned char)sql[i])) ++i;
        char buf[8] = {};
        for (int k = 0; k < 7 && i < sql.size() && !std::isspace((unsigned char)sql[i]); ++k, ++i)
            buf[k] = (char)std::toupper((unsigned char)sql[i]);
        return std::strcmp(buf,"INSERT")==0 || std::strcmp(buf,"UPDATE")==0 || std::strcmp(buf,"DELETE")==0;
    }

    // 将 PG 风格 SQL 翻译为 SQLite 风格
    static std::string toSqlite(const std::string& in) {
        std::string s;
        s.reserve(in.size());

    // 1. $N → ?（跳过单引号字符串字面量，避免 '$2a$%' 中的 $2 被误替换）
        bool inStr = false;
        for (size_t i = 0; i < in.size(); ) {
            char c = in[i];
            if (c == '\'' && !inStr) {
                inStr = true; s += c; ++i;
            } else if (c == '\'' && inStr) {
                // 处理转义引号 ''
                if (i+1 < in.size() && in[i+1] == '\'') { s += "''"; i += 2; }
                else { inStr = false; s += c; ++i; }
            } else if (!inStr && c == '$' && i+1 < in.size() && std::isdigit((unsigned char)in[i+1])) {
                s += '?'; ++i;
                while (i < in.size() && std::isdigit((unsigned char)in[i])) ++i;
            } else {
                s += c; ++i;
            }
        }

    // 2. NOW() → datetime('now')
        s = replaceCI(s, "NOW()", "datetime('now')", false);
    // 3. TRUE / FALSE → 1 / 0
        s = replaceCI(s, "TRUE",  "1",  true);
        s = replaceCI(s, "FALSE", "0",  true);
        // 4. strip RETURNING clause
        {
            size_t pos = findCI(s, "RETURNING");
            if (pos != std::string::npos) {
                bool prevSpace = (pos == 0 || std::isspace((unsigned char)s[pos-1]));
                size_t after = pos + 9;
                bool nextSpace = (after < s.size() && std::isspace((unsigned char)s[after]));
                if (prevSpace && nextSpace) {
                    size_t end = s.find(';', pos);
                    s = s.substr(0, pos) + (end != std::string::npos ? s.substr(end) : "");
                }
            }
        }
        return s;
    }

    static size_t findCI(const std::string& s, const char* word) {
        size_t wlen = std::strlen(word);
        for (size_t i = 0; i + wlen <= s.size(); ++i) {
            bool match = true;
            for (size_t j = 0; j < wlen; ++j) {
                if (std::tolower((unsigned char)s[i+j]) != std::tolower((unsigned char)word[j])) { match=false; break; }
            }
            if (match) return i;
        }
        return std::string::npos;
    }

    // wordBound=true: replace whole words only
    static std::string replaceCI(const std::string& s, const char* from, const char* to, bool wordBound) {
        std::string result;
        result.reserve(s.size());
        size_t flen = std::strlen(from);
        size_t i = 0;
        while (i < s.size()) {
            if (i + flen <= s.size()) {
                bool match = true;
                for (size_t j = 0; j < flen; ++j) {
                    if (std::tolower((unsigned char)s[i+j]) != std::tolower((unsigned char)from[j])) { match=false; break; }
                }
                if (match) {
                    bool ok = true;
                    if (wordBound) {
                        bool lb = (i == 0 || !std::isalnum((unsigned char)s[i-1]) && s[i-1] != '_');
                        size_t after = i + flen;
                        bool rb = (after >= s.size() || !std::isalnum((unsigned char)s[after]) && s[after] != '_');
                        ok = lb && rb;
                    }
                    if (ok) { result += to; i += flen; continue; }
                }
            }
            result += s[i++];
        }
        return result;
    }

    void pendingSyncIfRoom(PendingWrite pw) {
        if (isDml(pw.sql) && pendingSync_.size() < MAX_PENDING)
            pendingSync_.push_back(std::move(pw));
    }

    // --------------------------------------------------------
    // 内部状态管理（mutex 已持有）
    // --------------------------------------------------------
    void ensurePgOrFallbackLocked() {
        if (useSqlite_) return;
        // pool 模式：快速探活
        if (pgPool_) {
            void* c = pqLite_.pool_acquire(pgPool_, 500);
            if (c) { pqLite_.pool_release(pgPool_, c); return; }
            // pool 耗尽或宕机，销毁后回退
            pqLite_.pool_destroy(pgPool_); pgPool_ = nullptr;
            pqLite_.unload();
        }
        if (conn_ && PQstatus(conn_) == CONNECTION_OK) return;
        if (conn_) { PQfinish(conn_); conn_ = nullptr; }
        auto now = std::chrono::steady_clock::now();
        if (!connStr_.empty() && now >= pgRetryAt_) {
            pgRetryAt_ = now + std::chrono::seconds(5);
            conn_ = PQconnectdb(connStr_.c_str());
            if (PQstatus(conn_) == CONNECTION_OK) {
                PQsetClientEncoding(conn_, "UTF8");
                return;
            }
            PQfinish(conn_); conn_ = nullptr;
        }
        if (lite_) switchToSqliteLocked();
    }

    void switchToSqliteLocked() {
        if (useSqlite_) return;
        useSqlite_ = true;
        pgRetryAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        std::cout << "[DB] PostgreSQL unavailable, switching to SQLite fallback" << std::endl;
    }

    void tryRecoverPgLocked() {
        if (!useSqlite_ || connStr_.empty()) return;
        auto now = std::chrono::steady_clock::now();
        if (now < pgRetryAt_) return;
        pgRetryAt_ = now + std::chrono::seconds(5);

        // 先尝试 libpq-lite 连接池
        if (!pqLite_.loaded() && pqLite_.load()) {
            pgPool_ = pqLite_.pool_create(connStr_.c_str(), 3, 20);
            if (pgPool_) {
                void* c = pqLite_.pool_acquire(pgPool_, 3000);
                if (c) {
                    pqLite_.pool_release(pgPool_, c);
                    useSqlite_ = false;
                    std::cout << "[DB] PostgreSQL recovered via libpq-lite pool, replaying "
                              << pendingSync_.size() << " pending writes..." << std::endl;
                    replaySyncLocked();
                    return;
                }
                pqLite_.pool_destroy(pgPool_); pgPool_ = nullptr;
            }
            pqLite_.unload();
        }
        // 回退官方 libpq
        if (conn_) { PQfinish(conn_); conn_ = nullptr; }
        conn_ = PQconnectdb(connStr_.c_str());
        if (PQstatus(conn_) == CONNECTION_OK) {
            PQsetClientEncoding(conn_, "UTF8");
            useSqlite_ = false;
            std::cout << "[DB] PostgreSQL recovered (official libpq), replaying "
                      << pendingSync_.size() << " pending writes..." << std::endl;
            replaySyncLocked();
        } else {
            PQfinish(conn_); conn_ = nullptr;
        }
    }

    void replaySyncLocked() {
        int ok = 0, fail = 0;
        for (auto& pw : pendingSync_) {
            bool r = pw.params.empty() ? execPgLocked(pw.sql) : execParamsPgLocked(pw.sql, pw.params);
            r ? ++ok : ++fail;
        }
        pendingSync_.clear();
        std::cout << "[DB] 离线写入回写完成: 成功=" << ok << " 失败=" << fail << std::endl;
    }

    // --------------------------------------------------------
    // PG 执行（mutex 已持有）
    // --------------------------------------------------------
    bool execPgLocked(const std::string& sql) {
        if (pgPool_) {
            void* c = pqLite_.pool_acquire(pgPool_, 5000);
            if (!c) return false;
            void* res = pqLite_.exec_fn(c, sql.c_str());
            int st = res ? pqLite_.res_status(res) : 3;
            bool ok = (st == 1 || st == 2);
            if (!ok) std::cout << "[DB] exec error (pool)" << std::endl;
            if (res) pqLite_.clear_fn(res);
            pqLite_.pool_release(pgPool_, c);
            return ok;
        }
        if (!conn_) return false;
        PGresult* res = PQexec(conn_, sql.c_str());
        bool ok = (PQresultStatus(res)==PGRES_COMMAND_OK || PQresultStatus(res)==PGRES_TUPLES_OK);
        if (!ok) std::cout << "[DB] exec error: " << PQerrorMessage(conn_) << std::endl;
        PQclear(res);
        return ok;
    }

    bool execParamsPgLocked(const std::string& sql, const std::vector<std::string>& params) {
        std::vector<const char*> pv;
        for (auto& p : params) pv.push_back(p.c_str());
        if (pgPool_) {
            void* c = pqLite_.pool_acquire(pgPool_, 5000);
            if (!c) return false;
            void* res = pqLite_.exec_params(c, sql.c_str(), (int)pv.size(), pv.data());
            int st = res ? pqLite_.res_status(res) : 3;
            bool ok = (st == 1 || st == 2);
            if (!ok) std::cout << "[DB] execParams error (pool) SQL: " << sql << std::endl;
            if (res) pqLite_.clear_fn(res);
            pqLite_.pool_release(pgPool_, c);
            return ok;
        }
        if (!conn_) return false;
        PGresult* res = PQexecParams(conn_, sql.c_str(), (int)pv.size(), nullptr, pv.data(), nullptr, nullptr, 0);
        bool ok = (PQresultStatus(res)==PGRES_COMMAND_OK || PQresultStatus(res)==PGRES_TUPLES_OK);
        if (!ok) std::cout << "[DB] execParams error: " << PQerrorMessage(conn_) << " SQL: " << sql << std::endl;
        PQclear(res);
        return ok;
    }

    QueryResult queryPgLocked(const std::string& sql) {
        if (pgPool_) {
            void* c = pqLite_.pool_acquire(pgPool_, 5000);
            if (!c) return {};
            void* res = pqLite_.exec_fn(c, sql.c_str());
            pqLite_.pool_release(pgPool_, c);
            if (!res || pqLite_.res_status(res) != 2) {
                std::cout << "[DB] query error (pool)" << std::endl;
                if (res) pqLite_.clear_fn(res);
                return {};
            }
            QueryResult qr = pqLiteToResult(res);
            pqLite_.clear_fn(res);
            return qr;
        }
        if (!conn_) return {};
        PGresult* res = PQexec(conn_, sql.c_str());
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::cout << "[DB] query error: " << PQerrorMessage(conn_) << std::endl;
            PQclear(res); return {};
        }
        QueryResult qr = pgToResult(res);
        PQclear(res);
        return qr;
    }

    QueryResult queryParamsPgLocked(const std::string& sql, const std::vector<std::string>& params) {
        std::vector<const char*> pv;
        for (auto& p : params) pv.push_back(p.c_str());
        if (pgPool_) {
            void* c = pqLite_.pool_acquire(pgPool_, 5000);
            if (!c) return {};
            void* res = pqLite_.exec_params(c, sql.c_str(), (int)pv.size(), pv.data());
            pqLite_.pool_release(pgPool_, c);
            if (!res || pqLite_.res_status(res) != 2) {
                std::cout << "[DB] queryParams error (pool) SQL: " << sql << std::endl;
                if (res) pqLite_.clear_fn(res);
                return {};
            }
            QueryResult qr = pqLiteToResult(res);
            pqLite_.clear_fn(res);
            return qr;
        }
        if (!conn_) return {};
        PGresult* res = PQexecParams(conn_, sql.c_str(), (int)pv.size(), nullptr, pv.data(), nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::cout << "[DB] queryParams error: " << PQerrorMessage(conn_) << " SQL: " << sql << std::endl;
            PQclear(res); return {};
        }
        QueryResult qr = pgToResult(res);
        PQclear(res);
        return qr;
    }

    QueryResult pqLiteToResult(void* res) {
        QueryResult qr;
        qr.valid_ = true;
        int nr = pqLite_.ntuples(res), nc = pqLite_.nfields(res);
        qr.cols_ = nc;
        for (int r = 0; r < nr; ++r) {
            std::vector<std::string> row;
            std::vector<bool>        nrow;
            for (int c = 0; c < nc; ++c) {
                bool n = pqLite_.getisnull(res, r, c) != 0;
                nrow.push_back(n);
                const char* v = n ? nullptr : pqLite_.getvalue(res, r, c);
                row.push_back(v ? v : "");
            }
            qr.data_.push_back(std::move(row));
            qr.nulls_.push_back(std::move(nrow));
        }
        return qr;
    }

    static QueryResult pgToResult(PGresult* res) {
        QueryResult qr;
        qr.valid_ = true;
        int nr = PQntuples(res), nc = PQnfields(res);
        qr.cols_ = nc;
        for (int r = 0; r < nr; ++r) {
            std::vector<std::string> row;
            std::vector<bool>        nrow;
            for (int c = 0; c < nc; ++c) {
                bool n = PQgetisnull(res, r, c);
                nrow.push_back(n);
                row.push_back(n ? "" : (PQgetvalue(res,r,c) ? PQgetvalue(res,r,c) : ""));
            }
            qr.data_.push_back(std::move(row));
            qr.nulls_.push_back(std::move(nrow));
        }
        return qr;
    }

    // --------------------------------------------------------
    // SQLite 执行（mutex 已持有）
    // --------------------------------------------------------
    // 致命 SQLite 错误（I/O 损坏等）：关闭连接，后续自动跳过
    void closeSqliteOnFatal(int rc, const char* ctx) {
        int base = rc & 0xFF;
        if (base == SQLITE_IOERR || base == SQLITE_CORRUPT || base == SQLITE_NOTADB) {
            std::cout << "[DB][SQLite] Fatal error (" << ctx << "): "
                      << sqlite3_errmsg(lite_) << " — SQLite disabled until restart" << std::endl;
            sqlite3_close(lite_);
            lite_ = nullptr;
        }
    }

    bool execSqliteLocked(const std::string& sql, const std::vector<std::string>& params) {
        if (!lite_) return false;
        sqlite3_stmt* stmt = nullptr;
        int prc = sqlite3_prepare_v2(lite_, sql.c_str(), -1, &stmt, nullptr);
        if (prc != SQLITE_OK) {
            closeSqliteOnFatal(prc, "prepare");
            return false;
        }
        for (int i = 0; i < (int)params.size(); ++i)
            sqlite3_bind_text(stmt, i+1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc == SQLITE_DONE || rc == SQLITE_ROW) return true;
        closeSqliteOnFatal(rc, "exec");
        if (lite_) // 非致命错误才打详细日志
            std::cout << "[DB][SQLite] exec error: " << sqlite3_errmsg(lite_) << std::endl;
        return false;
    }

    QueryResult querySqliteLocked(const std::string& sql, const std::vector<std::string>& params) {
        QueryResult qr;
        if (!lite_) return qr;
        sqlite3_stmt* stmt = nullptr;
        int prc = sqlite3_prepare_v2(lite_, sql.c_str(), -1, &stmt, nullptr);
        if (prc != SQLITE_OK) {
            closeSqliteOnFatal(prc, "query-prepare");
            return qr;
        }
        for (int i = 0; i < (int)params.size(); ++i)
            sqlite3_bind_text(stmt, i+1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        qr.valid_ = true;
        int nc = sqlite3_column_count(stmt);
        qr.cols_ = nc;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::vector<std::string> row;
            std::vector<bool>        nrow;
            for (int c = 0; c < nc; ++c) {
                bool isNull = (sqlite3_column_type(stmt, c) == SQLITE_NULL);
                nrow.push_back(isNull);
                const char* v = isNull ? nullptr : (const char*)sqlite3_column_text(stmt, c);
                row.push_back(v ? v : "");
            }
            qr.data_.push_back(std::move(row));
            qr.nulls_.push_back(std::move(nrow));
        }
        sqlite3_finalize(stmt);
        return qr;
    }
};

#endif // RUOYI_USE_PG_DRIVER
