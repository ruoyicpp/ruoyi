/**
 * @file DatabaseAdapter.h
 * @brief 数据库运行时适配层 - 支持 MySQL 和 PostgreSQL 同时使用
 * 
 * 设计思路：
 * 1. 编译时：包含所有数据库驱动（MySQL + PostgreSQL）
 * 2. 运行时：根据 config.json 的 database.type 选择驱动
 * 3. 业务代码：统一使用 PostgreSQL 语法，自动转换为对应数据库
 * 
 * 支持的数据库：
 * - MySQL 5.7+ / 8.0+
 * - PostgreSQL 9.4+
 * - SQLite3（可选）
 */

#pragma once

#include <string>
#include <memory>
#include <drogon/drogon.h>
#include <json/json.h>

namespace ruoyi {

/**
 * @brief 数据库类型枚举
 */
enum class DatabaseType {
    POSTGRESQL = 0,  // PostgreSQL（默认）
    MYSQL = 1,       // MySQL / MariaDB
    SQLITE3 = 2      // SQLite3
};

/**
 * @brief 数据库适配器 - 运行时选择数据库驱动
 * 
 * 使用示例：
 * @code
 * // 初始化
 * auto adapter = DatabaseAdapter::instance();
 * adapter.init(config["database"]);
 * 
 * // 查询（自动转换 SQL）
 * auto result = adapter.query("SELECT * FROM users WHERE id = $1", userId);
 * @endcode
 */
class DatabaseAdapter {
public:
    /**
     * @brief 获取单例实例
     */
    static DatabaseAdapter& instance() {
        static DatabaseAdapter _instance;
        return _instance;
    }

    /**
     * @brief 初始化数据库适配器
     * @param config 数据库配置（来自 config.json）
     * @return 初始化是否成功
     */
    bool init(const Json::Value& config);

    /**
     * @brief 获取当前数据库类型
     */
    DatabaseType getType() const { return _type; }

    /**
     * @brief 获取数据库类型名称
     */
    std::string getTypeName() const;

    /**
     * @brief 检查是否为 MySQL
     */
    bool isMysql() const { return _type == DatabaseType::MYSQL; }

    /**
     * @brief 检查是否为 PostgreSQL
     */
    bool isPostgresql() const { return _type == DatabaseType::POSTGRESQL; }

    /**
     * @brief 检查是否为 SQLite3
     */
    bool isSqlite3() const { return _type == DatabaseType::SQLITE3; }

    /**
     * @brief 转换 SQL 语句
     * 
     * 将 PostgreSQL 语法转换为对应数据库的语法
     * 
     * @param pgSql PostgreSQL 格式的 SQL 语句
     * @return 转换后的 SQL 语句
     * 
     * 转换规则：
     * - MySQL: $1, $2 → ?, ?
     * - MySQL: RETURNING col → （删除）
     * - MySQL: ON CONFLICT → ON DUPLICATE KEY UPDATE
     * - MySQL: EXCLUDED → VALUES()
     * - PostgreSQL: 保持原样
     */
    std::string convertSql(const std::string& pgSql) const;

    /**
     * @brief 转换参数占位符
     * 
     * @param pgSql PostgreSQL 格式的 SQL（使用 $1, $2 等）
     * @return 转换后的 SQL（MySQL 使用 ?，PostgreSQL 保持原样）
     */
    std::string convertPlaceholders(const std::string& pgSql) const;

    /**
     * @brief 转换 UPSERT 语句
     * 
     * PostgreSQL: ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name
     * MySQL:      ON DUPLICATE KEY UPDATE name = VALUES(name)
     */
    std::string convertUpsert(const std::string& pgSql) const;

    /**
     * @brief 转换 RETURNING 子句
     * 
     * PostgreSQL: RETURNING id
     * MySQL:      （删除，使用 LAST_INSERT_ID()）
     */
    std::string convertReturning(const std::string& pgSql) const;

    /**
     * @brief 转换布尔值
     * 
     * @param value 布尔值
     * @return MySQL: 0/1，PostgreSQL: true/false
     */
    std::string convertBoolean(bool value) const;

    /**
     * @brief 转换布尔字符串
     * 
     * @param value 布尔字符串（"true"/"false" 或 "1"/"0"）
     * @return 转换后的值
     */
    std::string convertBooleanString(const std::string& value) const;

    /**
     * @brief 获取当前时间函数
     * 
     * @return MySQL: NOW()，PostgreSQL: NOW()
     */
    std::string getCurrentTimestampFunc() const;

    /**
     * @brief 获取自增 ID 函数
     * 
     * @return MySQL: LAST_INSERT_ID()，PostgreSQL: LASTVAL()
     */
    std::string getLastInsertIdFunc() const;

    /**
     * @brief 获取字符串拼接函数
     * 
     * @param a 第一个字符串
     * @param b 第二个字符串
     * @return MySQL: CONCAT(a, b)，PostgreSQL: a || b
     */
    std::string getConcatFunc(const std::string& a, const std::string& b) const;

    /**
     * @brief 获取分页 SQL 片段
     * 
     * @param limit 每页记录数
     * @param offset 偏移量
     * @return MySQL: LIMIT offset, limit，PostgreSQL: LIMIT limit OFFSET offset
     */
    std::string getLimitOffsetClause(int limit, int offset) const;

private:
    DatabaseAdapter() = default;
    ~DatabaseAdapter() = default;

    // 禁止拷贝和移动
    DatabaseAdapter(const DatabaseAdapter&) = delete;
    DatabaseAdapter& operator=(const DatabaseAdapter&) = delete;

    DatabaseType _type = DatabaseType::POSTGRESQL;  // 默认 PostgreSQL
    std::string _host;
    int _port = 0;
    std::string _dbname;
    std::string _user;
    std::string _passwd;
};

}  // namespace ruoyi
