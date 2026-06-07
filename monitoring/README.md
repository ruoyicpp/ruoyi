# RuoYi-Cpp 性能监控与可观测性集成 (`monitoring`)

`monitoring` 目录提供了 RuoYi-Cpp 生态的性能监控、日志指标收集（Observability）与度量大屏可视化所需的配置文件和看板模板。

---

## 📦 目录结构

```
monitoring/
├── prometheus.yml           # Prometheus 采集端配置文件
├── grafana-dashboard.json   # Grafana 精美系统度量监控大屏模板
└── README.md                # 本文档
```

---

## 🛠️ 监控组件详解

### 1. 采集端配置 (`prometheus.yml`)
指定了 Prometheus 时序数据库拉取（Scrape）系统指标的目标和频次：
* **RuoYi-Cpp 应用指标**：
  * **路由目标**：应用程序容器 `app:8080`（支持通过容器服务名发现）。
  * **指标端点**：`/metrics`（通过 C++ 后端内建指标服务暴露）。
  * **采集间隔**：高频 `10s`，实时捕捉接口瞬时流量尖峰。
* **时序库自身指标**：本地 `localhost:9090`，常规 `15s` 抓取。
* **PostgreSQL 数据库指标**：通过官方 `postgres-exporter:9187` 对接，分析读写延时与长事务。

### 2. 监控可视化大盘 (`grafana-dashboard.json`)
为您预先配置了一套精美的图形化监控看板。只需将其导入到您的 Grafana 实例中，即可直接享用以下大盘图表：
* **应用吞吐量**：QPS 曲线、请求成功率、HTTP 状态码分布统计。
* **延迟分析**：P50 / P95 / P99 响应延迟直方图，帮助迅速定位慢接口。
* **系统负载**：CPU 使用率、虚拟/物理内存开销，实时监测内存泄露或计算瓶颈。
* **数据库健康**：活动连接数分布、连接池闲置水位。
* **硬件级监控**：Drogon 工作线程与主事件循环状态。

---

## 🚀 部署与启用步骤

通过 Docker-Compose 堆栈与您的应用集群无缝集成：

### Step 1. 在 `docker-compose.yml` 中追加监控节点
```yaml
services:
  # Prometheus 采集器
  prometheus:
    image: prom/prometheus:latest
    container_name: ruoyi-prometheus
    volumes:
      - ./monitoring/prometheus.yml:/etc/prometheus/prometheus.yml
    ports:
      - "9090:9090"
    restart: unless-stopped

  # Grafana 可视化仪表盘
  grafana:
    image: grafana/grafana:latest
    container_name: ruoyi-grafana
    ports:
      - "3000:3000"
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
    volumes:
      - grafana_data:/var/lib/grafana
    restart: unless-stopped

volumes:
  grafana_data:
```

### Step 2. 导入 Grafana 可视化大盘
1. 访问并登录 Grafana 后台：`http://localhost:3000`（默认账号密码：`admin` / `admin`）。
2. 在左侧菜单点击 **Connections** -> **Data sources**，添加 **Prometheus**，填写连接地址 `http://ruoyi-prometheus:9090` 并保存。
3. 在左侧菜单点击 **Dashboards** -> **New** -> **Import**。
4. 点击 **Upload JSON file**，选择并导入本目录下的 `grafana-dashboard.json`，关联刚才创建的 Prometheus 数据源。
5. 尽情享用精美的高性能 C++ 后端运行状态看板！
