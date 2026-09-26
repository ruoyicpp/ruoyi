/**
 * @file DbConnStr.cc
 * @brief 构造 libpq (PostgreSQL) 连接串
 *
 * 从原 main.cc 中抽出，两个重载：
 *   - 文件路径版本（兼容旧调用方式）
 *   - ConfigLoader 版本（推荐，支持 Vault 密码补全）
 */

#include "AppBootstrap.h"

/**
 * @brief 从 config.json 的 database 节构造 libpq 连接串
 *
 * 根据配置文件中的数据库参数生成 PostgreSQL 连接字符串。
 * 兼容旧版本的调用方式。
 *
 * @param cfgFile 配置文件路径（通常为 "config.json"）
 * @param timeout 连接超时时间（秒，默认 5）
 *
 * @return 格式为 "host=... port=... dbname=... user=... password=... connect_timeout=..."
 *         的 libpq 连接字符串，如果配置文件不存在或格式错误返回空字符串
 *
 * @note
 *   - 如果 database 节不存在，返回空字符串
 *   - 密码为空时也会包含在连接字符串中
 *   - 此函数为兼容性函数，新代码应使用 ConfigLoader 版本
 *
 * @see buildDbConnStr(const ConfigLoader&, int) - ConfigLoader 版本
 */
std::string buildDbConnStr(const std::string& cfgFile, int timeout) {
    std::ifstream f(cfgFile);
    if (!f.is_open()) return {};
    Json::Value root; Json::CharReaderBuilder rb; std::string errs;
    if (!Json::parseFromStream(rb, f, &root, &errs)) return {};
    if (!root.isMember("database")) return {};
    auto& d = root["database"];
    return "host="     + d.get("host",   "127.0.0.1").asString()
         + " port="   + std::to_string(d.get("port", 5432).asInt())
         + " dbname=" + d.get("dbname", "ruoyi").asString()
         + " user="   + d.get("user",   "postgres").asString()
         + " password=" + d.get("passwd", "").asString()
         + " connect_timeout=" + std::to_string(timeout);
}

/**
 * @brief 从 ConfigLoader 构造 libpq 连接串（推荐版本）
 *
 * 使用 ConfigLoader 从配置文件和 Vault 中读取数据库参数，
 * 生成 PostgreSQL 连接字符串。
 *
 * @param loader ConfigLoader 实例，包含配置和 Vault 集成
 * @param timeout 连接超时时间（秒，默认 5）
 *
 * @return 格式为 "host=... port=... dbname=... user=... password=... connect_timeout=..."
 *         的 libpq 连接字符串
 *
 * @note
 *   - 如果 database.passwd 为空，会从 Vault 中补全（如果配置了 Vault）
 *   - 使用 ConfigLoader 的 get() 方法，支持环境变量覆盖
 *   - 推荐在新代码中使用此版本而不是文件路径版本
 *
 * @see buildDbConnStr(const std::string&, int) - 文件路径版本（已弃用）
 * @see ConfigLoader - 配置加载器
 */
std::string buildDbConnStr(const ConfigLoader& loader, int timeout) {
    auto& d = loader.raw()["database"];
    return "host="     + loader.get("database", "host",   "127.0.0.1")
         + " port="   + std::to_string(d.get("port", 5432).asInt())
         + " dbname=" + loader.get("database", "dbname", "ruoyi")
         + " user="   + loader.get("database", "user",   "postgres")
         + " password=" + loader.get("database", "passwd", "")
         + " connect_timeout=" + std::to_string(timeout);
}
