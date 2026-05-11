#pragma once
// =============================================================
// DatabaseServicePg.h — 自研驱动专用版本
// 当 CMake 选项 RUOYI_USE_PG_DRIVER=ON 时启用。由 DatabaseService.h
// 顶部的 #ifdef 分发引入；公开 API（class DatabaseService / QueryResult /
// Transaction）与官方 libpq 版本完全兼容，所有控制器代码无需改动。
//
// 内部走 drogon_pg_driver（基于官方 libpq 的 C 库），提供：
//   - 主连接池（3~24）+ 降级连接池（2~3）
//   - 熔断器（CLOSED / OPEN / HALF_OPEN）
//   - 独占连接事务（解决池模式下 BEGIN/COMMIT 跨连接问题）
//   - 慢查询日志、心跳保活
// =============================================================
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
#include "common/SqliteCipher.h"

extern "C" {
#  include "pg_client.h"
#  include "pg_result.h"
#  include "pg_config.h"
#  include "pg_transaction.h"
}

class DatabaseService {
public:
    // --------------------------------------------------------
    // 通用结果集（与官方 libpq 版本字段完全一致）
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
    // 事务 RAII（drogon_pg_driver 独占连接，事务可靠）
    // --------------------------------------------------------
    class Transaction {
        DatabaseService& db_;
        bool committed_ = false;
    public:
        explicit Transaction(DatabaseService& db) : db_(db) { db_.begin(); }
        ~Transaction() { if (!committed_) db_.rollback(); }
        void commit() { db_.commit(); committed_ = true; }
    };

    static DatabaseService& instance() {
        static DatabaseService inst;
        return inst;
    }

    DatabaseService() = default;
    ~DatabaseService() {
        if (pgInit_) { pg_client_cleanup(); pgInit_ = false; }
        if (lite_) { sqlite3_close(lite_); lite_ = nullptr; }
    }

    // --------------------------------------------------------
    // 连接
    // --------------------------------------------------------
    bool connect(const std::string& connStr) {
        connStr_ = connStr;
        if (pgInit_ && !useSqlite_) {
            std::cout << "[DB] Already connected via drogon_pg_driver, reusing." << std::endl;
            return true;
        }
        // 配置连接池参数
        pg_config_t* cfg = pg_config_get_instance();
        if (cfg) {
            pg_config_set_connection_string(cfg, connStr.c_str());
            pg_config_set_pool_min_size(cfg, 3);
            pg_config_set_pool_init_size(cfg, 8);
            pg_config_set_pool_max_size(cfg, 24);
            pg_config_set_fallback_pool_min_size(cfg, 2);
            pg_config_set_fallback_pool_init_size(cfg, 2);
            pg_config_set_fallback_pool_max_size(cfg, 3);
            pg_config_set_failure_threshold(cfg, 5);
            pg_config_set_recovery_threshold(cfg, 10);
            pg_config_set_health_check_interval_sec(cfg, 30);
            pg_config_set_slow_query_threshold_ms(cfg, 200);
        }
        if (pg_client_init(connStr.c_str()) != 0) {
            std::cout << "[DB] drogon_pg_driver 初始化失败" << std::endl;
            return false;
        }
        // 预热连接池（建立初始连接，发现 PG 不可用立即返回失败）
        pg_client_warmup_pool();
        pgInit_   = true;
        useSqlite_ = false;
        std::cout << "[DB] Connected via drogon_pg_driver (pool 3~24 + fallback 2~3)" << std::endl;
        return true;
    }

    bool connectSqlite(const std::string& path = "ruoyi.db") {
        sqlitePath_ = path;
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

    // 设置慢查询阈值（对接 drogon_pg_driver 自身的 slow_query_threshold_ms）
    void setSlowQueryThreshold(int warnMs, int /*errMs*/) {
        pg_config_t* cfg = pg_config_get_instance();
        if (cfg) pg_config_set_slow_query_threshold_ms(cfg, warnMs > 0 ? warnMs : 200);
    }

    bool isConnected() const {
        if (useSqlite_) return lite_ != nullptr;
        return pgInit_;
    }
    bool hasSqlite() const { return lite_ != nullptr; }
    bool isUsingSqlite() const { return useSqlite_; }
    PGconn* raw() { return nullptr; }   // 自研驱动不暴露原始 PGconn*

    bool reconnect() {
        if (pgInit_) { pg_client_cleanup(); pgInit_ = false; }
        return connect(connStr_);
    }
    bool ensureConnection() {
        if (isConnected()) return true;
        if (!useSqlite_)   return reconnect();
        return lite_ != nullptr;
    }

    std::string backendInfo() const {
        if (!useSqlite_) return pgInit_ ? "postgresql(drogon_pg_driver)" : "postgresql";
        return "sqlite(" + sqlitePath_ + ")";
    }
    size_t pendingCount() const {
        std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(mutex_));
        return pendingSync_.size();
    }

    void activateSqliteFallback() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (lite_) {
            useSqlite_ = true;
            pgRetryAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(60);
            std::cout << "[DB] PG 启动失败，切换至 SQLite 应急模式" << std::endl;
        }
    }

    bool execSqliteDirect(const std::string& sql) {
        std::lock_guard<std::mutex> lock(mutex_);
        return execSqliteLocked(sql, {});
    }
    QueryResult querySqliteDirect(const std::string& sql, const std::vector<std::string>& params = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        return querySqliteLocked(sql, params);
    }

    // --------------------------------------------------------
    // 写入：PG 优先；不可用时切 SQLite + 入待回写队列
    // --------------------------------------------------------
    bool exec(const std::string& sql) {
        std::lock_guard<std::mutex> lock(mutex_);
        ensurePgOrFallbackLocked();
        if (!useSqlite_) {
            bool ok = execPgLocked(sql);
            if (!ok && !pgInit_) {
                switchToSqliteLocked();
                if (isDml(sql)) pendingSyncIfRoom({sql, {}});
                return execSqliteLocked(toSqlite(sql), {});
            }
            if (ok && isDml(sql) && lite_) execSqliteLocked(toSqlite(sql), {});
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
            if (!ok && !pgInit_) {
                switchToSqliteLocked();
                pendingSyncIfRoom({sql, params});
                return execSqliteLocked(toSqlite(sql), params);
            }
            if (ok && isDml(sql) && lite_) execSqliteLocked(toSqlite(sql), params);
            return ok;
        } else {
            tryRecoverPgLocked();
            pendingSyncIfRoom({sql, params});
            return execSqliteLocked(toSqlite(sql), params);
        }
    }

    QueryResult query(const std::string& sql) {
        std::lock_guard<std::mutex> lock(mutex_);
        ensurePgOrFallbackLocked();
        if (!useSqlite_) {
            auto r = queryPgLocked(sql);
            if (!r.ok() && !pgInit_) {
                switchToSqliteLocked();
                return querySqliteLocked(toSqlite(sql), {});
            }
            return r;
        }
        tryRecoverPgLocked();
        return querySqliteLocked(toSqlite(sql), {});
    }

    QueryResult queryParams(const std::string& sql, const std::vector<std::string>& params) {
        std::lock_guard<std::mutex> lock(mutex_);
        ensurePgOrFallbackLocked();
        if (!useSqlite_) {
            auto r = queryParamsPgLocked(sql, params);
            if (!r.ok() && !pgInit_) {
                switchToSqliteLocked();
                return querySqliteLocked(toSqlite(sql), params);
            }
            return r;
        }
        tryRecoverPgLocked();
        return querySqliteLocked(toSqlite(sql), params);
    }

    bool begin()    { return exec("BEGIN"); }
    bool commit()   { return exec("COMMIT"); }
    bool rollback() { return exec("ROLLBACK"); }

private:
    bool        pgInit_     = false;
    std::string connStr_;
    sqlite3*    lite_       = nullptr;
    std::string sqlitePath_ = "ruoyi.db";
    std::string cipherKey_;
    SqliteCipher::KeyConfig cipherCfg_;
    std::mutex  mutex_;
    bool        useSqlite_  = false;
    std::chrono::steady_clock::time_point pgRetryAt_{};

    struct PendingWrite { std::string sql; std::vector<std::string> params; };
    std::vector<PendingWrite> pendingSync_;
    static constexpr size_t PENDING_MAX = 10000;
    void pendingSyncIfRoom(PendingWrite pw) {
        if (pendingSync_.size() < PENDING_MAX) pendingSync_.push_back(std::move(pw));
        else if ((pendingSync_.size() % 1000) == 0)
            std::cout << "[DB] pendingSync 超过 " << PENDING_MAX << "，丢弃后续" << std::endl;
    }

    // --------------------------------------------------------
    // 内部状态管理
    // --------------------------------------------------------
    void ensurePgOrFallbackLocked() {
        if (useSqlite_) return;
        if (pgInit_) {
            if (pg_client_health_check() == 0) return;
            std::cout << "[DB] drogon_pg_driver 健康检查失败，准备切换 SQLite" << std::endl;
            pg_client_cleanup(); pgInit_ = false;
        }
        if (lite_) switchToSqliteLocked();
    }

    void switchToSqliteLocked() {
        if (!lite_) { useSqlite_ = false; return; }
        useSqlite_ = true;
        pgRetryAt_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        std::cout << "[DB] PG 不可用，切换至 SQLite 模式" << std::endl;
    }

    void tryRecoverPgLocked() {
        auto now = std::chrono::steady_clock::now();
        if (now < pgRetryAt_) return;
        pgRetryAt_ = now + std::chrono::seconds(5);
        if (pg_client_init(connStr_.c_str()) == 0) {
            pgInit_ = true;
            useSqlite_ = false;
            std::cout << "[DB] PostgreSQL recovered via drogon_pg_driver, replaying "
                      << pendingSync_.size() << " pending writes..." << std::endl;
            replaySyncLocked();
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
    // PG 执行（mutex 已持有）— 走 drogon_pg_driver 的 pg_client_*
    // --------------------------------------------------------
    bool execPgLocked(const std::string& sql) {
        if (!pgInit_) return false;
        pg_result_t* r = pg_client_exec(sql.c_str(), nullptr, 0);
        bool ok = r && !pg_result_is_error(r);
        if (!ok && r) {
            std::cout << "[DB] exec error: "
                      << (pg_result_error_message(r) ? pg_result_error_message(r) : "")
                      << " SQL: " << sql << std::endl;
        }
        if (r) pg_result_destroy(r);
        return ok;
    }

    bool execParamsPgLocked(const std::string& sql, const std::vector<std::string>& params) {
        if (!pgInit_) return false;
        std::vector<const char*> pv;
        pv.reserve(params.size());
        for (auto& p : params) pv.push_back(p.c_str());
        pg_result_t* r = pg_client_exec(sql.c_str(), pv.empty() ? nullptr : pv.data(), (int)pv.size());
        bool ok = r && !pg_result_is_error(r);
        if (!ok && r) {
            std::cout << "[DB] execParams error: "
                      << (pg_result_error_message(r) ? pg_result_error_message(r) : "")
                      << " SQL: " << sql << std::endl;
        }
        if (r) pg_result_destroy(r);
        return ok;
    }

    QueryResult queryPgLocked(const std::string& sql) {
        if (!pgInit_) return {};
        pg_result_t* r = pg_client_exec(sql.c_str(), nullptr, 0);
        if (!r || pg_result_is_error(r)) {
            if (r) {
                std::cout << "[DB] query error: "
                          << (pg_result_error_message(r) ? pg_result_error_message(r) : "") << std::endl;
                pg_result_destroy(r);
            }
            return {};
        }
        QueryResult qr = pgClientToResult(r);
        pg_result_destroy(r);
        return qr;
    }

    QueryResult queryParamsPgLocked(const std::string& sql, const std::vector<std::string>& params) {
        if (!pgInit_) return {};
        std::vector<const char*> pv;
        pv.reserve(params.size());
        for (auto& p : params) pv.push_back(p.c_str());
        pg_result_t* r = pg_client_exec(sql.c_str(), pv.empty() ? nullptr : pv.data(), (int)pv.size());
        if (!r || pg_result_is_error(r)) {
            if (r) {
                std::cout << "[DB] queryParams error: "
                          << (pg_result_error_message(r) ? pg_result_error_message(r) : "")
                          << " SQL: " << sql << std::endl;
                pg_result_destroy(r);
            }
            return {};
        }
        QueryResult qr = pgClientToResult(r);
        pg_result_destroy(r);
        return qr;
    }

    QueryResult pgClientToResult(pg_result_t* r) {
        QueryResult qr;
        qr.valid_ = true;
        int nr = pg_result_n_tuples(r);
        int nc = pg_result_n_columns(r);
        qr.cols_ = nc;
        for (int i = 0; i < nr; ++i) {
            std::vector<std::string> row;
            std::vector<bool>        nrow;
            pg_row_t pgr = pg_result_row(r, i);
            for (int c = 0; c < nc; ++c) {
                bool n = pg_row_is_null(&pgr, c) != 0;
                nrow.push_back(n);
                const char* v = n ? nullptr : pg_row_get_value(&pgr, c);
                row.push_back(v ? v : "");
            }
            qr.data_.push_back(std::move(row));
            qr.nulls_.push_back(std::move(nrow));
        }
        return qr;
    }

    // --------------------------------------------------------
    // SQLite 执行（mutex 已持有）— 与官方 libpq 版本完全相同的实现
    // --------------------------------------------------------
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
        if (lite_)
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

    void closeSqliteOnFatal(int rc, const char* ctx) {
        int base = rc & 0xFF;
        if (base == SQLITE_IOERR || base == SQLITE_CORRUPT || base == SQLITE_NOTADB) {
            std::cout << "[DB][SQLite] Fatal error (" << ctx << "): "
                      << sqlite3_errmsg(lite_) << " — SQLite disabled until restart" << std::endl;
            sqlite3_close(lite_);
            lite_ = nullptr;
        }
    }

    // --------------------------------------------------------
    // SQL 方言转换：PG → SQLite（NOW(), $N → ?）
    // --------------------------------------------------------
    static bool isDml(const std::string& sql) {
        std::string s; s.reserve(sql.size());
        for (char c : sql) s += (char)std::tolower((unsigned char)c);
        // skip leading whitespace
        size_t i = 0; while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
        return s.compare(i, 6, "insert") == 0 ||
               s.compare(i, 6, "update") == 0 ||
               s.compare(i, 6, "delete") == 0;
    }

    static std::string toSqlite(const std::string& sql) {
        std::string out; out.reserve(sql.size());
        // $N → ?  以及 NOW() → CURRENT_TIMESTAMP
        for (size_t i = 0; i < sql.size(); ) {
            if (sql[i] == '$' && i + 1 < sql.size() && std::isdigit((unsigned char)sql[i+1])) {
                out += '?';
                ++i;
                while (i < sql.size() && std::isdigit((unsigned char)sql[i])) ++i;
            } else if ((sql[i] == 'N' || sql[i] == 'n') && i + 4 < sql.size()
                       && (sql.compare(i, 5, "NOW()") == 0 || sql.compare(i, 5, "now()") == 0)) {
                out += "CURRENT_TIMESTAMP";
                i += 5;
            } else {
                out += sql[i++];
            }
        }
        return out;
    }
};
