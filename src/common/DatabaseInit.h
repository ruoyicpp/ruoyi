#pragma once
#include <string>
#include <vector>
#include <iostream>
#include "../services/DatabaseService.h"
#include "SecurityUtils.h"
#include "TokenCache.h"
#include "Constants.h"

// ���ʱ�Զ����� + ��ʼ������
// ��ṹ�� RuoYi.Net ����һ�£����ݿ�Ϊ PostgreSQL
// ֱ��ʹ�� libpq�������� Drogon ORM��
class DatabaseInit {
public:
    static void run() {
        auto& db = DatabaseService::instance();
        if (!db.isConnected()) {
            std::cerr << "[DatabaseInit] ���ݿ�δ���ӣ�������ʼ��" << std::endl;
            return;
        }
        if (db.isUsingSqlite()) {
            runSqlite(db);
        } else {
            createTables(db);
            // ���� password �������� PBKDF2 ��ʽ��Լ105�ַ���
            db.exec("ALTER TABLE sys_user ALTER COLUMN password TYPE VARCHAR(200)");
            insertInitData(db);
            migratePasswords(db);
            // �� SQLite �Ѵ򿪣���ʼ���� schema ��֧��˫д����
            if (db.hasSqlite()) initSqliteSchema(db);
        }
        std::cout << "[DatabaseInit] ���ݿ��ʼ����ɣ����: " << db.backendInfo() << std::endl;
    }

private:
    // SQLite ר�ó�ʼ����PG ������ʱ��
    static void runSqlite(DatabaseService& db) {
        std::cout << "[DatabaseInit] ʹ�� SQLite ���˿��ʼ��..." << std::endl;
        for (auto& sql : getSqliteCreateTableSqls()) {
            if (!db.exec(sql))
                std::cerr << "[DatabaseInit][SQLite] ����ʧ��" << std::endl;
        }
        std::cout << "[DatabaseInit][SQLite] ���ʼ�����" << std::endl;
        for (auto& sql : getSqliteInitDataSqls()) {
            db.exec(sql);
        }
        std::cout << "[DatabaseInit][SQLite] ��ʼ���ݲ������" << std::endl;
        migratePasswords(db);
    }

    static void createTables(DatabaseService& db) {
        for (auto& sql : getCreateTableSqls()) {
            if (!db.exec(sql)) {
                std::cerr << "[DatabaseInit] ����ʧ��" << std::endl;
            }
        }
        std::cout << "[DatabaseInit] ���ݿ���ʼ�����" << std::endl;
    }

    static void insertInitData(DatabaseService& db) {
        for (auto& sql : getInitDataSqls()) {
            db.exec(sql); // �����Ѵ���ʱ����
        }
        std::cout << "[DatabaseInit] ��ʼ���ݲ������" << std::endl;
    }

    // ��Ϊ�޷���װBCrypt �⽫ BCrypt ��ʽ����Ǩ��Ϊ PBKDF2 ��ʽ
    static void migratePasswords(DatabaseService& db) {
        auto res = db.query("SELECT user_id, user_name, password FROM sys_user WHERE password LIKE '$2a$%' OR password LIKE '$2b$%'");
        if (!res.ok() || res.rows() == 0) return;
        int migrated = 0;
        // ��֪Ĭ������ӳ�䣺admin->admin123, ry->admin123
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
            // ������֪Ĭ������
            std::string rawPwd;
            for (auto& d : defaults) {
                if (d.userName == userName) { rawPwd = d.rawPwd; break; }
            }
            if (rawPwd.empty()) rawPwd = defPwd(); // �����û�����ΪĬ������
            std::string newHash = SecurityUtils::encryptPassword(rawPwd);
            db.execParams("UPDATE sys_user SET password=$1 WHERE user_id=$2", {newHash, userId});
            ++migrated;
        }
        if (migrated > 0) {
            std::cout << "[DatabaseInit] �ѽ� " << migrated << " ���û������ BCrypt Ǩ��Ϊ PBKDF2 ��ʽ" << std::endl;
        }
        // ����ڴ��е���������������
        for (int i = 0; i < res.rows(); ++i) {
            std::string userName = res.str(i, 1);
            MemCache::instance().remove(Constants::PWD_ERR_CNT_KEY + userName);
        }
    }

    static std::vector<std::string> getCreateTableSqls();
    static std::vector<std::string> getInitDataSqls();
    static std::vector<std::string> getSqliteCreateTableSqls();
    static std::vector<std::string> getSqliteInitDataSqls();

    // �� PG Ϊ����ʱ��Ҳ�� schema+����д�� SQLite������˫дԤ�ȣ�
    static void initSqliteSchema(DatabaseService& db) {
        std::cout << "[DatabaseInit] ��ʼ�� SQLite ˫д schema..." << std::endl;
        for (auto& sql : getSqliteCreateTableSqls())
            db.execSqliteDirect(sql);
        for (auto& sql : getSqliteInitDataSqls())
            db.execSqliteDirect(sql);
        std::cout << "[DatabaseInit] SQLite ˫д schema ���" << std::endl;
    }
};
