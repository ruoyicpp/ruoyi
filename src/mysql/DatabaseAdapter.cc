/**
 * @file DatabaseAdapter.cc
 * @brief 数据库运行时适配层实现
 */

#include "DatabaseAdapter.h"
#include <regex>
#include <algorithm>

namespace ruoyi {

bool DatabaseAdapter::init(const Json::Value& config) {
    if (!config.isMember("type")) {
        LOG_WARN << "[DatabaseAdapter] config.database.type not found, using PostgreSQL as default";
        _type = DatabaseType::POSTGRESQL;
    } else {
        std::string type = config["type"].asString();
        std::transform(type.begin(), type.end(), type.begin(), ::tolower);
        
        if (type == "mysql" || type == "mariadb") {
            _type = DatabaseType::MYSQL;
        } else if (type == "postgresql" || type == "postgres" || type == "pg") {
            _type = DatabaseType::POSTGRESQL;
        } else if (type == "sqlite3" || type == "sqlite") {
            _type = DatabaseType::SQLITE3;
        } else {
            LOG_WARN << "[DatabaseAdapter] Unknown database type: " << type << ", using PostgreSQL as default";
            _type = DatabaseType::POSTGRESQL;
        }
    }

    // 读取连接信息
    if (config.isMember("host")) {
        _host = config["host"].asString();
    }
    if (config.isMember("port")) {
        _port = config["port"].asInt();
    }
    if (config.isMember("dbname")) {
        _dbname = config["dbname"].asString();
    }
    if (config.isMember("user")) {
        _user = config["user"].asString();
    }
    if (config.isMember("passwd")) {
        _passwd = config["passwd"].asString();
    }

    LOG_INFO << "[DatabaseAdapter] Initialized with type: " << getTypeName()
             << ", host: " << _host << ":" << _port
             << ", dbname: " << _dbname;

    return true;
}

std::string DatabaseAdapter::getTypeName() const {
    switch (_type) {
        case DatabaseType::MYSQL:
            return "MySQL";
        case DatabaseType::POSTGRESQL:
            return "PostgreSQL";
        case DatabaseType::SQLITE3:
            return "SQLite3";
        default:
            return "Unknown";
    }
}

std::string DatabaseAdapter::convertSql(const std::string& pgSql) const {
    if (_type == DatabaseType::POSTGRESQL) {
        // PostgreSQL 不需要转换
        return pgSql;
    }

    std::string result = pgSql;

    // MySQL 转换
    if (_type == DatabaseType::MYSQL) {
        // 1. 转换参数占位符：$1, $2 → ?
        result = convertPlaceholders(result);

        // 2. 转换 UPSERT
        result = convertUpsert(result);

        // 3. 转换 RETURNING
        result = convertReturning(result);

        // 4. 转换布尔值：true → 1, false → 0
        result = std::regex_replace(result, std::regex("\\btrue\\b"), "1");
        result = std::regex_replace(result, std::regex("\\bfalse\\b"), "0");
        result = std::regex_replace(result, std::regex("\\bTRUE\\b"), "1");
        result = std::regex_replace(result, std::regex("\\bFALSE\\b"), "0");
    }

    return result;
}

std::string DatabaseAdapter::convertPlaceholders(const std::string& pgSql) const {
    if (_type == DatabaseType::POSTGRESQL) {
        return pgSql;
    }

    // MySQL: 将 $1, $2, $3... 转换为 ?
    std::string result = pgSql;
    std::regex placeholder(R"(\$\d+)");
    result = std::regex_replace(result, placeholder, "?");
    return result;
}

std::string DatabaseAdapter::convertUpsert(const std::string& pgSql) const {
    if (_type == DatabaseType::POSTGRESQL) {
        return pgSql;
    }

    std::string result = pgSql;

    // PostgreSQL: ON CONFLICT (col) DO UPDATE SET col1 = EXCLUDED.col1
    // MySQL:      ON DUPLICATE KEY UPDATE col1 = VALUES(col1)

    // 1. 替换 ON CONFLICT ... DO UPDATE SET 为 ON DUPLICATE KEY UPDATE
    std::regex conflictPattern(
        R"(ON\s+CONFLICT\s*\([^)]*\)\s+DO\s+UPDATE\s+SET)",
        std::regex::icase
    );
    result = std::regex_replace(result, conflictPattern, "ON DUPLICATE KEY UPDATE");

    // 2. 替换 EXCLUDED.col 为 VALUES(col)
    std::regex excludedPattern(R"(EXCLUDED\.(\w+))", std::regex::icase);
    result = std::regex_replace(result, excludedPattern, "VALUES($1)");

    return result;
}

std::string DatabaseAdapter::convertReturning(const std::string& pgSql) const {
    if (_type == DatabaseType::POSTGRESQL) {
        return pgSql;
    }

    // MySQL 不支持 RETURNING，删除该子句
    std::string result = pgSql;
    std::regex returningPattern(R"(\s+RETURNING\s+\w+\s*;?)", std::regex::icase);
    result = std::regex_replace(result, returningPattern, ";");
    
    // 清理多余的分号
    result = std::regex_replace(result, std::regex(R"(;+)"), ";");

    return result;
}

std::string DatabaseAdapter::convertBoolean(bool value) const {
    if (_type == DatabaseType::MYSQL) {
        return value ? "1" : "0";
    }
    // PostgreSQL
    return value ? "true" : "false";
}

std::string DatabaseAdapter::convertBooleanString(const std::string& value) const {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    bool boolValue = (lower == "true" || lower == "1" || lower == "yes");

    if (_type == DatabaseType::MYSQL) {
        return boolValue ? "1" : "0";
    }
    // PostgreSQL
    return boolValue ? "true" : "false";
}

std::string DatabaseAdapter::getCurrentTimestampFunc() const {
    // 两个数据库都使用 NOW()
    return "NOW()";
}

std::string DatabaseAdapter::getLastInsertIdFunc() const {
    if (_type == DatabaseType::MYSQL) {
        return "LAST_INSERT_ID()";
    }
    // PostgreSQL
    return "LASTVAL()";
}

std::string DatabaseAdapter::getConcatFunc(const std::string& a, const std::string& b) const {
    if (_type == DatabaseType::MYSQL) {
        return "CONCAT(" + a + ", " + b + ")";
    }
    // PostgreSQL
    return "(" + a + " || " + b + ")";
}

std::string DatabaseAdapter::getLimitOffsetClause(int limit, int offset) const {
    if (_type == DatabaseType::MYSQL) {
        // MySQL: LIMIT offset, limit
        return "LIMIT " + std::to_string(offset) + ", " + std::to_string(limit);
    }
    // PostgreSQL: LIMIT limit OFFSET offset
    return "LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset);
}

}  // namespace ruoyi
