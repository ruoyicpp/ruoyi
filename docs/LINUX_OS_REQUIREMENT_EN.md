# Linux OS Requirement: Strictly Requires Ubuntu 24.04 LTS

---

### ⚠️ Important Notice: Linux OS Constraint
The Linux binary release and runtime environment of this project **strictly require and only support Ubuntu 24.04 LTS (Noble Numbat)** or higher.
Do not attempt to run the compiled binaries directly on Ubuntu 22.04, Debian 12, or older Linux distributions, as doing so will cause immediate linker errors and runtime crashes.

---

### 🛠️ Why is Ubuntu 24.04 Strictly Required?

1. **Strict Dependency on GLIBC 2.39+**
   - Ubuntu 24.04 ships with **GLIBC 2.39** by default.
   - `ruoyi-cpp` is compiled against modern glibc symbols. Running it on older platforms (like Ubuntu 22.04 which has GLIBC 2.35) will result in the following error:
     ```bash
     /lib/x86_64-linux-gnu/libc.so.6: version `GLIBC_2.39' not found
     ```

2. **C++20 Standard Library (GCC 13 / libstdc++ 13+)**
   - This project leverages advanced **C++20 language and library features** (including `std::ranges`, `std::format`, high-performance coroutines, and atomic barriers).
   - Older Linux systems do not have full runtime support for these C++20 features in their older `libstdc++.so.6` versions, leading to unresolved references or crashes.

3. **OpenSSL 3.3.0 & Modern Security Symbols**
   - Cryptographic, HTTPS, and ACME auto-renewal modules strictly bind to modern OpenSSL shared objects and `t64` standard symbols (such as `libssl3t64`), which are absent in older platforms.

4. **Modern ABI Specifications for Dependencies**
   - Shared libraries such as `libpq` (PostgreSQL client), `libhiredis` (Redis), and `libjsoncpp` compiled under Ubuntu 24.04 adopt new modern ABI standards (including 64-bit time_t) that differ fundamentally from older versions.

---

### 💡 Solutions & Workarounds

#### Solution 1: Run Directly on Ubuntu 24.04 LTS (Recommended)
Ensure your target production server or development machine is upgraded to **Ubuntu 24.04 LTS**, then run the application normally:
```bash
./ruoyi-cpp
```

#### Solution 2: Deploy via Docker (Cross-Platform)
If your host OS cannot be upgraded to Ubuntu 24.04 (e.g., you are constrained to CentOS, Debian, or Ubuntu 22.04), utilize our **Docker containerization**.
Our Docker images encapsulate the full Ubuntu 24.04 runtime, allowing the application to run smoothly on **any** Linux distribution:
```bash
# Spin up the containers (built on top of Ubuntu 24.04 base image)
docker-compose up -d
```

#### Solution 3: Compile from Source Locally (Complete Compatibility)
The Ubuntu 24.04 constraint **only applies to directly running our precompiled official Linux binary package**. If you download the source code and **compile it locally** on your own Linux machine (such as Ubuntu 22.04, Debian 12, CentOS, Fedora, etc.), the compiler will link against your local GLIBC, OpenSSL, and C++20 runtime libraries, **making it fully compatible and runnable across any Linux operating system**:
```bash
# Local compilation steps:
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja ruoyi-cpp
```
