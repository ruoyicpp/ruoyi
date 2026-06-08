# 插件系统使用指南

## 概述

插件系统允许用户在**不修改主程序、不重启服务**的情况下，通过放置一个 DLL 文件来扩展系统功能（自定义 API 路由、通知渠道、AI 模型供应商等）。

---

## 一、快速上手（5 分钟）

### 1.1 创建插件目录

在项目根目录的 `plugins/` 下新建插件目录，目录名即为插件名称：

```
plugins/
└── my_plugin/              ← 插件目录（名称自定）
    ├── plugin.json         ← 插件清单（必需）
    ├── my_plugin.dll       ← 编译产物（必需，Windows）
    └── frontend/           ← 前端资源（可选）
        └── index.js
```

### 1.2 编写 plugin.json

```json
{
    "name": "my_plugin",
    "version": "1.0.0",
    "description": "我的第一个插件",
    "type": "route",
    "greeting": "你好"
}
```

| 字段 | 必需 | 说明 |
|------|------|------|
| `name` | ✅ | 插件名称，必须与目录名一致 |
| `version` | ✅ | 版本号 |
| `description` | ❌ | 描述 |
| `type` | ❌ | 类型：`route`（默认）/ `notify` / `ai` / `menu` |

### 1.3 编写 C++ 代码

创建 `plugin.h`：

```cpp
#pragma once
#include "../../src/libs/plugin/IPlugin.h"
#include <drogon/HttpController.h>
#include <drogon/HttpResponse.h>

class MyPluginCtrl : public drogon::HttpController<MyPluginCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MyPluginCtrl::hello, "/plugin/my_plugin/hello", drogon::Get, "JwtAuthFilter");
    METHOD_LIST_END
    void hello(const drogon::HttpRequestPtr&,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};

class MyPlugin : public ruoyi::plugin::IPlugin {
public:
    std::string name()     const override { return "my_plugin"; }
    std::string version()  const override { return "1.0.0"; }
    std::string description() const override { return "我的第一个插件"; }
    PluginType type() const override { return PluginType::Route; }

    void onLoad(const Json& config) override;
    void registerRoutes() override;
    std::string frontendEntry() const override { return "index.js"; }
};
```

创建 `plugin.cc`：

```cpp
#include "plugin.h"
#include <ctime>

using Json = nlohmann::json;

void MyPluginCtrl::hello(const drogon::HttpRequestPtr&,
                         std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    Json r;
    r["msg"]  = "Hello from my_plugin!";
    r["time"] = static_cast<long>(std::time(nullptr));
    cb(drogon::HttpResponse::newHttpJsonResponse(r));
}

void MyPlugin::onLoad(const Json& config) {
    // 从 plugin.json 读取自定义配置
    if (config.is_object() && config.contains("greeting")) {
        // ...
    }
}

void MyPlugin::registerRoutes() {
    // 可以在这里用 app().registerHandler() 注册额外路由
}

extern "C" {
    RUOYI_PLUGIN_API ruoyi::plugin::IPlugin* createPlugin() {
        return new MyPlugin();
    }
    RUOYI_PLUGIN_API void destroyPlugin(ruoyi::plugin::IPlugin* p) {
        delete p;
    }
}
```

### 1.4 编译 DLL

```bash
# 在插件目录下创建编译目录
mkdir build-my_plugin && cd build-my_plugin

# 配置（请根据实际路径修改）
cmake -DPLUGIN_NAME=my_plugin \
      -DPLUGIN_SOURCE_DIR=../my_plugin \
      -DDROGON_INSTALL_PREFIX=g:/back/recovered/drogon-master/need/install-static \
      -DVCPKG_INSTALLED=g:/back/recovered/drogon-master/vcpkg-master/installed/x64-mingw-static \
      -DMSYS_PREFIX=H:/msys64/msys64/ucrt64 \
      ..

# 编译
cmake --build . --parallel

# 编译完成后 DLL 自动输出到 plugins/my_plugin/my_plugin.dll
```

> 编译脚本参考 `plugins/hello_plugin/CMakeLists.txt`，需要确保路径与主程序一致。

### 1.5 启用插件

**方式 A：通过管理后台**

1. 打开系统管理后台 → 插件管理
2. 页面自动显示 `GET /plugin/discover` 发现的所有插件
3. 点击"发现"列表中的插件 → 点击"启用"

**方式 B：通过 API**

```bash
# 1. 查看已发现的插件
curl -X GET http://localhost:8848/plugin/discover \
     -H "Authorization: Bearer <token>"

# 返回：发现 my_plugin 但 status=discovered

# 2. 启用插件（热加载）
curl -X POST http://localhost:8848/plugin/register/my_plugin \
     -H "Authorization: Bearer <token>"

# 返回 code=200 表示加载成功

# 3. 调用插件 API
curl http://localhost:8848/plugin/my_plugin/hello \
     -H "Authorization: Bearer <token>"
# {"msg":"Hello from my_plugin!","time":1749000000}
```

---

## 二、API 参考

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/plugin/discover` | 扫描 `plugins/` 目录，返回所有发现的插件（含未加载的） |
| POST | `/plugin/register/{name}` | **热加载**指定插件，DLL 加载后路由立即生效 |
| GET | `/plugin/list` | 列出已加载的插件 |
| DELETE | `/plugin/{name}` | 卸载插件 |
| GET | `/plugin/{name}` | 插件详情（名称/版本/菜单） |
| GET | `/plugin/{name}/menus` | 插件贡献的前端菜单（可合并到系统菜单） |
| GET | `/plugin/{name}/frontend/{*path}` | 插件前端静态资源（供前端动态 import） |

---

## 三、前端集成

### 3.1 动态加载插件前端 JS

插件 DLL 加载后，前端可以直接动态 import 插件的 JS：

```javascript
// 方式 A：运行时动态加载（推荐，无需重构建前端）
const mod = await import('/plugin/my_plugin/frontend/index.js');
mod.install?.(app);

// 方式 B：前端构建时配置 extraPluginsDir（需重构建）
```

### 3.2 注册前端菜单

插件启用后，前端获取插件菜单并合并到路由表：

```javascript
// 获取插件贡献的菜单
const menus = await fetch('/plugin/my_plugin/menus', {
    headers: { 'Authorization': 'Bearer ' + token }
}).then(r => r.json());

// 合并到 vue-router
menus.forEach(menu => {
    router.addRoute('Layout', {
        path: menu.path,
        component: () => import(`/plugin/${pluginName}/frontend/${menu.component}`)
    });
});
```

---

## 四、热重载

插件支持热更新，步骤：

1. 编译新版 DLL，替换 `plugins/{name}/{name}.dll`
2. 调用卸载接口：`DELETE /plugin/{name}`
3. 调用启用接口：`POST /plugin/register/{name}`

路由立即生效，无需重启主程序。

---

## 五、插件类型说明

| 类型 | 说明 | 关键方法 |
|------|------|----------|
| `Route` | 自定义 API 路由 | `registerRoutes()` |
| `NotifyChannel` | 通知渠道（钉钉/飞书等） | `buildRequest()` |
| `AiProvider` | AI 模型供应商 | `chat()` |
| `Menu` | 前端菜单/页面 | `getMenus()` |

---

## 六、示例插件

参考 `plugins/hello_plugin/` 完整示例：

```
plugins/hello_plugin/
├── plugin.json          ← 元数据
├── plugin.h            ← 接口定义
├── plugin.cc           ← 实现 + DLL 工厂
├── CMakeLists.txt      ← 编译脚本
└── frontend/
    └── index.js        ← 前端入口
```

编译并启用后，访问：
- API：`GET /plugin/hello/greet`
- 菜单：`/hello`（前端插件示例）

---

## 七、常见问题

**Q: DLL 放到目录后访问 404？**
A: DLL 放进去后需要先调用 `POST /plugin/register/{name}` 启用，系统不会自动加载。

**Q: 编译报错找不到 drogon？**
A: 检查 CMakeLists.txt 中的 `DROGON_INSTALL_PREFIX`、`VCPKG_INSTALLED` 路径是否正确指向主程序编译产物。

**Q: 插件路由 404？**
A: 确认 `plugin.h` 中 `METHOD_LIST_BEGIN/END` 声明了路由，且 DLL 加载成功（查看服务端日志 `[Plugin] Loaded:`）。

**Q: 如何传配置给插件？**
A: 在 `plugin.json` 中添加自定义字段，`onLoad(const Json& config)` 可读取。
