#include "PluginManager.h"
#include <drogon/HttpAppFramework.h>
#include <filesystem>
#include <fstream>
#include <trantor/utils/Logger.h>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace fs = std::filesystem;
namespace dru = drogon;

namespace ruoyi {
namespace plugin {

void* loadLib(const std::string& path) {
#ifdef _WIN32
    SetThreadErrorMode(SEM_NOALIGNMENTFAULTEXCEPT, nullptr);
    return LoadLibraryExA(path.c_str(), nullptr,
                          LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
                          | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
}

void freeLib(void* handle) {
    if (!handle) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* getSym(void* handle, const char* sym) {
    if (!handle) return nullptr;
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(
        static_cast<HMODULE>(handle), sym));
#else
    return dlsym(handle, sym);
#endif
}

std::string PluginManager::findDllPath(const std::string& name) const {
    // 搜索路径: plugins/{name}/{name}.dll  或  plugins/{name}/lib{name}.so
    const fs::path base = "plugins";
    std::vector<fs::path> candidates;
#ifdef _WIN32
    candidates.push_back(base / name / (name + ".dll"));
#else
    candidates.push_back(base / name / ("lib" + name + ".so"));
    candidates.push_back(base / name / (name + ".so"));
#endif
    for (const auto& p : candidates) {
        if (fs::exists(p)) return p.string();
    }
    // fallback: 扫描 plugins/{name}/ 下所有 dll/so，找第一个
    auto pluginDir = base / name;
    if (fs::exists(pluginDir)) {
        for (const auto& entry : fs::directory_iterator(pluginDir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
#ifdef _WIN32
            if (ext == ".dll") return entry.path().string();
#else
            if (ext == ".so")  return entry.path().string();
#endif
        }
    }
    return "";
}

const PluginDescriptor& PluginManager::load(const std::string& name) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (plugins_.count(name)) {
            return plugins_.at(name)->descriptor;
        }
    }

    auto dllPath = findDllPath(name);
    if (dllPath.empty()) {
        throw std::runtime_error("plugin not found: " + name
            + " (searched: plugins/" + name + "/{name}.dll|.so)");
    }

    void* handle = loadLib(dllPath);
    if (!handle) {
        throw std::runtime_error("failed to load " + dllPath + ": " + dlError());
    }

    auto create = reinterpret_cast<IPlugin* (*)()>(
        getSym(handle, "createPlugin"));
    auto destroy = reinterpret_cast<void (*)(IPlugin*)>(
        getSym(handle, "destroyPlugin"));

    if (!create) {
        freeLib(handle);
        throw std::runtime_error("createPlugin not found in " + dllPath);
    }

    IPlugin* instance = create();
    if (!instance) {
        freeLib(handle);
        throw std::runtime_error("createPlugin() returned nullptr: " + dllPath);
    }

    // 读 plugin.json（可选）
    fs::path pluginDir = fs::path("plugins") / name;
    fs::path jsonPath = pluginDir / "plugin.json";
    nlohmann::json cfg = nlohmann::json::object();
    if (fs::exists(jsonPath)) {
        std::ifstream f(jsonPath);
        if (f.is_open()) f >> cfg;
    }

    // 回调 onLoad
    try { instance->onLoad(cfg); } catch (const std::exception& e) {
        LOG_WARN << "[Plugin] onLoad exception in " << name << ": " << e.what();
    }

    // 注册路由
    if (instance->type() == PluginType::Route) {
        try {
            instance->registerRoutes();
            LOG_INFO << "[Plugin] Routes registered: " << name;
        } catch (const std::exception& e) {
            LOG_ERROR << "[Plugin] registerRoutes failed for " << name << ": " << e.what();
        }
    }

    PluginDescriptor desc;
    desc.name = instance->name();
    desc.version = instance->version();
    desc.description = instance->description();
    desc.type = instance->type();
    desc.frontendEntry = instance->frontendEntry();

    auto loaded = std::make_shared<LoadedPlugin>();
    loaded->dllPath = dllPath;
    loaded->pluginDir = pluginDir.string();
    loaded->handle = handle;
    loaded->instance = instance;
    loaded->descriptor = desc;
    loaded->destroyFn = destroy ? destroy : [](IPlugin* p){ delete p; };

    std::lock_guard<std::mutex> lock(mutex_);
    plugins_[name] = loaded;

    LOG_INFO << "[Plugin] Loaded: " << name << " v" << desc.version
             << " type=" << static_cast<int>(desc.type)
             << " from " << dllPath;
    return plugins_.at(name)->descriptor;
}

void PluginManager::unload(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(name);
    if (it == plugins_.end()) return;

    auto& p = it->second;
    if (p->instance) {
        try { p->instance->onUnload(); } catch (...) {}
        p->destroyFn(p->instance);
    }
    if (p->handle) freeLib(p->handle);
    plugins_.erase(it);
    LOG_INFO << "[Plugin] Unloaded: " << name;
}

bool PluginManager::isLoaded(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return plugins_.count(name) > 0;
}

const PluginDescriptor* PluginManager::getDescriptor(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(name);
    return it != plugins_.end() ? &it->second->descriptor : nullptr;
}

IPlugin* PluginManager::getPlugin(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(name);
    return it != plugins_.end() ? it->second->instance : nullptr;
}

std::vector<std::string> PluginManager::listLoaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (auto& [n, _] : plugins_) names.push_back(n);
    return names;
}

size_t PluginManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return plugins_.size();
}

bool PluginManager::getFrontendPath(const std::string& pluginName,
                                    const std::string& relativePath,
                                    std::string& outFilePath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(pluginName);
    if (it == plugins_.end()) return false;

    fs::path p = fs::path(it->second->pluginDir) / "frontend" / relativePath;
    if (!fs::exists(p) || !fs::is_regular_file(p)) return false;
    outFilePath = p.string();
    return true;
}

void PluginManager::listPluginsJson(nlohmann::json& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    out = nlohmann::json::object();
    for (auto& [name, p] : plugins_) {
        nlohmann::json info = nlohmann::json::object();
        info["name"]           = p->descriptor.name;
        info["version"]        = p->descriptor.version;
        info["description"]    = p->descriptor.description;
        info["type"]           = static_cast<int>(p->descriptor.type);
        info["frontendEntry"]  = p->descriptor.frontendEntry;
        out[name] = info;
    }
}

void PluginManager::discoverPlugins(nlohmann::json& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    out = nlohmann::json::object();

    fs::path base = "plugins";
    if (!fs::exists(base) || !fs::is_directory(base)) return;

    for (const auto& entry : fs::directory_iterator(base)) {
        if (!entry.is_directory()) continue;
        auto name = entry.path().filename().string();
        if (name == "." || name == "..") continue;

        // 检查是否有 plugin.json 或同名 dll/so
        auto jsonPath = entry.path() / "plugin.json";
        auto dllPath = entry.path() / (name + ".dll");
#ifdef _WIN32
        if (!fs::exists(jsonPath) && !fs::exists(dllPath)) continue;
#else
        auto soPath = entry.path() / ("lib" + name + ".so");
        if (!fs::exists(jsonPath) && !fs::exists(soPath)) continue;
#endif

        // 读取 plugin.json
        nlohmann::json info = nlohmann::json::object();
        info["name"]   = name;
        info["status"] = "discovered";  // discovered | loaded
        if (fs::exists(jsonPath)) {
            std::ifstream f(jsonPath);
            if (f.is_open()) {
                try {
                    nlohmann::json j; f >> j;
                    info["version"]     = j.value("version", "1.0.0");
                    info["description"] = j.value("description", "");
                    info["type"]        = j.value("type", "route");
                    info["hasDll"]      = fs::exists(dllPath);
                    info["hasFrontend"] = fs::is_directory(entry.path() / "frontend");
                } catch (...) {}
            }
        } else {
            info["version"] = "unknown";
            info["description"] = "(no plugin.json)";
            info["type"] = "route";
            info["hasDll"] = true;
        }

        // 标记是否已加载
        if (plugins_.count(name)) {
            info["status"] = "loaded";
        }

        discovered_[name] = info;
        out[name] = info;
    }
}

void PluginManager::discoverOne(const std::string& name, nlohmann::json& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    out = nlohmann::json::object();

    fs::path pluginDir = fs::path("plugins") / name;
    if (!fs::exists(pluginDir)) {
        out["error"] = "plugin directory not found: " + name;
        return;
    }

    auto jsonPath = pluginDir / "plugin.json";
    auto dllPath = pluginDir / (name + ".dll");
#ifdef _WIN32
    bool dllExists = fs::exists(dllPath);
#else
    bool dllExists = fs::exists(pluginDir / ("lib" + name + ".so"));
#endif

    out["name"]       = name;
    out["hasDll"]     = dllExists;
    out["hasFrontend"] = fs::is_directory(pluginDir / "frontend");
    out["status"]     = plugins_.count(name) ? "loaded" : "discovered";

    if (fs::exists(jsonPath)) {
        std::ifstream f(jsonPath);
        if (f.is_open()) {
            try {
                nlohmann::json j;
                f >> j;
                for (auto& [k, v] : j.items())
                    out[k] = v;
            } catch (...) {}
        }
    }
}

std::vector<std::string> PluginManager::listDiscovered() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (auto& [name, _] : discovered_) names.push_back(name);
    return names;
}

PluginManager& PluginManager::instance() {
    static PluginManager inst;
    return inst;
}

std::string PluginManager::dlError() const {
#ifdef _WIN32
    char* msgBuf = nullptr;
    DWORD rc = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&msgBuf), 0, nullptr);
    std::string result;
    if (rc && msgBuf) { result = msgBuf; LocalFree(msgBuf); }
    return result.empty() ? "Unknown error" : result;
#else
    const char* e = dlerror();
    return e ? e : "Unknown error";
#endif
}

}}
