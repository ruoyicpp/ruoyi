# Task Queue Integration Guide

任务队列模块集成指南 - 如何将异步任务队列集成到若依 C++ 项目中

## 📋 集成步骤

### 1. 更新 CMakeLists.txt

在主 `CMakeLists.txt` 中添加任务队列模块：

```cmake
# 添加任务队列模块
add_subdirectory(src/taskqueue)
```

### 2. 更新 AppIncludes.h

在 `src/AppIncludes.h` 中添加任务队列的包含：

```cpp
// ── 任务队列 ────────────────────────────────────────────────
#include "taskqueue/TaskQueue.h"
#include "taskqueue/TaskQueueCtrl.h"
#include "taskqueue/TaskQueueExample.h"
```

### 3. 更新 config.json

在 `config.json` 中添加任务队列配置：

```json
{
  "app": {
    "name": "RuoYi-Cpp",
    "version": "1.0.0"
  },
  "listeners": [...],
  "database": {...},
  "redis": {...},
  
  "taskQueue": {
    "enabled": true,
    "workers": 4,
    "pollInterval": 1000,
    "maxRetries": 3,
    "retryBackoff": 60,
    "taskTimeout": 300000,
    "queueTypes": ["email", "export", "process", "notification"]
  }
}
```

### 4. 在 main.cc 中初始化

在 `src/main.cc` 的 `main()` 函数中添加初始化代码：

```cpp
#include "taskqueue/TaskQueue.h"
#include "taskqueue/TaskQueueExample.h"

int main(int argc, char* argv[]) {
    // ... 其他初始化代码 ...

    // 加载配置
    drogon::app().loadConfigFile(configFile);
    
    // ── 初始化任务队列 ──────────────────────────────────────
    {
        std::ifstream cf(configFile);
        Json::Value root;
        Json::CharReaderBuilder rb;
        std::string errs;
        if (cf.is_open() && Json::parseFromStream(rb, cf, &root, &errs)) {
            if (root.isMember("taskQueue")) {
                // 初始化任务队列
                TaskQueue::instance().init(root["taskQueue"]);
                
                // 注册任务处理器
                TaskQueue::instance().registerHandler("email", EmailTaskHandler::handle);
                TaskQueue::instance().registerHandler("export", ExportTaskHandler::handle);
                TaskQueue::instance().registerHandler("process", FileProcessTaskHandler::handle);
                TaskQueue::instance().registerHandler("notification", NotificationTaskHandler::handle);
                
                // 启动 Worker 线程
                TaskQueue::instance().start();
                
                LOG_INFO << "[Main] Task queue initialized and started";
            }
        }
    }

    // ... 其他初始化代码 ...

    // 应用运行
    drogon::app().run();

    // ── 关闭任务队列 ────────────────────────────────────────
    TaskQueue::instance().stop();
    LOG_INFO << "[Main] Task queue stopped";

    return 0;
}
```

### 5. 在业务代码中使用

#### 发送邮件（异步）

```cpp
// 在 SysEmailConfigCtrl 或其他地方
void sendEmail(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto to = req->getParameter("to");
    auto subject = req->getParameter("subject");
    auto body = req->getParameter("body");
    
    // 异步发送邮件
    EmailTaskHandler::enqueueEmail(to, subject, body);
    
    RESP_MSG(cb, "Email queued for sending");
}
```

#### 导出数据（异步）

```cpp
// 在 SysUserCtrl 或其他地方
void exportUsers(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    long userId = GET_USER_ID(req);
    
    // 异步导出用户数据
    ExportTaskHandler::enqueueExport(
        "csv",
        "SELECT * FROM sys_user",
        "users_" + std::to_string(std::time(nullptr)) + ".csv",
        userId
    );
    
    RESP_MSG(cb, "Export task queued");
}
```

#### 处理文件（异步）

```cpp
// 在文件上传处理中
void uploadImage(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto file = req->getUploadFile("image");
    std::string filepath = "./uploads/" + file->getFilename();
    
    // 保存原始文件
    file->save(filepath);
    
    // 异步生成缩略图
    FileProcessTaskHandler::enqueueResize(
        filepath,
        "./uploads/thumb_" + file->getFilename(),
        200, 200
    );
    
    RESP_MSG(cb, "Image uploaded, thumbnail generation queued");
}
```

#### 发送通知（异步）

```cpp
// 在告警或事件处理中
void sendAlert(const std::string& title, const std::string& content) {
    // 发送到钉钉
    NotificationTaskHandler::enqueueNotification(
        "dingtalk",
        "https://oapi.dingtalk.com/robot/send?access_token=xxx",
        title,
        content
    );
    
    // 发送到企业微信
    NotificationTaskHandler::enqueueNotification(
        "wecom",
        "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=xxx",
        title,
        content
    );
}
```

## 🔧 自定义任务处理器

### 创建新的任务处理器

```cpp
// 在 taskqueue/TaskQueueExample.h 中添加

class CustomTaskHandler {
public:
    static bool handle(const TaskQueue::Task& task) {
        try {
            // 从 task.payload 获取参数
            auto param1 = task.payload["param1"].asString();
            auto param2 = task.payload["param2"].asInt();
            
            // 执行业务逻辑
            bool success = doSomething(param1, param2);
            
            if (success) {
                LOG_INFO << "[CustomTask] Task completed: " << task.id;
            } else {
                LOG_WARN << "[CustomTask] Task failed: " << task.id;
            }
            
            return success;
        } catch (const std::exception& e) {
            LOG_ERROR << "[CustomTask] Error: " << e.what();
            return false;
        }
    }
    
    static void enqueueTask(const std::string& param1, int param2) {
        TaskQueue::Task task;
        task.type = "custom";
        task.payload["param1"] = param1;
        task.payload["param2"] = param2;
        task.maxRetries = 5;  // 自定义重试次数
        
        TaskQueue::instance().enqueue(task);
    }

private:
    static bool doSomething(const std::string& p1, int p2) {
        // 实现业务逻辑
        return true;
    }
};
```

### 注册自定义处理器

在 `main.cc` 中注册：

```cpp
// 注册自定义任务处理器
TaskQueue::instance().registerHandler("custom", CustomTaskHandler::handle);
```

### 在 config.json 中添加队列类型

```json
{
  "taskQueue": {
    "enabled": true,
    "workers": 4,
    "queueTypes": ["email", "export", "process", "notification", "custom"]
  }
}
```

## 📊 监控和管理

### 查看队列统计

```bash
curl http://localhost:8080/monitor/taskqueue/stats
```

响应示例：
```json
{
  "email": {
    "pending": 10,
    "processing": 2,
    "success": 1000,
    "failed": 5,
    "deadLetter": 2,
    "total": 1019
  },
  "export": {...},
  "process": {...},
  "notification": {...}
}
```

### 查看任务详情

```bash
curl http://localhost:8080/monitor/taskqueue/task/email_20240101_001
```

### 查看任务执行日志

```bash
curl http://localhost:8080/monitor/taskqueue/task/email_20240101_001/log
```

### 重新入队失败的任务

```bash
curl -X POST http://localhost:8080/monitor/taskqueue/requeue/email_20240101_001
```

### 清空死信队列

```bash
curl -X DELETE http://localhost:8080/monitor/taskqueue/deadletter/email
```

## 🛡️ 权限配置

在数据库中添加权限记录：

```sql
-- 任务队列权限
INSERT INTO sys_permission (perm_code, perm_name, resource, action) VALUES
('monitor:taskqueue:list', '任务队列查看', '/monitor/taskqueue/stats', 'GET'),
('monitor:taskqueue:detail', '任务详情查看', '/monitor/taskqueue/task/*', 'GET'),
('monitor:taskqueue:requeue', '任务重新入队', '/monitor/taskqueue/requeue/*', 'POST'),
('monitor:taskqueue:clear', '清空死信队列', '/monitor/taskqueue/deadletter/*', 'DELETE');

-- 为管理员角色添加权限
INSERT INTO sys_role_permission (role_id, perm_id) 
SELECT r.role_id, p.perm_id FROM sys_role r, sys_permission p
WHERE r.role_name = 'admin' AND p.perm_code LIKE 'monitor:taskqueue:%';
```

## 🔗 Redis 集成

当前实现使用注释代码作为占位符。要使用真实 Redis，需要：

### 1. 添加 Redis 客户端库

在 `CMakeLists.txt` 中添加：

```cmake
find_package(redis++ REQUIRED)
target_link_libraries(ruoyi-cpp PRIVATE redis++)
```

### 2. 更新 TaskQueue.cc

替换注释代码为真实 Redis 操作：

```cpp
#include <sw/redis++/redis.h>

using namespace sw::redis;

void TaskQueue::enqueue(const Task& task) {
    auto redis = Redis("tcp://127.0.0.1:6379");
    
    Task t = task;
    if (t.id.empty()) {
        t.id = generateTaskId(t.type);
    }
    if (t.createdAt == 0) {
        t.createdAt = std::time(nullptr);
    }
    t.status = TaskStatus::PENDING;

    // 存储任务详情
    auto taskKey = getTaskKey(t.id);
    Json::StreamWriterBuilder wb;
    std::string taskJson = Json::writeString(wb, t.toJson());
    redis.hset(taskKey, "data", taskJson);
    redis.expire(taskKey, 86400);  // 24 hours TTL

    // 添加到队列
    auto queueKey = getQueueKey(t.type);
    redis.lpush(queueKey, t.id);

    LOG_INFO << "[TaskQueue] Task enqueued: " << t.id;
}
```

## 📈 性能调优

### 调整 Worker 数量

根据 CPU 核心数和任务类型调整：

```json
{
  "taskQueue": {
    "workers": 8  // 增加 Worker 数量以提高吞吐
  }
}
```

### 调整轮询间隔

```json
{
  "taskQueue": {
    "pollInterval": 500  // 减少轮询间隔以降低延迟
  }
}
```

### 调整重试策略

```json
{
  "taskQueue": {
    "maxRetries": 5,      // 增加重试次数
    "retryBackoff": 120   // 增加重试间隔
  }
}
```

## 🐛 故障排查

### 任务未被处理

1. 检查任务队列是否启用：`config.json` 中 `enabled: true`
2. 检查 Worker 线程是否启动：查看日志中的 "Worker threads started"
3. 检查任务处理器是否注册：确保 `registerHandler` 被调用
4. 检查 Redis 连接：确保 Redis 服务运行正常

### 任务一直重试

1. 检查任务处理器的实现：确保返回正确的 bool 值
2. 检查日志：查看具体的错误信息
3. 检查重试策略：调整 `maxRetries` 和 `retryBackoff`
4. 考虑将任务移到 DLQ：手动处理失败的任务

### 内存占用过高

1. 定期清理过期任务
2. 减少任务的 TTL
3. 清空死信队列：`DELETE /monitor/taskqueue/deadletter/{type}`
4. 调整 Worker 数量和轮询间隔

## 📚 参考资源

- TaskQueue.h - 核心接口
- TaskQueue.cc - 实现细节
- TaskQueueExample.h - 使用示例
- TaskQueueCtrl.h - 管理 API
- README.md - 完整文档

