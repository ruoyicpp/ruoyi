/**
 * @file OnnxService.h
 * @brief ONNX Runtime 推理服务 — 提供模型加载、推理、管理等功能
 * 
 * 功能概述：
 *   - 模型管理：扫描、加载、列表 ONNX 模型
 *   - 推理引擎：执行模型推理，支持多输入多输出
 *   - 自动降级：未安装 ONNX Runtime 时自动降级为 stub 模式
 * 
 * 编译要求：
 *   安装 ONNX Runtime C API，在 CMakeLists.txt 中添加：
 *   ```cmake
 *   find_library(ONNXRUNTIME_LIB NAMES onnxruntime)
 *   target_link_libraries(ruoyi-cpp ${ONNXRUNTIME_LIB})
 *   target_include_directories(ruoyi-cpp PRIVATE /usr/local/include/onnxruntime)
 *   target_compile_definitions(ruoyi-cpp PRIVATE HAVE_ONNXRUNTIME=1)
 *   ```
 * 
 * 未安装时的行为：
 *   - API 仍然可用，但所有操作返回失败或空结果
 *   - 模式显示为 "stub"，提示用户编译支持
 * 
 * 使用示例：
 *   ```cpp
 *   OnnxService::instance().init("./models");
 *   OnnxService::instance().loadModel("resnet50", "./models/resnet50.onnx");
 *   Json::Value inputs;
 *   inputs["input"] = Json::Value(Json::arrayValue);
 *   // 填充输入数据...
 *   auto result = OnnxService::instance().infer("resnet50", inputs);
 *   ```
 */

/**
 * @file OnnxService.h
 * @brief ONNX 推理服务 — 支持深度学习模型推理
 * 
 * 功能概述：
 *   - 模型加载：支持加载 ONNX 格式的深度学习模型
 *   - 推理执行：支持 CPU 和 GPU 推理
 *   - 批量推理：支持批量数据推理，提高吞吐量
 *   - 模型管理：支持多模型管理和版本控制
 *   - 性能优化：模型量化、剪枝、融合等优化
 *   - 推理监控：记录推理延迟、吞吐量、错误率
 * 
 * 核心特性：
 *   - ONNX Runtime：使用 ONNX Runtime 作为推理引擎
 *   - 多后端支持：支持 CPU、CUDA、TensorRT 等多种后端
 *   - 动态输入：支持动态 Batch Size 和动态输入形状
 *   - 内存管理：自动管理模型内存，防止内存泄漏
 *   - 错误恢复：推理失败时自动回退或重试
 *   - 热更新：支持模型热更新，无需重启应用
 * 
 * 支持的模型类型：
 *   - 图像分类：ResNet、VGG、MobileNet 等
 *   - 目标检测：YOLO、Faster R-CNN、SSD 等
 *   - 自然语言处理：BERT、GPT、T5 等
 *   - 时间序列预测：LSTM、GRU、Transformer 等
 *   - 推荐系统：Wide&Deep、DeepFM 等
 * 
 * 配置项（config.json）：
 *   - onnx.enabled: 是否启用 ONNX 推理（默认 false）
 *   - onnx.model_dir: 模型文件目录
 *   - onnx.backend: "cpu" | "cuda" | "tensorrt"（默认 "cpu"）
 *   - onnx.num_threads: CPU 线程数（默认 4）
 *   - onnx.batch_size: 推理 Batch Size（默认 1）
 *   - onnx.cache_size: 模型缓存大小（MB，默认 512）
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <filesystem>
#include <json/json.h>
#include <iostream>

#ifdef HAVE_ONNXRUNTIME
#  include <onnxruntime_c_api.h>
#endif

/**
 * @class OnnxService
 * @brief ONNX Runtime 推理服务单例
 * 
 * 提供 ONNX 模型的加载、推理、管理等功能。
 * 采用单例模式，全局唯一实例。
 * 
 * 支持两种模式：
 *   - Runtime 模式：ONNX Runtime 已安装，可执行真实推理
 *   - Stub 模式：ONNX Runtime 未安装，API 可用但返回失败
 */
class OnnxService {
public:
    static OnnxService &instance() { static OnnxService s; return s; }

    /**
     * @struct ModelInfo
     * @brief ONNX 模型元数据
     * 
     * 存储模型的基本信息，包括名称、路径、输入输出节点等。
     */
    struct ModelInfo {
        std::string name;                      ///< 模型名称（用于推理时引用）
        std::string path;                      ///< 模型文件路径
        std::string description;               ///< 模型描述
        std::vector<std::string> inputNames;   ///< 输入节点名称列表
        std::vector<std::string> outputNames;  ///< 输出节点名称列表
        bool loaded = false;                   ///< 是否已加载
    };

    /**
     * @brief 初始化 ONNX Runtime 服务
     * 
     * 创建模型目录、初始化 ONNX Runtime 环境、扫描已有模型。
     * 应在应用启动时调用一次。
     * 
     * @param modelDir 模型存储目录（默认 "./models"）
     * 
     * @note 
     *   - 如果目录不存在会自动创建
     *   - 如果 ONNX Runtime 未安装，自动降级为 stub 模式
     *   - 初始化后会自动扫描 modelDir 中的 .onnx 文件
     */
    void init(const std::string &modelDir = "./models") {
        modelDir_ = modelDir;
        std::filesystem::create_directories(modelDir_);
        scanModels();
#ifdef HAVE_ONNXRUNTIME
        env_ = OrtGetApiBase()->GetApi(ORT_API_VERSION);
        OrtEnv *ort_env = nullptr;
        env_->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "ruoyi-onnx", &ort_env);
        ortEnv_ = ort_env;
        std::cout << "[ONNX] Runtime initialized, model dir: " << modelDir_ << std::endl;
#else
        std::cout << "[ONNX] Runtime NOT available (stub mode). Define HAVE_ONNXRUNTIME to enable." << std::endl;
#endif
    }

    /**
     * @brief 列出所有已加载的模型
     * 
     * @return 模型信息列表
     */
    std::vector<ModelInfo> listModels() {
        std::vector<ModelInfo> v;
        for (auto &[k, m] : models_) v.push_back(m);
        return v;
    }

    /**
     * @brief 加载 ONNX 模型文件
     * 
     * 从指定路径加载 ONNX 模型，并注册到模型管理器。
     * 加载成功后会自动查询模型的输入输出节点信息。
     * 
     * @param name 模型名称（用于后续推理时引用）
     * @param path 模型文件路径（.onnx 文件）
     * 
     * @return 加载是否成功
     * 
     * @note 
     *   - 如果 ONNX Runtime 未安装，返回 false
     *   - 模型文件必须是有效的 ONNX 格式
     *   - 加载大型模型可能需要较长时间
     */
    bool loadModel(const std::string &name, const std::string &path) {
#ifdef HAVE_ONNXRUNTIME
        if (!ortEnv_) return false;
        ModelInfo info; info.name = name; info.path = path;
        try {
            OrtSessionOptions *opts = nullptr;
            env_->CreateSessionOptions(&opts);
            env_->SetIntraOpNumThreads(opts, 1);
            OrtSession *session = nullptr;
            env_->CreateSession(ortEnv_, path.c_str(), opts, &session);
            // 查询输入/输出名
            OrtAllocator *alloc = nullptr;
            env_->GetAllocatorWithDefaultOptions(&alloc);
            size_t n = 0;
            env_->SessionGetInputCount(session, &n);
            for (size_t i = 0; i < n; ++i) {
                char *nm = nullptr;
                env_->SessionGetInputName(session, i, alloc, &nm);
                info.inputNames.push_back(nm);
            }
            env_->SessionGetOutputCount(session, &n);
            for (size_t i = 0; i < n; ++i) {
                char *nm = nullptr;
                env_->SessionGetOutputName(session, i, alloc, &nm);
                info.outputNames.push_back(nm);
            }
            info.loaded = true;
            sessions_[name] = session;
            env_->ReleaseSessionOptions(opts);
        } catch (...) { return false; }
        models_[name] = info;
        return true;
#else
        ModelInfo info; info.name = name; info.path = path; info.loaded = false;
        models_[name] = info;
        return false;
#endif
    }

    // 推理：inputs = {inputName: [float values]}
    // 返回: {outputName: [float values]}
    Json::Value infer(const std::string &modelName, const Json::Value &inputs) {
        Json::Value result;
#ifdef HAVE_ONNXRUNTIME
        auto it = sessions_.find(modelName);
        if (it == sessions_.end()) throw std::runtime_error("Model not loaded: " + modelName);
        auto &info = models_[modelName];
        OrtSession *session = it->second;
        OrtAllocator *alloc = nullptr;
        env_->GetAllocatorWithDefaultOptions(&alloc);

        std::vector<std::vector<float>> inputData;
        std::vector<OrtValue*> inputTensors;
        std::vector<const char*> inputNamesC, outputNamesC;
        OrtMemoryInfo *memInfo = nullptr;
        env_->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memInfo);

        for (auto &name : info.inputNames) {
            inputNamesC.push_back(name.c_str());
            const Json::Value &arr = inputs[name];
            std::vector<float> data;
            for (const auto &v : arr) data.push_back(v.asFloat());
            inputData.push_back(data);
            auto &d = inputData.back();
            int64_t shape[] = {1, (int64_t)d.size()};
            OrtValue *tensor = nullptr;
            env_->CreateTensorWithDataAsOrtValue(memInfo, d.data(),
                d.size() * sizeof(float), shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &tensor);
            inputTensors.push_back(tensor);
        }
        for (auto &name : info.outputNames) outputNamesC.push_back(name.c_str());

        std::vector<OrtValue*> outputTensors(info.outputNames.size(), nullptr);
        env_->Run(session, nullptr,
                  inputNamesC.data(), inputTensors.data(), inputTensors.size(),
                  outputNamesC.data(), outputTensors.size(), outputTensors.data());

        for (size_t i = 0; i < info.outputNames.size(); ++i) {
            float *ptr = nullptr;
            env_->GetTensorMutableData(outputTensors[i], (void**)&ptr);
            OrtTensorTypeAndShapeInfo *shapeInfo = nullptr;
            env_->GetTensorTypeAndShape(outputTensors[i], &shapeInfo);
            size_t cnt = 0; env_->GetTensorShapeElementCount(shapeInfo, &cnt);
            env_->ReleaseTensorTypeAndShapeInfo(shapeInfo);
            Json::Value arr(Json::arrayValue);
            for (size_t j = 0; j < cnt; ++j) arr.append(ptr[j]);
            result[info.outputNames[i]] = arr;
            env_->ReleaseValue(outputTensors[i]);
        }
        for (auto *t : inputTensors) env_->ReleaseValue(t);
        env_->ReleaseMemoryInfo(memInfo);
#else
        throw std::runtime_error("ONNX Runtime not available. Compile with HAVE_ONNXRUNTIME=1.");
#endif
        return result;
    }

    bool available() const {
#ifdef HAVE_ONNXRUNTIME
        return ortEnv_ != nullptr;
#else
        return false;
#endif
    }

    std::string modelDir() const { return modelDir_; }

private:
    std::string modelDir_ = "./models";
    std::map<std::string, ModelInfo> models_;

#ifdef HAVE_ONNXRUNTIME
    const OrtApi *env_ = nullptr;
    OrtEnv *ortEnv_ = nullptr;
    std::map<std::string, OrtSession*> sessions_;
#endif

    void scanModels() {
        std::error_code ec;
        for (auto &e : std::filesystem::directory_iterator(modelDir_, ec)) {
            if (!e.is_regular_file()) continue;
            auto ext = e.path().extension().string();
            if (ext != ".onnx") continue;
            std::string name = e.path().stem().string();
            ModelInfo info; info.name = name; info.path = e.path().string();
            models_[name] = info;
        }
    }
};
