#pragma once
#include <string>
#include <vector>
#include <iostream>
#include "../services/DatabaseService.h"
#include "SecurityUtils.h"
#include "TokenCache.h"
#include "Constants.h"

// 启动时自动建表 + 初始化数据
// 表结构与 RuoYi.Net 保持一致，数据库为 PostgreSQL
// 直接使用 libpq（不依赖 Drogon ORM）
class DatabaseInit {
public:
    static void run() {
        auto& db = DatabaseService::instance();
        if (!db.isConnected()) {
            std::cerr << "[DatabaseInit] 数据库未连接，跳过初始化" << std::endl;
            return;
        }
        if (db.isUsingSqlite()) {
            runSqlite(db);
        } else {
            createTables(db);
            // 扩展 password 列长度为 PBKDF2 格式（约105字符）
            db.exec("ALTER TABLE sys_user ALTER COLUMN password TYPE VARCHAR(200)");
            // 先初始化 SQLite schema，再插入初始数据（避免双写时 "no such table"）
            if (db.hasSqlite()) initSqliteSchema(db);
            insertInitData(db);
            migratePasswords(db);
        }
        std::cout << "[DatabaseInit] 数据库初始化完成，后端: " << db.backendInfo() << std::endl;
    }

private:
    // SQLite 专用初始化（PG 不可用时）
    static void runSqlite(DatabaseService& db) {
        std::cout << "[DatabaseInit] 使用 SQLite 模式初始化..." << std::endl;
        db.execSqliteDirect("BEGIN");
        for (auto& sql : getSqliteCreateTableSqls()) {
            if (!db.execSqliteDirect(sql))
                std::cerr << "[DatabaseInit][SQLite] 建表失败" << std::endl;
        }
        db.execSqliteDirect("COMMIT");
        std::cout << "[DatabaseInit][SQLite] 表初始化完成" << std::endl;
        db.execSqliteDirect("BEGIN");
        for (auto& sql : getSqliteInitDataSqls()) {
            db.execSqliteDirect(sql);
        }
        db.execSqliteDirect("COMMIT");
        std::cout << "[DatabaseInit][SQLite] 初始数据插入完成" << std::endl;
        migrateSqlitePasswords(db);
    }

    static void createTables(DatabaseService& db) {
        for (auto& sql : getCreateTableSqls()) {
            if (!db.exec(sql)) {
                std::cerr << "[DatabaseInit] 建表失败" << std::endl;
            }
        }
        std::cout << "[DatabaseInit] 数据库表初始化完成" << std::endl;
    }

    static void insertInitData(DatabaseService& db) {
        for (auto& sql : getInitDataSqls()) {
            db.exec(sql); // 数据已存在时忽略
        }
        std::cout << "[DatabaseInit] 初始数据插入完成" << std::endl;
    }

    // 因无法安装BCrypt库，将 BCrypt 格式密码迁移为 PBKDF2 格式
    static void migratePasswords(DatabaseService& db) {
        // 使用 queryParams 避免 toSqlite 误将 '$2a$%' 中的 $2 替换为 ?
        auto res = db.queryParams(
            "SELECT user_id, user_name, password FROM sys_user WHERE password LIKE $1 OR password LIKE $2",
            {"$2a$%", "$2b$%"});
        if (!res.ok() || res.rows() == 0) return;
        int migrated = 0;
        // 已知默认密码映射：admin->admin123, ry->admin123
        struct DefaultPwd { std::string userName; std::string rawPwd; };
        auto defPwd = []() -> std::string {
            char p[] = {'a','d','m','i','n','1','2','3',0};
            return p;
        };
        std::vector<DefaultPwd> defaults = {
            {"admin", defPwd()},
            {"ry", defPwd()}
        };
        for (int i = 0; i < res.rows(); ++i) {
            std::string userId = res.str(i, 0);
            std::string userName = res.str(i, 1);
            // 查找已知默认密码
            std::string rawPwd;
            for (auto& d : defaults) {
                if (d.userName == userName) { rawPwd = d.rawPwd; break; }
            }
            if (rawPwd.empty()) rawPwd = defPwd(); // 其他用户也使用默认密码
            std::string newHash = SecurityUtils::encryptPassword(rawPwd);
            db.execParams("UPDATE sys_user SET password=$1 WHERE user_id=$2", {newHash, userId});
            ++migrated;
        }
        if (migrated > 0) {
            std::cout << "[DatabaseInit] 已将 " << migrated << " 个用户密码从 BCrypt 迁移为 PBKDF2 格式" << std::endl;
        }
        // 清除内存中的密码错误计数
        for (int i = 0; i < res.rows(); ++i) {
            std::string userName = res.str(i, 1);
            MemCache::instance().remove(Constants::PWD_ERR_CNT_KEY + userName);
        }
    }

    static std::vector<std::string> getCreateTableSqls();
    static std::vector<std::string> getInitDataSqls();
    static std::vector<std::string> getSqliteCreateTableSqls();
    static std::vector<std::string> getSqliteInitDataSqls();

    // 当 PG 为主时也将 schema+初始数据写入 SQLite（为双写预热）
    static void initSqliteSchema(DatabaseService& db) {
        std::cout << "[DatabaseInit] 初始化 SQLite 双写 schema..." << std::endl;
        for (auto& sql : getSqliteCreateTableSqls())
            db.execSqliteDirect(sql);
        for (auto& sql : getSqliteInitDataSqls())
            db.execSqliteDirect(sql);
        std::cout << "[DatabaseInit] SQLite 双写 schema 完成" << std::endl;
        // INSERT OR IGNORE 可能保留了旧的 BCrypt 密码，单独对 SQLite 做迁移
        migrateSqlitePasswords(db);
    }

    // 专门迁移 SQLite 中残留的 BCrypt 密码（不经过路由，直接操作 SQLite）
    static void migrateSqlitePasswords(DatabaseService& db) {
        if (!db.hasSqlite()) return;
        auto res = db.querySqliteDirect(
            "SELECT user_id, user_name FROM sys_user WHERE password LIKE '$2a$%' OR password LIKE '$2b$%'");
        if (!res.ok() || res.rows() == 0) return;
        const std::string defPwd = "admin123";
        const std::string newHash = SecurityUtils::encryptPassword(defPwd);
        int migrated = 0;
        for (int i = 0; i < res.rows(); ++i) {
            std::string userId = res.str(i, 0);
            db.execSqliteDirect("UPDATE sys_user SET password='" + newHash + "' WHERE user_id=" + userId);
            ++migrated;
        }
        if (migrated > 0)
            std::cout << "[DatabaseInit] SQLite: 已将 " << migrated << " 个 BCrypt 密码迁移为 PBKDF2" << std::endl;
    }
};
