#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <iostream>
#include <chrono>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <libpq-fe.h>
#include <sqlite3.h>
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
        if (lite_) { sqlite3_close(lite_); lite_ = nullptr; }
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
        useSqlite_ = false;
        std::cout << "[DB] Connected to PostgreSQL (official libpq)" << std::endl;
        return true;
    }

    bool connectSqlite(const std::string& path = "ruoyi.db") {
        sqlitePath_ = path;
        if (sqlite3_open(path.c_str(), &lite_) != SQLITE_OK) {
            std::cout << "[DB] SQLite 打开失败: " << sqlite3_errmsg(lite_) << std::endl;
            lite_ = nullptr;
            return false;
        }
        sqlite3_exec(lite_, "PRAGMA journal_mode=WAL;",      nullptr, nullptr, nullptr);
        sqlite3_exec(lite_, "PRAGMA synchronous=NORMAL;",    nullptr, nullptr, nullptr);
        sqlite3_exec(lite_, "PRAGMA foreign_keys=ON;",       nullptr, nullptr, nullptr);
        sqlite3_busy_timeout(lite_, 5000);
        std::cout << "[DB] Connected to SQLite: " << path << std::endl;
        return true;
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
        std::lock_guard<std::mutex> lock(mutex_);
        ensurePgOrFallbackLocked();

        if (!useSqlite_) {
            bool ok = execPgLocked(sql);
            if (!ok && (!conn_ || PQstatus(conn_) != CONNECTION_OK)) {
                switchToSqliteLocked();
                if (isDml(sql)) {
                    pendingSyncIfRoom({sql, {}});
                    return execSqliteLocked(toSqlite(sql), {});
                }
                return false;
            }
            if (ok && isDml(sql) && lite_)
                execSqliteLocked(toSqlite(sql), {});
            return ok;
        } else {
            tryRecoverPgLocked();
            if (isDml(sql)) pendingSyncIfRoom({sql, {}});
            return execSqliteLocked(toSqlite(sql), {});
        }
    }

    bool execParams(const std::string& sql, const std::vector<std::string>& params) {
        std::lock_guard<std::mutex> lock(mutex_);
        ensurePgOrFallbackLocked();

        if (!useSqlite_) {
            bool ok = execParamsPgLocked(sql, params);
            if (!ok && (!conn_ || PQstatus(conn_) != CONNECTION_OK)) {
                switchToSqliteLocked();
                pendingSyncIfRoom({sql, params});
                return execSqliteLocked(toSqlite(sql), params);
            }
            if (ok && isDml(sql) && lite_)
                execSqliteLocked(toSqlite(sql), params);
            return ok;
        } else {
            tryRecoverPgLocked();
            pendingSyncIfRoom({sql, params});
            return execSqliteLocked(toSqlite(sql), params);
        }
    }

    // --------------------------------------------------------
    // query：SELECT（返回行集）
    //   优先用 PG；PG 失败时自动切换到 SQLite
    // --------------------------------------------------------
    QueryResult query(const std::string& sql) {
        std::lock_guard<std::mutex> lock(mutex_);
        ensurePgOrFallbackLocked();

        if (!useSqlite_) {
            auto r = queryPgLocked(sql);
            if (!r.ok() && (!conn_ || PQstatus(conn_) != CONNECTION_OK)) {
                switchToSqliteLocked();
                return querySqliteLocked(toSqlite(sql), {});
            }
            return r;
        } else {
            tryRecoverPgLocked();
            return querySqliteLocked(toSqlite(sql), {});
        }
    }

    QueryResult queryParams(const std::string& sql, const std::vector<std::string>& params) {
        std::lock_guard<std::mutex> lock(mutex_);
        ensurePgOrFallbackLocked();

        if (!useSqlite_) {
            auto r = queryParamsPgLocked(sql, params);
            if (!r.ok() && (!conn_ || PQstatus(conn_) != CONNECTION_OK)) {
                switchToSqliteLocked();
                return querySqliteLocked(toSqlite(sql), params);
            }
            return r;
        } else {
            tryRecoverPgLocked();
            return querySqliteLocked(toSqlite(sql), params);
        }
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
    std::mutex  mutex_;
    bool        useSqlite_  = false;
    std::chrono::steady_clock::time_point pgRetryAt_{};

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
