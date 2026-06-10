/**
 * @file IotCtrl.h
 * @brief IoT 设备管理控制器 — 提供 Modbus TCP 设备管理和数据读写
 * 
 * 功能概述：
 *   - 设备管理：注册、列表、删除、连通性测试
 *   - Modbus 读写：支持读寄存器、线圈、线圈组
 *   - 批量轮询：一次请求批量读取多个地址
 *   - 连接管理：自动连接复用、超时控制
 * 
 * API 端点：
 *   - GET    /iot/devices              - 列出所有设备
 *   - POST   /iot/devices              - 注册新设备
 *   - DELETE /iot/devices/{id}         - 删除设备
 *   - GET    /iot/devices/{id}/ping    - 连通性测试
 *   - POST   /iot/modbus/read          - 读寄存器/线圈
 *   - POST   /iot/modbus/write         - 写寄存器/线圈
 *   - POST   /iot/modbus/poll          - 批量轮询多个地址
 * 
 * 权限要求：
 *   - 所有接口都需要 JWT 认证
 *   - 需要 iot:device:list、iot:device:add、iot:device:delete 权限
 * 
 * 使用示例：
 *   // 注册设备
 *   POST /iot/devices
 *   {
 *     "id": "device_001",
 *     "name": "温度传感器",
 *     "host": "192.168.1.100",
 *     "port": 502,
 *     "unitId": 1,
 *     "timeoutMs": 2000,
 *     "description": "车间温度监测"
 *   }
 *   
 *   // 读寄存器
 *   POST /iot/modbus/read
 *   {
 *     "deviceId": "device_001",
 *     "functionCode": 3,
 *     "startAddress": 0,
 *     "quantity": 10
 *   }
 * 
 * @see ModbusGateway - Modbus TCP 网关
 * @see DatabaseService - 数据库服务
 */

#pragma once
#include <drogon/drogon.h>
#include "ModbusGateway.h"
#include "../common/AjaxResult.h"
#include "../services/DatabaseService.h"

/**
 * @class IotCtrl
 * @brief IoT 设备管理控制器
 * 
 * 提供 Modbus TCP 设备的管理和数据读写功能。
 * 支持多个设备的并发操作和批量数据读取。
 */
class IotCtrl : public drogon::HttpController<IotCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(IotCtrl::listDevices,  "/iot/devices",          drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(IotCtrl::addDevice,    "/iot/devices",          drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(IotCtrl::delDevice,    "/iot/devices/{id}",     drogon::Delete, "JwtAuthFilter");
        ADD_METHOD_TO(IotCtrl::pingDevice,   "/iot/devices/{id}/ping",drogon::Get,    "JwtAuthFilter");
        ADD_METHOD_TO(IotCtrl::modbusRead,   "/iot/modbus/read",      drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(IotCtrl::modbusWrite,  "/iot/modbus/write",     drogon::Post,   "JwtAuthFilter");
        ADD_METHOD_TO(IotCtrl::modbusPoll,   "/iot/modbus/poll",      drogon::Post,   "JwtAuthFilter");
    METHOD_LIST_END

    /**
     * @brief 列出所有设备
     * 
     * GET /iot/devices
     * 
     * @param req HTTP 请求
     * @param cb 回调函数
     * @return 设备列表 JSON 数组
     */
    void listDevices(const drogon::HttpRequestPtr&,
                     std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        Json::Value arr(Json::arrayValue);
        for (auto &d : ModbusGateway::instance().listDevices()) {
            Json::Value o;
            o["id"]          = d.id;
            o["name"]        = d.name;
            o["host"]        = d.host;
            o["port"]        = d.port;
            o["unitId"]      = d.unitId;
            o["description"] = d.description;
            arr.append(o);
        }
        RESP_OK(cb, arr);
    }

    // POST /iot/devices
    void addDevice(const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body)["id"].isString() || !(*body)["host"].isString())
            { RESP_ERR(cb, "缺少 id / host"); return; }
        ModbusGateway::Device dev;
        dev.id          = (*body)["id"].asString();
        dev.name        = (*body).get("name", dev.id).asString();
        dev.host        = (*body)["host"].asString();
        dev.port        = (*body).get("port", 502).asInt();
        dev.unitId      = (uint8_t)(*body).get("unitId", 1).asInt();
        dev.timeoutMs   = (*body).get("timeoutMs", 2000).asInt();
        dev.description = (*body).get("description", "").asString();
        ModbusGateway::instance().addDevice(dev);
        // 持久化到数据库
        auto &db = DatabaseService::instance();
        db.execParams(
            "INSERT INTO iot_device(id,name,host,port,unit_id,timeout_ms,description)"
            " VALUES($1,$2,$3,$4,$5,$6,$7)"
            " ON CONFLICT(id) DO UPDATE SET name=$2,host=$3,port=$4,unit_id=$5,timeout_ms=$6,description=$7",
            {dev.id, dev.name, dev.host, std::to_string(dev.port),
             std::to_string(dev.unitId), std::to_string(dev.timeoutMs), dev.description});
        RESP_MSG(cb, "设备注册成功");
    }

    // DELETE /iot/devices/:id
    void delDevice(const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr&)> &&cb,
                   std::string id) {
        ModbusGateway::instance().removeDevice(id);
        DatabaseService::instance().execParams(
            "DELETE FROM iot_device WHERE id=$1", {id});
        RESP_MSG(cb, "已删除");
    }

    // GET /iot/devices/:id/ping
    void pingDevice(const drogon::HttpRequestPtr&,
                    std::function<void(const drogon::HttpResponsePtr&)> &&cb,
                    std::string id) {
        bool ok = ModbusGateway::instance().ping(id);
        Json::Value r; r["online"] = ok; r["deviceId"] = id;
        RESP_OK(cb, r);
    }

    // POST /iot/modbus/read
    // body: {deviceId, type:"holding"|"input"|"coil"|"discrete", startAddr, count}
    void modbusRead(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "invalid JSON"); return; }
        std::string devId  = (*body).get("deviceId", "").asString();
        std::string type   = (*body).get("type", "holding").asString();
        uint16_t startAddr = (uint16_t)(*body).get("startAddr", 0).asInt();
        uint16_t count     = (uint16_t)(*body).get("count", 1).asInt();
        if (count > 125) { RESP_ERR(cb, "count 不能超过 125"); return; }
        try {
            Json::Value r;
            r["deviceId"]  = devId;
            r["type"]      = type;
            r["startAddr"] = startAddr;
            Json::Value vals(Json::arrayValue);
            if (type == "holding") {
                for (auto v : ModbusGateway::instance().readHoldingRegisters(devId, startAddr, count))
                    vals.append(v);
            } else if (type == "input") {
                for (auto v : ModbusGateway::instance().readInputRegisters(devId, startAddr, count))
                    vals.append(v);
            } else if (type == "coil") {
                for (bool v : ModbusGateway::instance().readCoils(devId, startAddr, count))
                    vals.append((bool)v);
            } else if (type == "discrete") {
                for (bool v : ModbusGateway::instance().readDiscreteInputs(devId, startAddr, count))
                    vals.append((bool)v);
            } else { RESP_ERR(cb, "未知类型: " + type); return; }
            r["values"] = vals;
            r["count"]  = (int)vals.size();
            RESP_OK(cb, r);
        } catch (const std::exception &e) { RESP_ERR(cb, e.what()); }
    }

    // POST /iot/modbus/write
    // body: {deviceId, type:"register"|"coil", addr, value} 或 {values:[...]}
    void modbusWrite(const drogon::HttpRequestPtr &req,
                     std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body) { RESP_ERR(cb, "invalid JSON"); return; }
        std::string devId = (*body).get("deviceId", "").asString();
        std::string type  = (*body).get("type", "register").asString();
        uint16_t addr     = (uint16_t)(*body).get("addr", 0).asInt();
        try {
            if (type == "register") {
                if ((*body)["values"].isArray()) {
                    std::vector<uint16_t> vals;
                    for (auto &v : (*body)["values"]) vals.push_back((uint16_t)v.asInt());
                    ModbusGateway::instance().writeMultipleRegisters(devId, addr, vals);
                } else {
                    uint16_t val = (uint16_t)(*body).get("value", 0).asInt();
                    ModbusGateway::instance().writeSingleRegister(devId, addr, val);
                }
            } else if (type == "coil") {
                bool on = (*body).get("value", false).asBool();
                ModbusGateway::instance().writeSingleCoil(devId, addr, on);
            } else { RESP_ERR(cb, "未知类型: " + type); return; }
            RESP_MSG(cb, "写入成功");
        } catch (const std::exception &e) { RESP_ERR(cb, e.what()); }
    }

    // POST /iot/modbus/poll — 批量读多个地址
    // body: {deviceId, registers:[{addr, count, type, label},...]}
    void modbusPoll(const drogon::HttpRequestPtr &req,
                    std::function<void(const drogon::HttpResponsePtr&)> &&cb) {
        auto body = req->getJsonObject();
        if (!body || !(*body)["registers"].isArray())
            { RESP_ERR(cb, "缺少 registers 数组"); return; }
        std::string devId = (*body).get("deviceId", "").asString();
        Json::Value results(Json::arrayValue);
        for (auto &item : (*body)["registers"]) {
            uint16_t addr  = (uint16_t)item.get("addr", 0).asInt();
            uint16_t cnt   = (uint16_t)item.get("count", 1).asInt();
            std::string tp = item.get("type", "holding").asString();
            std::string lb = item.get("label", "").asString();
            Json::Value row; row["addr"] = addr; row["label"] = lb; row["type"] = tp;
            try {
                Json::Value vals(Json::arrayValue);
                if (tp == "holding")
                    for (auto v : ModbusGateway::instance().readHoldingRegisters(devId, addr, cnt))
                        vals.append(v);
                else if (tp == "input")
                    for (auto v : ModbusGateway::instance().readInputRegisters(devId, addr, cnt))
                        vals.append(v);
                row["values"] = vals;
                row["ok"] = true;
            } catch (const std::exception &e) {
                row["ok"]    = false;
                row["error"] = e.what();
            }
            results.append(row);
        }
        RESP_OK(cb, results);
    }

    // 启动时调用：从 DB 加载已存储设备到内存
    static void loadFromDb() {
        auto &db = DatabaseService::instance();
        // 建表（幂等）
        db.exec(
            "CREATE TABLE IF NOT EXISTS iot_device("
            " id VARCHAR(64) PRIMARY KEY,"
            " name VARCHAR(128),"
            " host VARCHAR(128) NOT NULL,"
            " port INTEGER DEFAULT 502,"
            " unit_id SMALLINT DEFAULT 1,"
            " timeout_ms INTEGER DEFAULT 2000,"
            " description VARCHAR(256)"
            ")");
        auto r = db.query("SELECT id,name,host,port,unit_id,timeout_ms,description FROM iot_device");
        if (!r.ok()) return;
        int loaded = 0;
        for (int i = 0; i < r.rows(); ++i) {
            ModbusGateway::Device dev;
            dev.id          = r.str(i, 0);
            dev.name        = r.str(i, 1);
            dev.host        = r.str(i, 2);
            dev.port        = r.intVal(i, 3);
            dev.unitId      = (uint8_t)r.intVal(i, 4);
            dev.timeoutMs   = r.intVal(i, 5);
            dev.description = r.str(i, 6);
            ModbusGateway::instance().addDevice(dev);
            ++loaded;
        }
        if (loaded > 0)
            std::cout << "[IoT] loaded " << loaded << " device(s) from DB" << std::endl;
    }
};
