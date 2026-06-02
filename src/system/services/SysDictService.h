/**
 * @file SysDictService.h
 * @brief 系统字典服务 — 管理系统字典数据和缓存
 * 
 * 功能概述：
 *   - 字典管理：查询、缓存系统字典数据
 *   - 字典缓存：支持内存缓存加速字典查询
 *   - 字典加载：应用启动时加载所有字典到缓存
 *   - 字典更新：支持动态更新字典缓存
 * 
 * 核心特性：
 *   - 双层缓存：内存缓存（DictCache）+ 数据库持久化（sys_dict_type, sys_dict_data）
 *   - 线程安全：使用 std::mutex 保护缓存并发访问
 *   - 快速查询：缓存命中时 O(1) 查询，无数据库开销
 * 
 * 常用字典类型：
 *   - sys_user_sex: 用户性别（0=男, 1=女, 2=未知）
 *   - sys_normal_disable: 正常/禁用（0=正常, 1=禁用）
 *   - sys_yes_no: 是/否（Y=是, N=否）
 * 
 * 数据库表：
 *   - sys_dict_type: 字典类型表
 *   - sys_dict_data: 字典数据表
 */

#pragma once
#include <json/json.h>
#include <string>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include "../../common/Constants.h"
#include "../../common/TokenCache.h"
#include "../../services/DatabaseService.h"

/**
 * @class DictCache
 * @brief 字典内存缓存单例
 * 
 * 对应 RuoYi.Net 中的 DictUtils，提供字典数据的内存缓存。
 * 采用单例模式，全局唯一实例。
 * 
 * 使用 std::unordered_map 存储字典数据，std::mutex 保护并发访问。
 */
class DictCache {
public:
    static DictCache &instance() {
        static DictCache inst;
        return inst;
    }

    /**
     * @brief 设置字典缓存
     * 
     * @param dictType 字典类型（如 "sys_user_sex"）
     * @param data 字典数据（JSON 数组）
     */
    void set(const std::string &dictType, const Json::Value &data) {
        std::lock_guard<std::mutex> lock(mutex_);
        store_[dictType] = data;
    }

    /**
     * @brief 获取字典缓存
     * 
     * @param dictType 字典类型
     * @return 字典数据（JSON 数组），如果不存在返回空数组
     */
    Json::Value get(const std::string &dictType) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(dictType);
        if (it != store_.end()) return it->second;
        return Json::Value(Json::arrayValue);
    }

    /**
     * @brief 删除指定字典缓存
     * 
     * @param dictType 字典类型
     */
    void remove(const std::string &dictType) {
        std::lock_guard<std::mutex> lock(mutex_);
        store_.erase(dictType);
    }

    /**
     * @brief 清空所有字典缓存
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        store_.clear();
    }
private:
    std::unordered_map<std::string, Json::Value> store_;  ///< 字典缓存存储
    std::mutex mutex_;                                     ///< 并发访问保护
};

/**
 * @class SysDictService
 * @brief 系统字典服务单例
 * 
 * 对应 RuoYi.Net 中的 SysDictTypeService + SysDictDataService，
 * 处理字典数据的查询和缓存。
 * 采用单例模式，全局唯一实例。
 * 
 * 使用 libpq 直接查询 PostgreSQL 数据库，支持字典缓存和动态更新。
 */
class SysDictService {
public:
    static SysDictService &instance() {
        static SysDictService inst;
        return inst;
    }

    void loadDictCache() {
        auto& db = DatabaseService::instance();
        auto res = db.query(
            "SELECT dict_type, dict_label, dict_value, dict_sort, "
            "css_class, list_class, is_default, status "
            "FROM sys_dict_data WHERE status='0' ORDER BY dict_sort");
        if (!res.ok()) {
            std::cerr << "[SysDictService] 加载字典缓存失败" << std::endl;
            return;
        }
        std::unordered_map<std::string, Json::Value> tmp;
        for (int i = 0; i < res.rows(); ++i) {
            std::string dt = res.str(i, 0);
            Json::Value item;
            item["dictLabel"]  = res.str(i, 1);
            item["dictValue"]  = res.str(i, 2);
            item["dictSort"]   = res.intVal(i, 3);
            item["cssClass"]   = res.str(i, 4);
            item["listClass"]  = res.str(i, 5);
            item["isDefault"]  = res.str(i, 6);
            tmp[dt].append(item);
        }
        for (auto &[k, v] : tmp) DictCache::instance().set(k, v);
        std::cout << "[SysDictService] 字典缓存加载完成" << std::endl;
    }

    void resetDictCache() {
        DictCache::instance().clear();
        loadDictCache();
    }

    Json::Value selectDictDataByType(const std::string &dictType) {
        auto cached = DictCache::instance().get(dictType);
        if (!cached.empty()) return cached;

        auto& db = DatabaseService::instance();
        auto res = db.queryParams(
            "SELECT dict_label, dict_value, dict_sort, css_class, list_class, is_default "
            "FROM sys_dict_data WHERE dict_type=$1 AND status='0' ORDER BY dict_sort",
            {dictType});
        Json::Value arr(Json::arrayValue);
        if (res.ok()) {
            for (int i = 0; i < res.rows(); ++i) {
                Json::Value item;
                item["dictLabel"]  = res.str(i, 0);
                item["dictValue"]  = res.str(i, 1);
                item["dictSort"]   = res.intVal(i, 2);
                item["cssClass"]   = res.str(i, 3);
                item["listClass"]  = res.str(i, 4);
                item["isDefault"]  = res.str(i, 5);
                arr.append(item);
            }
            DictCache::instance().set(dictType, arr);
        }
        return arr;
    }
};
