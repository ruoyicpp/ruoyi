#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>
#include <vector>
#include <iostream>
#include <atomic>
#include <fstream>
#include <json/json.h>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

// VramCache: GPU 显存缓存层
// 通过动态加载 CUDA runtime（cudart.dll / libcudart.so）实现，
// 无编译期 CUDA 依赖，自动检测可用性。
// 优先级：Redis > VramCache > MemCache(RAM)
class VramCache {
public:
    static VramCache& instance() {
        static VramCache inst;
        return inst;
    }

    bool available() const { return cuAvail_; }

    std::string backendInfo() const {
        if (!cuAvail_) return "unavailable";
        return "cuda(device:" + std::to_string(devIdx_) + ")";
    }

    void setString(const std::string& key, const std::string& val, int expireSeconds = 0) {
        if (!cuAvail_ || val.empty()) return;
        std::lock_guard<std::mutex> lk(mtx_);
        evictEntry(key);
        void* devPtr = nullptr;
        if (pfnMalloc_(&devPtr, val.size()) != 0 || !devPtr) return;
        if (pfnMemcpy_(devPtr, val.data(), val.size(), kHostToDevice) != 0) {
            pfnFree_(devPtr); return;
        }
        Entry e;
        e.devPtr    = devPtr;
        e.size      = val.size();
        e.hasExpire = expireSeconds > 0;
        if (e.hasExpire)
            e.expireAt = std::chrono::steady_clock::now() + std::chrono::seconds(expireSeconds);
        idx_[key] = e;
    }

    std::optional<std::string> getString(const std::string& key) {
        if (!cuAvail_) return std::nullopt;
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = idx_.find(key);
        if (it == idx_.end()) return std::nullopt;
        if (isExpired(it->second)) {
            pfnFree_(it->second.devPtr);
            idx_.erase(it);
            return std::nullopt;
        }
        std::string out(it->second.size, '\0');
        if (pfnMemcpy_(out.data(), it->second.devPtr, it->second.size, kDeviceToHost) != 0)
            return std::nullopt;
        return out;
    }

    void remove(const std::string& key) {
        if (!cuAvail_) return;
        std::lock_guard<std::mutex> lk(mtx_);
        evictEntry(key);
    }

    void removeByPrefix(const std::string& prefix) {
        if (!cuAvail_) return;
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto it = idx_.begin(); it != idx_.end(); ) {
            if (it->first.rfind(prefix, 0) == 0) {
                pfnFree_(it->second.devPtr);
                it = idx_.erase(it);
            } else ++it;
        }
    }

    std::vector<std::string> getKeysByPrefix(const std::string& prefix) {
        std::vector<std::string> result;
        if (!cuAvail_) return result;
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto& kv : idx_) {
            if (kv.first.rfind(prefix, 0) == 0)
                result.push_back(kv.first);
        }
        return result;
    }

    void clear() {
        if (!cuAvail_) return;
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto& kv : idx_) pfnFree_(kv.second.devPtr);
        idx_.clear();
    }

    size_t size() {
        if (!cuAvail_) return 0;
        std::lock_guard<std::mutex> lk(mtx_);
        return idx_.size();
    }

    // 返回当前显存占用字节数
    size_t bytesUsed() {
        if (!cuAvail_) return 0;
        std::lock_guard<std::mutex> lk(mtx_);
        size_t total = 0;
        for (auto& kv : idx_) total += kv.second.size;
        return total;
    }

private:
    VramCache() { tryInit(); }
    ~VramCache() {
        clear();
#ifdef _WIN32
        if (lib_) FreeLibrary(lib_);
#else
        if (lib_) dlclose(lib_);
#endif
    }
    VramCache(const VramCache&) = delete;
    VramCache& operator=(const VramCache&) = delete;

    // CUDA 函数指针类型（cudaError_t = int，0 = cudaSuccess）
    using fn_cudaMalloc    = int(*)(void**, size_t);
    using fn_cudaMemcpy    = int(*)(void*, const void*, size_t, int);
    using fn_cudaFree      = int(*)(void*);
    using fn_cudaGetDevice = int(*)(int*);

    static constexpr int kHostToDevice = 1;
    static constexpr int kDeviceToHost = 2;

    fn_cudaMalloc    pfnMalloc_  = nullptr;
    fn_cudaMemcpy    pfnMemcpy_  = nullptr;
    fn_cudaFree      pfnFree_    = nullptr;
    fn_cudaGetDevice pfnGetDev_  = nullptr;

#ifdef _WIN32
    HMODULE lib_ = nullptr;
#else
    void* lib_ = nullptr;
#endif

    bool cuAvail_ = false;
    int  devIdx_  = 0;

    struct Entry {
        void*  devPtr    = nullptr;
        size_t size      = 0;
        std::chrono::steady_clock::time_point expireAt;
        bool   hasExpire = false;
    };

    std::unordered_map<std::string, Entry> idx_;
    std::mutex mtx_;

    static bool isExpired(const Entry& e) {
        return e.hasExpire && std::chrono::steady_clock::now() > e.expireAt;
    }

    void evictEntry(const std::string& key) {
        auto it = idx_.find(key);
        if (it != idx_.end()) {
            pfnFree_(it->second.devPtr);
            idx_.erase(it);
        }
    }

    // 从 config.json 读取是否禁用 vram 缓存
    static bool isEnabledInConfig() {
        try {
            std::ifstream f("config.json");
            if (!f.is_open()) return true;
            Json::Value root;
            Json::CharReaderBuilder rb;
            std::string errs;
            if (!Json::parseFromStream(rb, f, &root, &errs)) return true;
            if (root.isMember("vram"))
                return root["vram"].get("enabled", true).asBool();
        } catch (...) {}
        return true;
    }

    void tryInit() {
        if (!isEnabledInConfig()) {
            std::cout << "[VramCache] disabled in config" << std::endl;
            return;
        }

#ifdef _WIN32
        // 优先按名称加载（已在 PATH/System32 时直接成功）
        const char* names[] = {
            "cudart64_12.dll", "cudart64_120.dll",
            "cudart64_11.dll", "cudart64_110.dll",
            "cudart64_100.dll", "cudart.dll", nullptr
        };
        for (int i = 0; names[i] && !lib_; ++i)
            lib_ = LoadLibraryA(names[i]);

        // 未找到时搜索 CUDA Toolkit 标准安装目录
        if (!lib_) {
            // 读取 CUDA_PATH 环境变量（NVIDIA 官方安装器设置）
            char cudaEnv[512] = {};
            DWORD envLen = GetEnvironmentVariableA("CUDA_PATH", cudaEnv, sizeof(cudaEnv));
            if (envLen == 0) envLen = GetEnvironmentVariableA("CUDA_PATH_V12_0", cudaEnv, sizeof(cudaEnv));
            if (envLen == 0) envLen = GetEnvironmentVariableA("CUDA_PATH_V11_0", cudaEnv, sizeof(cudaEnv));

            if (envLen > 0) {
                std::string base(cudaEnv);
                for (const char* n : {"cudart64_12.dll","cudart64_120.dll",
                                      "cudart64_11.dll","cudart64_110.dll"}) {
                    std::string full = base + "\\bin\\" + n;
                    lib_ = LoadLibraryA(full.c_str());
                    if (lib_) { std::cout << "[VramCache] loaded " << full << std::endl; break; }
                }
            }
        }

        // 仍未找到：搜索 Program Files 下 CUDA 目录
        if (!lib_) {
            std::string pf = "C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA";
            WIN32_FIND_DATAA fd;
            HANDLE hf = FindFirstFileA((pf + "\\*").c_str(), &fd);
            if (hf != INVALID_HANDLE_VALUE) {
                do {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                    if (fd.cFileName[0] == '.') continue;
                    for (const char* n : {"cudart64_12.dll","cudart64_120.dll",
                                          "cudart64_11.dll","cudart64_110.dll"}) {
                        std::string full = pf + "\\" + fd.cFileName + "\\bin\\" + n;
                        lib_ = LoadLibraryA(full.c_str());
                        if (lib_) { std::cout << "[VramCache] loaded " << full << std::endl; break; }
                    }
                    if (lib_) break;
                } while (FindNextFileA(hf, &fd));
                FindClose(hf);
            }
        }

        if (!lib_) {
            std::cout << "[VramCache] cudart not found, VRAM cache disabled" << std::endl;
            return;
        }
        auto resolve = [&](const char* name) -> void* {
            return reinterpret_cast<void*>(GetProcAddress(lib_, name));
        };
#else
        const char* names[] = {
            "libcudart.so.12", "libcudart.so.11",
            "libcudart.so.10", "libcudart.so", nullptr
        };
        for (int i = 0; names[i] && !lib_; ++i)
            lib_ = dlopen(names[i], RTLD_LAZY);
        if (!lib_) {
            std::cout << "[VramCache] cudart not found, VRAM cache disabled" << std::endl;
            return;
        }
        auto resolve = [&](const char* name) -> void* {
            return dlsym(lib_, name);
        };
#endif
        pfnMalloc_  = reinterpret_cast<fn_cudaMalloc>   (resolve("cudaMalloc"));
        pfnMemcpy_  = reinterpret_cast<fn_cudaMemcpy>   (resolve("cudaMemcpy"));
        pfnFree_    = reinterpret_cast<fn_cudaFree>     (resolve("cudaFree"));
        pfnGetDev_  = reinterpret_cast<fn_cudaGetDevice>(resolve("cudaGetDevice"));

        if (!pfnMalloc_ || !pfnMemcpy_ || !pfnFree_) {
            std::cout << "[VramCache] cudart symbols missing, VRAM cache disabled" << std::endl;
#ifdef _WIN32
            FreeLibrary(lib_); lib_ = nullptr;
#else
            dlclose(lib_); lib_ = nullptr;
#endif
            return;
        }

        // 探测：小块分配测试，确认 GPU 可用且有足够显存
        void* test = nullptr;
        int ret = pfnMalloc_(&test, 64);
        if (ret != 0 || !test) {
            std::cout << "[VramCache] cudaMalloc test failed (ret=" << ret
                      << "), VRAM cache disabled" << std::endl;
            return;
        }
        pfnFree_(test);

        if (pfnGetDev_) pfnGetDev_(&devIdx_);
        cuAvail_ = true;
        std::cout << "[Cache] VRAM backend available (CUDA device " << devIdx_ << ")" << std::endl;
    }
};
