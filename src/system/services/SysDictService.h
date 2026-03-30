#pragma once
#include <json/json.h>
#include <string>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include "../../common/Constants.h"
#include "../../common/TokenCache.h"
#include "../../services/DatabaseService.h"

// �ֵ��ڴ滺�棨��Ӧ RuoYi.Net DictUtils��
class DictCache {
public:
    static DictCache &instance() {
        static DictCache inst;
        return inst;
    }
    void set(const std::string &dictType, const Json::Value &data) {
        std::lock_guard<std::mutex> lock(mutex_);
        store_[dictType] = data;
    }
    Json::Value get(const std::string &dictType) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(dictType);
        if (it != store_.end()) return it->second;
        return Json::Value(Json::arrayValue);
    }
    void remove(const std::string &dictType) {
        std::lock_guard<std::mutex> lock(mutex_);
        store_.erase(dictType);
    }
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        store_.clear();
    }
private:
    std::unordered_map<std::string, Json::Value> store_;
    std::mutex mutex_;
};

// ��Ӧ RuoYi.Net SysDictTypeService + SysDictDataService��ʹ������ libpq ������
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
            std::cerr << "[SysDictService] �����ֵ仺��ʧ��" << std::endl;
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
        std::cout << "[SysDictService] �ֵ仺��������" << std::endl;
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
