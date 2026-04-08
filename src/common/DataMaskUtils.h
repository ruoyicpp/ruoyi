#pragma once
#include <string>
#include <json/json.h>

// 数据脱敏工具：手机、身份证、银行卡、邮箱、姓名
struct DataMaskUtils {
    // 手机号：138****8888
    static std::string maskPhone(const std::string &s) {
        if (s.size() < 7) return s;
        return s.substr(0, 3) + "****" + s.substr(s.size() - 4);
    }

    // 身份证：110***********1234
    static std::string maskIdCard(const std::string &s) {
        if (s.size() < 8) return s;
        return s.substr(0, 3) + std::string(s.size() - 7, '*') + s.substr(s.size() - 4);
    }

    // 银行卡：6222***********1234
    static std::string maskBankCard(const std::string &s) {
        if (s.size() < 8) return s;
        return s.substr(0, 4) + std::string(s.size() - 8, '*') + s.substr(s.size() - 4);
    }

    // 邮箱：ab***@qq.com
    static std::string maskEmail(const std::string &s) {
        auto at = s.find('@');
        if (at == std::string::npos || at < 2) return s;
        return s.substr(0, 2) + std::string(at - 2, '*') + s.substr(at);
    }

    // 姓名：张*  /  张*三
    static std::string maskName(const std::string &s) {
        // 按 UTF-8 中文字符计算（每个汉字3字节）
        size_t len = 0;
        for (size_t i = 0; i < s.size(); ) {
            unsigned char c = s[i];
            if (c < 0x80) { ++i; } else if (c < 0xE0) { i += 2; } else { i += 3; }
            ++len;
        }
        if (len <= 1) return s;
        // 保留首字，中间全部替换为*
        size_t firstEnd = 0;
        { unsigned char c = (unsigned char)s[0]; firstEnd = c < 0x80 ? 1 : c < 0xE0 ? 2 : 3; }
        std::string result = s.substr(0, firstEnd);
        for (size_t i = 1; i < len - 1; ++i) result += '*';
        if (len > 2) {
            size_t lastStart = s.size();
            { size_t i = firstEnd; for (size_t k = 1; k < len - 1; ++k) { unsigned char c=(unsigned char)s[i]; i += c<0x80?1:c<0xE0?2:3; } lastStart = i; }
            result += s.substr(lastStart);
        }
        return result;
    }

    // 自动识别并脱敏 JSON 对象中的敏感字段
    static void maskJsonValue(Json::Value &obj) {
        if (!obj.isObject()) return;
        static const std::vector<std::string> phoneKeys   = {"phonenumber","phone","mobile","tel"};
        static const std::vector<std::string> emailKeys   = {"email","mail"};
        static const std::vector<std::string> idcardKeys  = {"idcard","identitycard","cardno"};
        static const std::vector<std::string> bankcardKeys = {"bankcard","cardnumber","accountno"};
        static const std::vector<std::string> nameKeys    = {"username","realname","name","nickname"};

        auto match = [](const std::string &key, const std::vector<std::string> &list) {
            std::string k = key;
            for (auto &c : k) c = ::tolower(c);
            for (auto &p : list) if (k.find(p) != std::string::npos) return true;
            return false;
        };

        for (auto &key : obj.getMemberNames()) {
            if (!obj[key].isString()) { maskJsonValue(obj[key]); continue; }
            std::string val = obj[key].asString();
            if      (match(key, phoneKeys))    obj[key] = maskPhone(val);
            else if (match(key, emailKeys))    obj[key] = maskEmail(val);
            else if (match(key, idcardKeys))   obj[key] = maskIdCard(val);
            else if (match(key, bankcardKeys)) obj[key] = maskBankCard(val);
            else if (match(key, nameKeys))     obj[key] = maskName(val);
        }
    }
};
