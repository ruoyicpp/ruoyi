#include "PluginManager.h"
#include <drogon/HttpAppFramework.h>
#include <filesystem>
#include <fstream>
#include <set>
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

namespace {
class RequestGuard {
public:
    explicit RequestGuard(const std::shared_ptr<RouteRuntimeState>& state)
        : state_(state), engaged_(false) {
        if (!state_) return;
        state_->activeRequests.fetch_add(1, std::memory_order_acq_rel);
        engaged_ = true;
    }

    ~RequestGuard() {
        if (!engaged_ || !state_) return;
        const auto remaining = state_->activeRequests.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (remaining == 0) {
            std::lock_guard<std::mutex> lock(state_->drainMutex);
            state_->drainCv.notify_all();
        }
    }

private:
    std::shared_ptr<RouteRuntimeState> state_;
    bool engaged_;
};

void respondPluginUnavailable(const std::string& pluginName,
                              PluginResponseCallback&& cb) {
    nlohmann::json body;
    body["code"] = 410;
    body["msg"] = "plugin unloaded: " + pluginName;
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k410Gone);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(body.dump());
    cb(resp);
}
}

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

nlohmann::json PluginManager::menuToJson(const MenuItem& menu) {
    nlohmann::json jm;
    jm["path"] = menu.path;
    jm["name"] = menu.name;
    jm["component"] = menu.component;
    jm["icon"] = menu.icon;
    jm["menuType"] = menu.menuType;
    jm["parentId"] = menu.parentId;
    jm["meta"] = menu.meta;
    return jm;
}

std::string PluginManager::findDllPath(const std::string& name) const {
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

void PluginManager::registerManagedRoutes(const std::string& pluginName,
                                          const std::shared_ptr<LoadedPlugin>& loaded) {
    auto routes = loaded->instance->routes();
    for (auto& route : routes) {
        std::vector<drogon::internal::HttpConstraint> constraints;
        constraints.reserve(route.methods.size());
        for (const auto method : route.methods) {
            constraints.emplace_back(method);
        }
        auto state = loaded->runtimeState;
        auto handler = route.handler;
        drogon::app().registerHandler(
            route.path,
            [pluginName, state, handler](const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& cb) mutable {
                if (!state || !state->accepting.load(std::memory_order_acquire)) {
                    return respondPluginUnavailable(pluginName, std::move(cb));
                }

                RequestGuard guard(state);
                if (!state->accepting.load(std::memory_order_acquire)) {
                    return respondPluginUnavailable(pluginName, std::move(cb));
                }

                handler(req, std::move(cb));
            },
            constraints);
        LOG_INFO << "[Plugin] Managed route registered: " << pluginName << " path=" << route.path;
    }
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

    auto create = reinterpret_cast<IPlugin* (*)()>(getSym(handle, "createPlugin"));
    auto destroy = reinterpret_cast<void (*)(IPlugin*)>(getSym(handle, "destroyPlugin"));

    if (!create) {
        freeLib(handle);
        throw std::runtime_error("createPlugin not found in " + dllPath);
    }

    IPlugin* instance = create();
    if (!instance) {
        freeLib(handle);
        throw std::runtime_error("createPlugin() returned nullptr: " + dllPath);
    }

    fs::path pluginDir = fs::path("plugins") / name;
    fs::path jsonPath = pluginDir / "plugin.json";
    nlohmann::json cfg = nlohmann::json::object();
    if (fs::exists(jsonPath)) {
        std::ifstream f(jsonPath);
        if (f.is_open()) f >> cfg;
    }

    try { instance->onLoad(cfg); } catch (const std::exception& e) {
        LOG_WARN << "[Plugin] onLoad exception in " << name << ": " << e.what();
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
    loaded->runtimeState = std::make_shared<RouteRuntimeState>();
    loaded->menus = instance->getMenus();

    if (instance->type() == PluginType::Route) {
        try {
            registerManagedRoutes(name, loaded);
            instance->registerRoutes();
            LOG_INFO << "[Plugin] Routes registered: " << name;
        } catch (const std::exception& e) {
            loaded->runtimeState->accepting.store(false, std::memory_order_release);
            loaded->destroyFn(instance);
            freeLib(handle);
            throw std::runtime_error("registerRoutes failed for " + name + ": " + e.what());
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    plugins_[name] = loaded;

    LOG_INFO << "[Plugin] routers cache should be refreshed after load: " << name;
    LOG_INFO << "[Plugin] Loaded: " << name << " v" << desc.version
             << " type=" << static_cast<int>(desc.type)
             << " from " << dllPath;
    return plugins_.at(name)->descriptor;
}

std::vector<std::string> PluginManager::autoLoadFromConfig(const Json::Value& root) {
    std::vector<std::string> loadedNames;
    if (!root.isObject() || !root.isMember("plugins") || !root["plugins"].isObject()) {
        return loadedNames;
    }

    const auto& cfg = root["plugins"];
    const bool enabled = cfg.get("enabled", false).asBool();
    if (!enabled) {
        LOG_INFO << "[Plugin] auto-load disabled by config";
        return loadedNames;
    }

    const bool startupLoad = cfg.get("startup_load", false).asBool();
    nlohmann::json discovered;
    discoverPlugins(discovered);
    LOG_INFO << "[Plugin] discovered " << discovered.size()
             << " plugin(s), startup_load=" << startupLoad;

    if (!startupLoad) {
        LOG_WARN << "[Plugin] startup DLL loading is disabled; use /plugin/register/{name} for hot activation";
        return loadedNames;
    }

    std::set<std::string> targetNames;
    const bool autoDiscover = cfg.get("auto_discover", true).asBool();
    if (autoDiscover) {
        for (auto it = discovered.begin(); it != discovered.end(); ++it) {
            targetNames.insert(it.key());
        }
    }

    if (cfg.isMember("autoload") && cfg["autoload"].isArray()) {
        for (const auto& item : cfg["autoload"]) {
            if (item.isString()) {
                targetNames.insert(item.asString());
            }
        }
    }

    std::set<std::string> disabledNames;
    if (cfg.isMember("disabled") && cfg["disabled"].isArray()) {
        for (const auto& item : cfg["disabled"]) {
            if (item.isString()) {
                disabledNames.insert(item.asString());
            }
        }
    }

    for (const auto& itemName : targetNames) {
        if (itemName.empty() || disabledNames.count(itemName) > 0) {
            continue;
        }
        try {
            load(itemName);
            loadedNames.push_back(itemName);
        } catch (const std::exception& e) {
            LOG_ERROR << "[Plugin] auto-load failed for " << itemName << ": " << e.what();
        }
    }

    return loadedNames;
}

void PluginManager::unload(const std::string& name) {
    std::shared_ptr<LoadedPlugin> loaded;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = plugins_.find(name);
        if (it == plugins_.end()) return;
        loaded = it->second;
        plugins_.erase(it);
    }

    if (loaded->runtimeState) {
        loaded->runtimeState->accepting.store(false, std::memory_order_release);
        std::unique_lock<std::mutex> waitLock(loaded->runtimeState->drainMutex);
        loaded->runtimeState->drainCv.wait_for(
            waitLock,
            std::chrono::seconds(10),
            [&loaded] {
                return loaded->runtimeState->activeRequests.load(std::memory_order_acquire) == 0;
            });
    }

    if (loaded->instance) {
        try { loaded->instance->onUnload(); } catch (...) {}
        loaded->destroyFn(loaded->instance);
        loaded->instance = nullptr;
    }
    if (loaded->handle) {
        freeLib(loaded->handle);
        loaded->handle = nullptr;
    }
    LOG_INFO << "[Plugin] routers cache should be refreshed after unload: " << name;
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
        info["name"] = p->descriptor.name;
        info["version"] = p->descriptor.version;
        info["description"] = p->descriptor.description;
        info["type"] = static_cast<int>(p->descriptor.type);
        info["frontendEntry"] = p->descriptor.frontendEntry;
        info["menuCount"] = p->menus.size();
        out[name] = info;
    }
}

void PluginManager::listAllMenusJson(nlohmann::json& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    out = nlohmann::json::array();
    for (const auto& [pluginName, loaded] : plugins_) {
        for (const auto& menu : loaded->menus) {
            auto jm = menuToJson(menu);
            jm["pluginName"] = pluginName;
            out.push_back(std::move(jm));
        }
    }
}

bool PluginManager::getPluginMenusJson(const std::string& pluginName, nlohmann::json& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(pluginName);
    if (it == plugins_.end()) {
        out = nlohmann::json::array();
        return false;
    }

    out = nlohmann::json::array();
    for (const auto& menu : it->second->menus) {
        out.push_back(menuToJson(menu));
    }
    return true;
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

        auto jsonPath = entry.path() / "plugin.json";
        auto dllPath = entry.path() / (name + ".dll");
#ifdef _WIN32
        if (!fs::exists(jsonPath) && !fs::exists(dllPath)) continue;
#else
        auto soPath = entry.path() / ("lib" + name + ".so");
        if (!fs::exists(jsonPath) && !fs::exists(soPath)) continue;
#endif

        nlohmann::json info = nlohmann::json::object();
        info["name"] = name;
        info["status"] = "discovered";
        if (fs::exists(jsonPath)) {
            std::ifstream f(jsonPath);
            if (f.is_open()) {
                try {
                    nlohmann::json j; f >> j;
                    info["version"] = j.value("version", "1.0.0");
                    info["description"] = j.value("description", "");
                    info["type"] = j.value("type", "route");
                    info["hasDll"] = fs::exists(dllPath);
                    info["hasFrontend"] = fs::is_directory(entry.path() / "frontend");
                } catch (...) {}
            }
        } else {
            info["version"] = "unknown";
            info["description"] = "(no plugin.json)";
            info["type"] = "route";
            info["hasDll"] = true;
        }

        auto loadedIt = plugins_.find(name);
        if (loadedIt != plugins_.end()) {
            info["status"] = "loaded";
            info["menuCount"] = loadedIt->second->menus.size();
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

    out["name"] = name;
    out["hasDll"] = dllExists;
    out["hasFrontend"] = fs::is_directory(pluginDir / "frontend");
    out["status"] = plugins_.count(name) ? "loaded" : "discovered";

    auto loadedIt = plugins_.find(name);
    if (loadedIt != plugins_.end()) {
        out["menuCount"] = loadedIt->second->menus.size();
    }

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
