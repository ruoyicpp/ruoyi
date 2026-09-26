/**
 * @file DataSourceManager.h
 * @brief 多数据源管理 — 动态注册 PG/SQLite 数据源、连通性测试、只读查询路由
 *
 * 功能概述：
 *   - 数据源注册表：sys_datasource 表持久化 + 内存缓存
 *   - 类型支持：postgres（libpq 直连）/ sqlite（本地文件）
 *   - 连通性测试：test() 实时探测，记录 last_ok/last_error
 *   - 只读查询：queryOn() 按名路由到指定数据源（连接即用即关，适合监控/报表）
 *   - 默认标记：is_default 标记主数据源（主连接切换需重启生效）
 *
 * 设计说明：
 *   - 主业务连接仍由 DatabaseService 单例管理（PG+SQLite 双写降级）
 *   - 本模块管理"辅助数据源"：报表库、只读副本、外部业务库
 *   - queryOn 每次新建连接（管理/监控场景低频，无需连接池）
 *
 * 配置项：无（数据源存 sys_datasource 表，运行时管理）
 */

#pragma once
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <unordered_map>
#include <cctype>
#include <ctime>
#include <libpq-fe.h>
#include <sqlite3.h>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include "../services/DatabaseService.h"

/**
 * @class DataSourceManager
 * @brief 多数据源管理单例
 */
class DataSourceManager {
public:
    struct DataSource {
        long        id        = 0;
        std::string name;                    ///< 唯一标识（如 "report_db"）
        std::string dbType;                  ///< postgres | sqlite
        std::string host       = "127.0.0.1";
        int         port       = 5432;
        std::string dbname;
        std::string username;
        std::string passwd;
        std::string status     = "0";        ///< 0启用 1停用
        bool        isDefault  = false;
        // 运行时状态（不持久化）
        bool        lastOk     = false;
        std::string lastError;
        int64_t     lastTestAt = 0;
    };

    static DataSourceManager& instance() {
        static DataSourceManager inst;
        return inst;
    }

    /// 启动时从 sys_datasource 加载注册表
    void loadAll() {
        auto res = DatabaseService::instance().query(
            "SELECT id,name,db_type,host,port,dbname,username,passwd,status,is_default "
            "FROM sys_datasource ORDER BY id");
        if (!res.ok()) return;
        std::lock_guard<std::mutex> lk(mu_);
        sources_.clear();
        for (int i = 0; i < res.rows(); ++i) {
            DataSource ds;
            ds.id        = res.longVal(i, 0);
            ds.name      = res.str(i, 1);
            ds.dbType    = res.str(i, 2);
            ds.host      = res.str(i, 3);
            ds.port      = res.isNull(i, 4) ? 5432 : (int)res.longVal(i, 4);
            ds.dbname    = res.str(i, 5);
            ds.username  = res.str(i, 6);
            ds.passwd    = res.str(i, 7);
            ds.status    = res.str(i, 8);
            ds.isDefault = res.str(i, 9) == "1" || res.str(i, 9) == "t";
            sources_[ds.name] = ds;
        }
        LOG_INFO << "[DataSource] loaded " << sources_.size() << " datasources";
    }

    /// 数据源列表（passwd 脱敏）
    std::vector<DataSource> list() {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<DataSource> out;
        for (auto& [_, ds] : sources_) {
            DataSource copy = ds;
            copy.passwd = copy.passwd.empty() ? "" : "******";
            out.push_back(copy);
        }
        return out;
    }

    /// 新增数据源（持久化 + 注册）
    bool add(const DataSource& ds, std::string& err) {
        if (ds.name.empty() || ds.dbType.empty()) { err = "name/db_type 必填"; return false; }
        if (ds.dbType != "postgres" && ds.dbType != "sqlite") {
            err = "db_type 仅支持 postgres/sqlite"; return false;
        }
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (sources_.count(ds.name)) { err = "数据源名已存在: " + ds.name; return false; }
        }
        bool ok = DatabaseService::instance().execParams(
            "INSERT INTO sys_datasource(name,db_type,host,port,dbname,username,passwd,status,is_default) "
            "VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9)",
            {ds.name, ds.dbType, ds.host, std::to_string(ds.port), ds.dbname,
             ds.username, ds.passwd, ds.status, ds.isDefault ? "1" : "0"});
        if (!ok) { err = "写入 sys_datasource 失败"; return false; }
        std::lock_guard<std::mutex> lk(mu_);
        sources_[ds.name] = ds;
        return true;
    }

    /// 删除数据源
    bool remove(const std::string& name) {
        DatabaseService::instance().execParams(
            "DELETE FROM sys_datasource WHERE name=$1", {name});
        std::lock_guard<std::mutex> lk(mu_);
        return sources_.erase(name) > 0;
    }

    /// 连通性测试（实时探测，更新 lastOk/lastError）
    bool test(const std::string& name, std::string& err) {
        DataSource ds;
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = sources_.find(name);
            if (it == sources_.end()) { err = "数据源不存在: " + name; return false; }
            ds = it->second;
        }
        bool ok = false;
        if (ds.dbType == "postgres") {
            std::string ci = "host=" + ds.host + " port=" + std::to_string(ds.port)
                           + " dbname=" + ds.dbname + " user=" + ds.username
                           + " password=" + ds.passwd + " connect_timeout=5";
            PGconn* c = PQconnectdb(ci.c_str());
            if (PQstatus(c) == CONNECTION_OK) ok = true;
            else err = PQerrorMessage(c);
            PQfinish(c);
        } else {
            sqlite3* db = nullptr;
            if (sqlite3_open(ds.dbname.c_str(), &db) == SQLITE_OK) ok = true;
            else err = sqlite3_errmsg(db);
            if (db) sqlite3_close(db);
        }
        std::lock_guard<std::mutex> lk(mu_);
        auto& s = sources_[name];
        s.lastOk = ok; s.lastError = err; s.lastTestAt = std::time(nullptr);
        return ok;
    }

    /**
     * @brief 在指定数据源执行只读查询，返回行集 JSON
     * @note 仅限 SELECT；连接即用即关，适合监控/报表场景
     */
    std::optional<Json::Value> queryOn(const std::string& name,
                                       const std::string& sql,
                                       std::string& err) {
        // 安全检查：仅允许 SELECT/WITH/SHOW/EXPLAIN
        std::string head = sql.substr(0, 16);
        for (auto& c : head) c = std::toupper(c);
        if (head.find("SELECT") != 0 && head.find("WITH") != 0 &&
            head.find("SHOW") != 0 && head.find("EXPLAIN") != 0) {
            err = "仅允许只读查询（SELECT/WITH/SHOW/EXPLAIN）";
            return std::nullopt;
        }
        DataSource ds;
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = sources_.find(name);
            if (it == sources_.end()) { err = "数据源不存在: " + name; return std::nullopt; }
            ds = it->second;
        }
        if (ds.status != "0") { err = "数据源已停用"; return std::nullopt; }

        Json::Value rows(Json::arrayValue);
        if (ds.dbType == "postgres") {
            std::string ci = "host=" + ds.host + " port=" + std::to_string(ds.port)
                           + " dbname=" + ds.dbname + " user=" + ds.username
                           + " password=" + ds.passwd + " connect_timeout=5";
            PGconn* c = PQconnectdb(ci.c_str());
            if (PQstatus(c) != CONNECTION_OK) {
                err = PQerrorMessage(c); PQfinish(c); return std::nullopt;
            }
            PGresult* r = PQexec(c, sql.c_str());
            if (PQresultStatus(r) != PGRES_TUPLES_OK) {
                err = PQerrorMessage(c);
                PQclear(r); PQfinish(c); return std::nullopt;
            }
            int cols = PQnfields(r);
            for (int i = 0; i < PQntuples(r); ++i) {
                Json::Value row(Json::objectValue);
                for (int j = 0; j < cols; ++j)
                    row[PQfname(r, j)] = PQgetisnull(r, i, j) ? Json::Value()
                                                            : PQgetvalue(r, i, j);
                rows.append(row);
            }
            PQclear(r); PQfinish(c);
        } else {
            sqlite3* db = nullptr;
            if (sqlite3_open(ds.dbname.c_str(), &db) != SQLITE_OK) {
                err = db ? sqlite3_errmsg(db) : "open failed";
                if (db) sqlite3_close(db);
                return std::nullopt;
            }
            sqlite3_stmt* st = nullptr;
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
                err = sqlite3_errmsg(db);
                sqlite3_close(db); return std::nullopt;
            }
            int cols = sqlite3_column_count(st);
            while (sqlite3_step(st) == SQLITE_ROW) {
                Json::Value row(Json::objectValue);
                for (int j = 0; j < cols; ++j) {
                    const char* v = (const char*)sqlite3_column_text(st, j);
                    row[sqlite3_column_name(st, j)] = v ? v : "";
                }
                rows.append(row);
            }
            sqlite3_finalize(st); sqlite3_close(db);
        }
        return rows;
    }

    /// 标记默认数据源（主连接切换需重启生效）
    bool setDefault(const std::string& name) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!sources_.count(name)) return false;
        }
        auto& db = DatabaseService::instance();
        db.exec("UPDATE sys_datasource SET is_default='0'");
        db.execParams("UPDATE sys_datasource SET is_default='1' WHERE name=$1", {name});
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& [_, ds] : sources_) ds.isDefault = (ds.name == name);
        return true;
    }

private:
    DataSourceManager() = default;
    std::unordered_map<std::string, DataSource> sources_;
    std::mutex mu_;
};
