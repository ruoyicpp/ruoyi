# Task Queue Quick Start Guide

任务队列快速开始指南

## 🚀 5 分钟快速开始

### 1. 配置 config.json

```json
{
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

### 2. 在 main.cc 中初始化

```cpp
#include "taskqueue/TaskQueue.h"
#include "taskqueue/TaskQueueExample.h"

int main(int argc, char* argv[]) {
    // ... 其他代码 ...
    
    // 加载配置
    drogon::app().loadConfigFile(configFile);
    
    // 初始化任务队列
    std::ifstream cf(configFile);
    Json::Value root;
    Json::CharReaderBuilder rb;
    std::string errs;
    if (cf.is_open() && Json::parseFromStream(rb, cf, &root, &errs)) {
        if (root.isMember("taskQueue")) {
            TaskQueue::instance().init(root["taskQueue"]);
            
            // 注册处理器
            TaskQueue::instance().registerHandler("email", EmailTaskHandler::handle);
            TaskQueue::instance().registerHandler("export", ExportTaskHandler::handle);
            TaskQueue::instance().registerHandler("process", FileProcessTaskHandler::handle);
            TaskQueue::instance().registerHandler("notification", NotificationTaskHandler::handle);
            
            // 启动
            TaskQueue::instance().start();
            LOG_INFO << "Task queue started";
        }
    }
    
    drogon::app().run();
    
    // 关闭
    TaskQueue::instance().stop();
    
    return 0;
}
```

### 3. 在业务代码中使用

```cpp
// 发送邮件
EmailTaskHandler::enqueueEmail("user@example.com", "Subject", "Body");

// 导出数据
ExportTaskHandler::enqueueExport("csv", "SELECT * FROM sys_user", "export.csv", userId);

// 处理文件
FileProcessTaskHandler::enqueueResize("input.jpg", "output.jpg", 200, 200);

// 发送通知
NotificationTaskHandler::enqueueNotification("dingtalk", "webhook_url", "Title", "Content");
```

## 📊 API 端点

```bash
# 查看统计
curl http://localhost:8080/monitor/taskqueue/stats

# 查看任务
curl http://localhost:8080/monitor/taskqueue/task/email_20240101_001

# 查看日志
curl http://localhost:8080/monitor/taskqueue/task/email_20240101_001/log

# 重新入队
curl -X POST http://localhost:8080/monitor/taskqueue/requeue/email_20240101_001

# 清空 DLQ
curl -X DELETE http://localhost:8080/monitor/taskqueue/deadletter/email
```

## 🎯 常见场景

### 场景 1：异步发送邮件

```cpp
// 在用户注册时
void registerUser(const drogon::HttpRequestPtr &req,
                  std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    auto email = req->getParameter("email");
    
    // 保存用户
    // ... 数据库操作 ...
    
    // 异步发送欢迎邮件
    EmailTaskHandler::enqueueEmail(
        email,
        "Welcome to RuoYi",
        "Thank you for registering!"
    );
    
    RESP_MSG(cb, "User registered, welcome email sent");
}
```

### 场景 2：异步导出数据

```cpp
// 在数据导出时
void exportUsers(const drogon::HttpRequestPtr &req,
                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
    long userId = GET_USER_ID(req);
    std::string filename = "users_" + std::to_string(std::time(nullptr)) + ".csv";
    
    // 异步导出
    ExportTaskHandler::enqueueExport(
        "csv",
        "SELECT * FROM sys_user",
        filename,
        userId
    );
    
    RESP_MSG(cb, "Export task queued, you will receive email when ready");
}
```

### 场景 3：异步处理上传的文件

```cpp
// 在图片上传时
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

### 场景 4：异步发送告警通知

```cpp
// 在系统告警时
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

```cpp
// 1. 定义处理器
class MyTaskHandler {
public:
    static bool handle(const TaskQueue::Task& task) {
        try {
            auto param = task.payload["param"].asString();
            // 执行业务逻辑
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR << "Error: " << e.what();
            return false;
        }
    }
    
    static void enqueueTask(const std::string& param) {
        TaskQueue::Task task;
        task.type = "mytype";
        task.payload["param"] = param;
        TaskQueue::instance().enqueue(task);
    }
};

// 2. 注册处理器（在 main.cc 中）
TaskQueue::instance().registerHandler("mytype", MyTaskHandler::handle);

// 3. 在 config.json 中添加队列类型
{
  "taskQueue": {
    "queueTypes": ["email", "export", "process", "notification", "mytype"]
  }
}

// 4. 使用
MyTaskHandler::enqueueTask("some parameter");
```

## 📈 监控和调试

### 查看队列深度

```bash
curl http://localhost:8080/monitor/taskqueue/stats | jq
```

### 查看失败的任务

```bash
curl http://localhost:8080/monitor/taskqueue/stats | jq '.email.deadLetter'
```

### 查看任务执行日志

```bash
curl http://localhost:8080/monitor/taskqueue/task/email_20240101_001/log
```

### 手动恢复失败的任务

```bash
curl -X POST http://localhost:8080/monitor/taskqueue/requeue/email_20240101_001
```

## ⚙️ 性能调优

### 增加 Worker 数量（提高吞吐）

```json
{
  "taskQueue": {
    "workers": 8
  }
}
```

### 减少轮询间隔（降低延迟）

```json
{
  "taskQueue": {
    "pollInterval": 500
  }
}
```

### 增加重试次数（提高可靠性）

```json
{
  "taskQueue": {
    "maxRetries": 5,
    "retryBackoff": 120
  }
}
```

## 🐛 故障排查

| 问题 | 原因 | 解决方案 |
|------|------|--------|
| 任务未被处理 | 队列未启用 | 检查 config.json 中 enabled: true |
| 任务一直重试 | 处理器返回 false | 检查处理器实现，查看日志 |
| 内存占用高 | 任务堆积 | 清空 DLQ，调整 Worker 数量 |
| 延迟高 | 轮询间隔太大 | 减少 pollInterval |

## 📚 文件说明

| 文件 | 说明 |
|------|------|
| TaskQueue.h | 核心接口定义 |
| TaskQueue.cc | 实现代码 |
| TaskQueueCtrl.h | 管理 API |
| TaskQueueExample.h | 使用示例 |
| README.md | 完整文档 |
| INTEGRATION.md | 集成指南 |
| QUICKSTART.md | 本文件 |

## 🔗 相关资源

- [README.md](README.md) - 完整功能说明
- [INTEGRATION.md](INTEGRATION.md) - 详细集成步骤
- [TaskQueue.h](TaskQueue.h) - API 文档
- [TaskQueueExample.h](TaskQueueExample.h) - 代码示例

