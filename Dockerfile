# ═══════════════════════════════════════════════════════════════════
# RuoYi-Cpp 多阶段构建
# 构建环境: Ubuntu 22.04 + GCC 12 + C++20
# 运行时:   Ubuntu 22.04 minimal (仅 .so)
# ═══════════════════════════════════════════════════════════════════

# ── Stage 1: 构建 ──────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++-12 gcc-12 cmake ninja-build git ca-certificates pkg-config \
    libpq-dev libhiredis-dev libjsoncpp-dev libssl-dev zlib1g-dev \
    libbrotli-dev libc-ares-dev uuid-dev \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 \
    && rm -rf /var/lib/apt/lists/*

# 编译 Drogon
WORKDIR /build
RUN git clone --depth=1 --branch v1.9.10 https://github.com/drogonframework/drogon.git \
    && cd drogon && git submodule update --init \
    && mkdir build && cd build \
    && cmake .. -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_EXAMPLES=OFF -DBUILD_CTL=OFF \
        -DBUILD_POSTGRESQL=ON -DBUILD_REDIS=ON -DBUILD_MYSQL=OFF \
        -DBUILD_SQLITE=OFF -DBUILD_BROTLI=ON \
    && ninja && ninja install

# 编译 RuoYi-Cpp
COPY . /app
WORKDIR /app/build
RUN cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DDROGON_INSTALL_PREFIX=/usr/local \
    -DENABLE_NGINX_EMBEDDED=OFF \
    && ninja ruoyi-cpp

# ── Stage 2: 运行时 ───────────────────────────────────────────────
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    libpq5 libhiredis0.14 libjsoncpp25 libssl3 zlib1g \
    libbrotli1 libc-ares2 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/ruoyi-cpp

# 复制二进制和必要文件
COPY --from=builder /app/build/ruoyi-cpp .
COPY --from=builder /app/config.json ./config.json
COPY --from=builder /app/vue-c++/dist/ ./dist/

# 日志和上传目录
RUN mkdir -p logs upload

EXPOSE 18080

# 环境变量配置（可覆盖 config.json 中的敏感字段）
# RUOYI_DATABASE_PASSWD=xxx
# RUOYI_DATABASE_HOST=postgres
# RUOYI_REDIS_HOST=redis

ENTRYPOINT ["./ruoyi-cpp"]
