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

// ============================================================
// DatabaseService �?PostgreSQL 主库 + SQLite 自动回退
// 公开 API 与旧版完全兼容（QueryResult 接口不变�?// 新增：connectSqlite(), isUsingSqlite(), backendInfo()
// ============================================================
class DatabaseService {
public:
    // --------------------------------------------------------
    // 通用结果集（同时支持 PG �?SQLite�?    // --------------------------------------------------------
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
        if (conn_) { PQfinish(conn_); conn_ = nullptr; }
        if (lite_) { sqlite3_close(lite_); lite_ = nullptr; }
    }

    // --------------------------------------------------------
    // 连接
    // --------------------------------------------------------
    bool connect(const std::string& connStr) {
        connStr_ = connStr;
        conn_ = PQconnectdb(connStr.c_str());
        if (PQstatus(conn_) != CONNECTION_OK) {
            std::cout << "[DB] PostgreSQL 连接失败: " << PQerrorMessage(conn_) << std::endl;
            PQfinish(conn_); conn_ = nullptr;
            return false;
        }
        PQsetClientEncoding(conn_, "UTF8");
        useSqlite_ = false;
        std::cout << "[DB] Connected to PostgreSQL" << std::endl;
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
        return conn_ && PQstatus(conn_) == CONNECTION_OK;
    }
    bool hasSqlite() const { return lite_ != nullptr; }
    bool isUsingSqlite() const { return useSqlite_; }
    PGconn* raw() { return conn_; }

    bool reconnect() {
        if (conn_) { PQfinish(conn_); conn_ = nullptr; }
        return connect(connStr_);
    }
    bool ensureConnection() {
        if (isConnected()) return true;
        if (!useSqlite_) return reconnect();
        return lite_ != nullptr;
    }

    std::string backendInfo() const {
        if (!useSqlite_) return "postgresql";
        return "sqlite(" + sqlitePath_ + ")";
    }

    // manually activate SQLite fallback when PG startup fails
    void activateSqliteFallback() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (lite_) { useSqlite_ = true; std::cout << "[DB] SQLite fallback activated" << std::endl; }
    }

    // directly execute SQL on SQLite (bypass routing, for schema init)
    bool execSqliteDirect(const std::string& sql) {
        std::lock_guard<std::mutex> lock(mutex_);
        return execSqliteLocked(sql, {});
    }

    // --------------------------------------------------------
    // exec �?DML / DDL（无返回行）
    //   PG 可用时：�?PG；若�?DML 则同时写 SQLite（双写）
    //   PG 不可用：�?SQLite，并�?DML 存入 pendingSync_
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
    // query �?SELECT（返回行�?    //   优先�?PG；PG 失败时自动切�?SQLite
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
    // 状�?    // --------------------------------------------------------
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

    // �?PG 风格 SQL 翻译�?SQLite 风格
    static std::string toSqlite(const std::string& in) {
        std::string s;
        s.reserve(in.size());

        // 1. $N �??
        for (size_t i = 0; i < in.size(); ) {
            if (in[i] == '$' && i+1 < in.size() && std::isdigit((unsigned char)in[i+1])) {
                s += '?';
                ++i;
                while (i < in.size() && std::isdigit((unsigned char)in[i])) ++i;
            } else {
                s += in[i++];
            }
        }

        // 2. NOW() �?datetime('now')
        s = replaceCI(s, "NOW()", "datetime('now')", false);
        // 3. TRUE / FALSE �?1 / 0
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
        if (conn_ && PQstatus(conn_) == CONNECTION_OK) return;
        // 有冷却时间时跳过重连，避免每次请求都阻塞
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

        if (conn_) { PQfinish(conn_); conn_ = nullptr; }
        conn_ = PQconnectdb(connStr_.c_str());
        if (PQstatus(conn_) == CONNECTION_OK) {
            PQsetClientEncoding(conn_, "UTF8");
            useSqlite_ = false;
            std::cout << "[DB] PostgreSQL recovered, replaying " << pendingSync_.size() << " pending writes..." << std::endl;
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
        if (!conn_) return false;
        PGresult* res = PQexec(conn_, sql.c_str());
        bool ok = (PQresultStatus(res)==PGRES_COMMAND_OK || PQresultStatus(res)==PGRES_TUPLES_OK);
        if (!ok) std::cout << "[DB] exec error: " << PQerrorMessage(conn_) << std::endl;
        PQclear(res);
        return ok;
    }

    bool execParamsPgLocked(const std::string& sql, const std::vector<std::string>& params) {
        if (!conn_) return false;
        std::vector<const char*> pv;
        for (auto& p : params) pv.push_back(p.c_str());
        PGresult* res = PQexecParams(conn_, sql.c_str(), (int)pv.size(), nullptr, pv.data(), nullptr, nullptr, 0);
        bool ok = (PQresultStatus(res)==PGRES_COMMAND_OK || PQresultStatus(res)==PGRES_TUPLES_OK);
        if (!ok) std::cout << "[DB] execParams error: " << PQerrorMessage(conn_) << " SQL: " << sql << std::endl;
        PQclear(res);
        return ok;
    }

    QueryResult queryPgLocked(const std::string& sql) {
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
        if (!conn_) return {};
        std::vector<const char*> pv;
        for (auto& p : params) pv.push_back(p.c_str());
        PGresult* res = PQexecParams(conn_, sql.c_str(), (int)pv.size(), nullptr, pv.data(), nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::cout << "[DB] queryParams error: " << PQerrorMessage(conn_) << " SQL: " << sql << std::endl;
            PQclear(res); return {};
        }
        QueryResult qr = pgToResult(res);
        PQclear(res);
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
    bool execSqliteLocked(const std::string& sql, const std::vector<std::string>& params) {
        if (!lite_) return false;
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(lite_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cout << "[DB][SQLite] prepare error: " << sqlite3_errmsg(lite_) << "\n  SQL: " << sql << std::endl;
            return false;
        }
        for (int i = 0; i < (int)params.size(); ++i)
            sqlite3_bind_text(stmt, i+1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc == SQLITE_DONE || rc == SQLITE_ROW) return true;
        std::cout << "[DB][SQLite] exec error: " << sqlite3_errmsg(lite_) << "\n  SQL: " << sql << std::endl;
        return false;
    }

    QueryResult querySqliteLocked(const std::string& sql, const std::vector<std::string>& params) {
        QueryResult qr;
        if (!lite_) return qr;
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(lite_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cout << "[DB][SQLite] prepare error: " << sqlite3_errmsg(lite_) << "\n  SQL: " << sql << std::endl;
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
