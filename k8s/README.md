# RuoYi-Cpp Kubernetes 部署配置 (`k8s`)

`k8s` 目录提供了 RuoYi-Cpp 容器化微服务在大规模云原生集群（Kubernetes）中运行所需的资源编排配置文件。

---

## 📦 目录结构

```
k8s/
├── deployment.yaml     # Kubernetes 资源编排主配置文件（包含 7 大核心组件）
└── README.md           # 本文档
```

---

## 🎡 编排组件详解 (`deployment.yaml`)

`deployment.yaml` 通过标准的多 YAML 段结构整合了系统运行、访问控制、横向扩缩容与外网路由：

### 1. 配置映射 (`ConfigMap: ruoyi-cpp-config`)
* **作用**：存储非敏感的通用配置，作为容器内部的默认配置文件。
* **数据**：挂载为 `/app/config.json`，指定端口为 `8080`，并配置数据库与 Redis 服务名称。

### 2. 密钥管理 (`Secret: ruoyi-cpp-secret`)
* **作用**：存储生产级别的敏感配置（如数据库密码、JWT 加密盐等）。
* **安全**：将真实密码与代码仓库分离，由集群进行加密存储。

### 3. 应用部署单元 (`Deployment: ruoyi-cpp`)
* **高可用**：默认启动 3 个应用副本（`replicas: 3`）保障容灾能力。
* **健康检查**：
  * **存活探针 (`livenessProbe`)**：在 30 秒冷启动宽限后开始每 10 秒轮询监测 `/health` 接口，异常时自动重启容器。
  * **就绪探针 (`readinessProbe`)**：启动 5 秒后检测应用就绪状态，就绪前不分发任何业务流量。
* **资源上限约束 (`resources`)**：限制单个 Pod 最多使用 0.5 核 CPU、1Gi 内存，确保集群节点不因单容器内存泄露而瘫痪。

### 4. 负载均衡网络与服务
* **内网 Service (`ruoyi-cpp-service`)**：分配稳定的集群内网 IP（ClusterIP），供内网微服务或 Ingress 路由。
* **外网 Service (`ruoyi-cpp-lb`)**：提供标准 LoadBalancer 类型，适配各大云厂商自动分配外网弹性 IP。

### 5. 自动水平扩缩容 (`HorizontalPodAutoscaler: ruoyi-cpp-hpa`)
* **策略**：
  * **最小实例数**：`2` 个。
  * **最大实例数**：`10` 个。
  * **扩容指标**：当 Pod 平均 CPU 使用率超过 **70%**，或平均内存使用率超过 **80%** 时，Kubernetes 将自动扩容。

### 6. 入口网关与路由 (`Ingress: ruoyi-cpp-ingress`)
* **域名路由**：将 `api.ruoyi.com` 请求路由到后台服务的 `8080` 端口。
* **SSL 证书集成**：内置 `cert-manager` 集群注解与 HTTPS 自动重定向。

---

## 🚀 部署操作指南

在已经准备好 Kubernetes 集群并配置好本地 `kubectl` 的环境下：

### 1. 创建专用命名空间
```bash
kubectl create namespace ruoyi
```

### 2. 一键应用资源编排
```bash
kubectl apply -f k8s/
```

### 3. 查看应用运行状态
```bash
# 查看所有 Pod 及事件
kubectl get pods -n ruoyi -o wide

# 查看应用负载均衡分配的外网 IP
kubectl get svc -n ruoyi
```

### 4. 查看日志或调试
```bash
# 跟踪其中一个 Pod 的实时日志
kubectl logs -f -l app=ruoyi-cpp -n ruoyi --tail=100
```
