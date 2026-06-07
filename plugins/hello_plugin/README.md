# Hello Plugin 示例插件

`hello_plugin` 是 RuoYi-Cpp 项目的动态加载插件示例，演示了如何通过 C++ 动态链接库（DLL/SO）和前端 JS 组件，实现无需重启主程序、无需重新打包前端的“热插拔”式模块扩展。

---

## 📦 目录结构

```
hello_plugin/
├── CMakeLists.txt      # 插件独立构建脚本
├── plugin.json         # 插件配置文件（元数据、路由类型、初始配置等）
├── plugin.h            # 插件控制器与主类声明
├── plugin.cc           # 插件控制器逻辑、生命周期及导出工厂函数
├── frontend/
│   └── index.js        # 前端动态注入与交互入口
└── README.md           # 本文档
```

---

## 🛠️ 核心机制与接口

插件主类 `HelloPlugin` 继承自 `ruoyi::plugin::IPlugin`，并实现了以下核心接口：

### 1. 插件生命周期与元数据
* **`name()`**: 返回插件的唯一标识符（`hello_plugin`）。
* **`version()`**: 返回版本号。
* **`onLoad(const Json& config)`**: 插件加载时的初始化回调。可读取 `plugin.json` 中的自定义配置（如 `greeting`）。

### 2. 路由与接口注册
* **`registerRoutes()`**: 
  * 插件在加载时，主程序会自动调用此接口。
  * 可通过 `drogon::app().registerHandler()` 注册无鉴权的简单测试路由（如 `/plugin/hello/health`）。
  * 核心业务 API 建议写在 `HelloPluginCtrl` 中，并通过 `ADD_METHOD_TO` 注册，支持挂载 `JwtAuthFilter` 等全局鉴权拦截器。

### 3. 动态菜单挂载
* **`getMenus()`**:
  返回插件想要向系统路由表贡献的菜单项。主程序加载插件后，前端可通过 API 获取这些菜单并动态合并到前端路由表中。

---

## 💻 编写插件 C++ 代码

### 声明类与导出接口
在 `plugin.h` 和 `plugin.cc` 中，必须声明并实现 `createPlugin` 和 `destroyPlugin` 的 `extern "C"` 工厂函数，供主程序 `dlopen` / `LoadLibrary` 发现：

```cpp
extern "C" {
    RUOYI_PLUGIN_API ruoyi::plugin::IPlugin* createPlugin() {
        return new HelloPlugin();
    }
    RUOYI_PLUGIN_API void destroyPlugin(ruoyi::plugin::IPlugin* p) {
        delete p;
    }
}
```

---

## 🌐 前端集成 (`frontend/index.js`)

前端通过运行时动态导入（ES Modules `import()`）来加载插件前端逻辑，无须重新构建主前端项目：

```javascript
// 1. 动态加载前端组件并初始化
const mod = await import('/plugin/hello_plugin/frontend/index.js');
mod.install?.(app);

// 2. 运行时激活后端 DLL
await fetch('/plugin/register/hello_plugin', { method: 'POST' });

// 3. 获取插件动态路由/菜单并合并
const menus = await fetch('/plugin/hello_plugin/menus').then(r => r.json());
menus.forEach(m => router.addRoute(m));
```

---

## 🔨 编译与构建

你可以使用 `CMake` 独立编译该插件为动态链接库：

### 1. 构建命令
在插件根目录或指定目录执行：

```powershell
mkdir build-hello
cd build-hello

# 配置 CMake（请根据实际路径调整依赖项路径）
cmake -DPLUGIN_NAME=hello_plugin `
      -DPLUGIN_SOURCE_DIR=../plugins/hello_plugin `
      -DDROGON_INSTALL_PREFIX=g:/back/recovered/drogon-master/need/install-static `
      -DVCPKG_INSTALLED=g:/back/recovered/drogon-master/vcpkg-master/installed/x64-mingw-static `
      -DMSYS_PREFIX=H:/msys64/msys64/ucrt64 `
      -DCMAKE_BUILD_TYPE=Release ..

# 执行编译
cmake --build . --parallel
```

### 2. 输出产物
编译成功后，将在 `plugins/hello_plugin/` 目录下生成：
* **Windows**: `hello_plugin.dll` 以及相关的依赖运行时 DLL（如 `libjsoncpp.dll`、`libssl` 等）。
* **Linux**: `libhello_plugin.so`。

主程序启动后，会自动扫描并加载此目录下的插件。
