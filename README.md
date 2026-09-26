<div align="center">

[English](README_EN.md) | 涓枃

# RuoYi-Cpp

**RuoYi 绠＄悊妗嗘灦鐨?C++ 楂樻€ц兘鐗堟湰** 路 `v1.3.3`

鍩轰簬 [Drogon](https://github.com/drogonframework/drogon) + PostgreSQL锛屼笌 RuoYi-Vue 鍓嶇 100% 鍏煎

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Drogon](https://img.shields.io/badge/Drogon-latest-green.svg)](https://github.com/drogonframework/drogon)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-brightgreen.svg)]()

[![RuoYi-Vue](https://img.shields.io/badge/RuoYi--Vue-3.8-red.svg)](https://gitee.com/y_project/RuoYi-Vue)
[![Vue](https://img.shields.io/badge/Vue-2.x-4FC08D.svg?logo=vue.js)](https://v2.vuejs.org)
[![Element UI](https://img.shields.io/badge/Element--UI-2.x-409EFF.svg)](https://element.eleme.io)
[![PostgreSQL](https://img.shields.io/badge/PostgreSQL-12%2B-336791.svg?logo=postgresql)](https://www.postgresql.org)
[![SQLite](https://img.shields.io/badge/SQLite-3-003B57.svg?logo=sqlite)](https://www.sqlite.org)
[![JWT](https://img.shields.io/badge/JWT-jwt--cpp-000000.svg?logo=jsonwebtokens)](https://github.com/Thalhammer/jwt-cpp)
[![OpenSSL](https://img.shields.io/badge/OpenSSL-3.x-721412.svg?logo=openssl)](https://www.openssl.org)
[![Nginx](https://img.shields.io/badge/Nginx-optional-009639.svg?logo=nginx)](https://nginx.org)
[![MinIO](https://img.shields.io/badge/MinIO%2FS3-optional-C72E49.svg)](https://min.io)
[![TOTP](https://img.shields.io/badge/TOTP-RFC6238-brightgreen.svg)](https://datatracker.ietf.org/doc/html/rfc6238)
[![OAuth2](https://img.shields.io/badge/OAuth2-GitHub%20%7C%20Google%20%7C%20%E9%92%89%E9%92%89%20%7C%20%E9%A3%9E%E4%B9%A6-4A90E2.svg)]()

</div>

---

## 鍦ㄧ嚎婕旂ず

馃寪 **婕旂ず鍦板潃**锛歔https://www.nulk.cn/](https://www.nulk.cn/)

> 榛樿璐﹀彿锛歚admin` / `admin123`

---

## 椤圭洰绠€浠?

RuoYi-Cpp 鏄?[鑻ヤ緷锛圧uoYi-Vue锛塢(https://gitee.com/y_project/RuoYi-Vue) 绠＄悊妗嗘灦鐨?C++ 楂樻€ц兘鐗堟湰锛屽悗绔熀浜?Drogon 寮傛 HTTP 妗嗘灦锛屾暟鎹簱浣跨敤 PostgreSQL锛屼笌鍘熺増 RuoYi-Vue 鍓嶇淇濇寔瀹屽叏 API 鍏煎銆?

> 鉁?**骞冲彴鏀寔**锛氬凡鍦?**Windows锛圡SYS2 MinGW64锛?* 鍜?**Linux锛圙CC锛?* 涓婂畬鏁寸紪璇戦獙璇侀€氳繃銆傛暟鎹簱浣跨敤 **SQLite 鍐呭祵妯″紡**锛堟棤闇€瀹夎 PostgreSQL 鍗冲彲杩愯锛夛紝PostgreSQL 浣滀负鍙€変富鏁版嵁搴撱€?

**鐩告瘮 Java 鐗堟湰鐨勪紭鍔匡細**

| 瀵规瘮椤?| Java (Spring Boot) | RuoYi-Cpp |
|--------|-------------------|-----------|
| 鍚姩鍐呭瓨 | ~300鈥?00 MB | **~3.2-10 MB** |
| 鍚姩鏃堕棿 | 5鈥?5 绉?| **< 1 绉?* |
| 杩愯鏃朵緷璧?| JDK 17+ | 鏃狅紙闈欐€侀摼鎺ワ級 |
| 閮ㄧ讲鏂瑰紡 | JAR + JVM | **鍗曚釜鍙墽琛屾枃浠?* |
| 閫傜敤鍦烘櫙 | 浜戞湇鍔″櫒 | 浜戞湇鍔″櫒 / NAS / 宓屽叆寮?|
| Nginx 渚濊禆 | **鍙€?* | **鍙€?*锛堝唴缃墠绔墭绠★級|
| 鐑洿鏂拌兘鍔?| 闇€瑕侀噸鍚?| **鏀寔鍔ㄦ€佸簱鐑洿鏂?* |

---

## 鏍稿績鐗规€?

- 鉁?**100% API 鍏煎** - 鐩存帴浣跨敤瀹樻柟 RuoYi-Vue 鍓嶇锛屾棤闇€淇敼
- 鉁?**鏋佽嚧鎬ц兘** - 鍗曟牳 C++17 寮傛妗嗘灦锛孮PS 鍙揪 10000+
- 鉁?**闆朵緷璧栭儴缃?* - 闈欐€侀摼鎺ユ墍鏈夊簱锛屽崟涓彲鎵ц鏂囦欢锛屾棤闇€ JVM/Runtime
- 鉁?**鍐呯疆鍓嶇鎵樼** - 鏃犻渶 Nginx锛孌rogon 鐩存帴鎵樼 Vue 鍓嶇
- 鉁?**鍙屾暟鎹簱鏀寔** - PostgreSQL 涓诲簱 + SQLite 鑷姩闄嶇骇锛孭G 鎭㈠鍚庤嚜鍔ㄥ悓姝?
- 鉁?**浼佷笟绾у姛鑳?* - RBAC 鏉冮檺銆佸璁℃棩蹇椼€佹暟鎹劚鏁忋€佽姹傜鍚嶃€佽澶囩粦瀹?
- 鉁?**绗笁鏂圭櫥褰?* - GitHub / Google / 浼佷笟寰俊 / 閽夐拤 / 椋炰功 / QQ OAuth2
- 鉁?**涓ゆ楠岃瘉** - Google Authenticator TOTP RFC 6238
- 鉁?**瀵嗛挜绠＄悊** - HashiCorp Vault 闆嗘垚锛岃嚜鍔ㄥ惎鍔?瑙ｅ皝/娉ㄥ叆
- 鉁?**鍙娴嬫€?* - Prometheus 鎸囨爣銆乆-Request-ID 閾捐矾杩借釜銆丣SON 缁撴瀯鍖栨棩蹇?
- 鉁?**WAF 闃茬伀澧?* - 鍐呯疆 SQL 娉ㄥ叆 / XSS / 璺緞绌胯秺 / 鍛戒护娉ㄥ叆姝ｅ垯瑙勫垯寮曟搸锛孖P 榛戠櫧鍚嶅崟锛圕IDR锛夛紝Linux 涓嬪彲鑱斿姩 nftables 鍐呮牳灞傚皝绂?
- 鉁?**鍔ㄦ€佸簱妯″潡** - 浠ｇ爜鐢熸垚妯″潡鐙珛缂栬瘧锛屾敮鎸佺儹鏇存柊鏃犻渶閲嶅惎涓荤▼搴?
- 鉁?**闆嗙兢閮ㄧ讲** - 鏀寔澶?Worker 杩涚▼锛岃嚜鍔ㄧ敓鎴?Nginx upstream.conf

---

## 鍔熻兘妯″潡

> 馃摉 鎺ュ彛鏂囨。涓嶅湪浠撳簱涓淮鎶も€斺€斿惎鍔ㄥ悗璁块棶 **`/swagger-ui/`**锛圫wagger UI锛夋垨 `GET /v3/api-docs`锛圤penAPI 3.0 JSON锛夎幏鍙栧疄鏃舵帴鍙ｅ畾涔夈€?

- **绯荤粺绠＄悊** 鈥?鐢ㄦ埛 / 瑙掕壊 / 鑿滃崟 / 閮ㄩ棬 / 宀椾綅 / 鍙傛暟閰嶇疆 / 瀛楀吀 / 閫氱煡鍏憡 / 閭欢閰嶇疆 / TOTP 涓ゆ楠岃瘉 / OAuth2 绗笁鏂圭櫥褰曪紙GitHub銆丟oogle銆佷紒涓氬井淇°€侀拤閽夈€侀涔︺€丵Q锛?
- **绯荤粺鐩戞帶** 鈥?鎿嶄綔鏃ュ織 / 鐧诲綍鏃ュ織 / 鍦ㄧ嚎鐢ㄦ埛 / 瀹氭椂浠诲姟锛堢绾?Cron锛? 绯荤粺鏃ュ織鏌ョ湅鍣?/ 鏈嶅姟鐩戞帶锛圕PU銆佸唴瀛樸€佺鐩樸€丟PU锛? 缂撳瓨鐩戞帶 / 鏁版嵁婧愮洃鎺?/ 閲嶅惎绠＄悊椤?
- **璐﹀彿鑷姪** 鈥?鐧诲綍锛堟敮鎸?LDAP锛? 娉ㄥ唽锛堥偖绠遍獙璇佺爜锛? 蹇樿瀵嗙爜 / 閲嶇疆瀵嗙爜
- **浠ｇ爜鐢熸垚涓庡伐鍏?* 鈥?浠ｇ爜鐢熸垚锛堢嫭绔嬪姩鎬佸簱鎻掍欢锛屾敮鎸佺儹鏇存柊锛? 椤圭洰鏋勫缓 / 缃戠珯淇℃伅 / 瑙嗛澶勭悊
- **AI 涓庢櫤鑳?* 鈥?澶фā鍨嬪璇濓紙娴佸紡锛? AI 浠ｇ爜鐢熸垚 / 璇煶璇嗗埆锛圵hisper锛? ONNX Embedding
- **IoT 涓庤澶囩鐞?* 鈥?璁惧绠＄悊 / Modbus 璇诲啓 / 鎵归噺杞
- **杩愮淮涓庡彲瑙傛祴鎬?* 鈥?`/actuator/health`銆乣/actuator/metrics`锛圥rometheus锛夈€乣/actuator/db`銆乣/actuator/reload`锛堥厤缃儹閲嶈浇锛?
- **WAF 闃茬伀澧?* 鈥?瑙勫垯寮曟搸锛堝唴缃?SQLi / XSS / 璺緞绌胯秺 / 鍛戒护娉ㄥ叆 + 鑷畾涔夋鍒欙級銆両P 榛戠櫧鍚嶅崟锛圕IDR锛夈€乁RI / UA 鍚嶅崟銆佸皝绂佺鐞嗭紙`/monitor/waf/**`锛夈€丯DJSON 鎷︽埅鏃ュ織銆乶ftables 鍐呮牳灞傚皝绂侊紙浠?Linux锛?

---

## 鎶€鏈爤

| 缁勪欢 | 鎶€鏈?|
|------|-----|
| HTTP 妗嗘灦 | [Drogon](https://github.com/drogonframework/drogon) (C++17, 寮傛闈為樆濉? |
| 涓绘暟鎹簱 | PostgreSQL (libpq 鐩磋繛 + 杩炴帴姹? |
| 澶囩敤鏁版嵁搴?| SQLite锛堣嚜鍔ㄩ檷绾э紝PG 鎭㈠鍚庤嚜鍔ㄥ悓姝ュ洖鍐欙級|
| 缂撳瓨灞?| 杩涚▼鍐?MemCache / GPU VramCache / Redis锛堜笁绾у彲閫夛級|
| 鏂囦欢瀛樺偍 | 鏈湴纾佺洏锛堥粯璁わ級/ MinIO / AWS S3锛圓WS SigV4 绛惧悕锛墊
| 璁よ瘉 | JWT锛圼jwt-cpp](https://github.com/Thalhammer/jwt-cpp)锛夛紝PBKDF2-SHA256 瀵嗙爜鍝堝笇 |
| 涓ゆ楠岃瘉 | TOTP RFC 6238锛圙oogle Authenticator锛岀函 OpenSSL 瀹炵幇锛墊
| 绗笁鏂圭櫥褰?| OAuth2锛欸itHub / Google / 浼佷笟寰俊 / 閽夐拤 / 椋炰功 / QQ锛孋SRF-state 闃叉姢 |
| LDAP/AD | OpenLDAP CLI 闆嗘垚锛圠inux锛夛紝Windows 棰勭暀鎺ュ彛 |
| 瀵嗛挜绠＄悊 | HashiCorp Vault锛堣嚜鍔ㄥ惎鍔?/ 瑙ｅ皝 / 瀵嗛挜娉ㄥ叆锛墊
| 閭欢鍙戦€?| OpenSSL Implicit-TLS SMTP锛圦Q/163/浼佷笟閭锛屽鍙戜欢浜鸿疆杞級|
| 鍓嶇 | RuoYi-Vue锛圴ue 2 + Element UI锛夛紝Drogon **鍐呯疆鎵樼**锛屾棤闇€ Nginx |
| 鍙嶅悜浠ｇ悊 | Nginx锛堝彲閫夛紝椤圭洰鍐呯疆鍚姩绠＄悊锛岃嚜鍔ㄧ敓鎴?upstream.conf锛墊
| 鏃ュ織 | JSON 缁撴瀯鍖栨棩蹇楋紙`.jsonl`锛屾瘡琛屼竴涓?JSON 瀵硅薄锛屽彲鎺?ELK锛墊
| 鍙娴?| Prometheus 鎸囨爣绔偣銆乆-Request-ID 鍏ㄩ摼璺拷韪?|
| 瀹夊叏 | 璇锋眰绛惧悕楠岃瘉銆両P 闄愭祦銆乆SS 杩囨护銆乄AF 瑙勫垯寮曟搸 + nftables 灏佺銆佽澶囩粦瀹氥€佽鍙瘉绠＄悊 |

### 鎶€鏈爤鐗堟湰璇︽儏

| 缁勪欢 | 鐗堟湰 | 璇存槑 |
|------|------|-----|
| **C++ 鏍囧噯** | C++20 | 浣跨敤鏈€鏂?C++ 鐗规€э紝缂栬瘧鍣ㄩ渶鏀寔 C++20 |
| **Drogon** | latest | 寮傛 HTTP 妗嗘灦锛屾敮鎸?WebSocket銆丠TTP/2 |
| **PostgreSQL** | 12+ | 涓绘暟鎹簱锛屾敮鎸?JSON銆乁UID銆佸叏鏂囨悳绱㈢瓑楂樼骇鐗规€?|
| **SQLite** | 3.x | 澶囩敤鏁版嵁搴擄紝鑷姩闄嶇骇鍜屾仮澶?|
| **OpenSSL** | 3.x | 瀵嗙爜瀛﹀簱锛屾敮鎸?TLS 1.3銆丳BKDF2銆丠MAC-SHA256 |
| **JsonCpp** | latest | JSON 瑙ｆ瀽鍜岀敓鎴愬簱 |
| **jwt-cpp** | latest | JWT 浠ょ墝鐢熸垚鍜岄獙璇侊紙header-only锛?|
| **RuoYi-Vue** | 3.8 | 鍓嶇妗嗘灦锛孷ue 2 + Element UI |
| **Nginx** | 1.20+ | 鍙嶅悜浠ｇ悊鍜岃礋杞藉潎琛★紙鍙€夛級 |
| **MinIO** | latest | 瀵硅薄瀛樺偍鏈嶅姟锛堝彲閫夛級 |
| **Redis** | 6.0+ | 缂撳瓨鍜屼細璇濆瓨鍌紙鍙€夛級 |
| **HashiCorp Vault** | 1.12+ | 瀵嗛挜绠＄悊鏈嶅姟锛堝彲閫夛級 |

---

---

## 绯荤粺瑕佹眰

### 杩愯鐜

| 椤圭洰 | 瑕佹眰 | 璇存槑 |
|------|------|-----|
| **鎿嶄綔绯荤粺** | Windows 10+ / Linux / macOS | 宸插湪 Windows 11锛圡SYS2 MinGW64锛変笌 Linux锛圙CC锛夐獙璇?|
| **澶勭悊鍣?* | x86-64 鎴?ARM64 | 鎺ㄨ崘 4 鏍镐互涓?|
| **鍐呭瓨** | 鏈€灏?512MB锛屾帹鑽?2GB+ | 鍖呭惈鏁版嵁搴撳拰搴旂敤 |
| **纾佺洏** | 鏈€灏?500MB | 鍖呭惈搴旂敤銆佹棩蹇椼€佷笂浼犳枃浠?|
| **鏁版嵁搴?* | PostgreSQL 12+ 鎴?SQLite 3.x | 榛樿浣跨敤 SQLite锛屽彲鍒囨崲 PostgreSQL |
| **缃戠粶** | TCP 18080 绔彛鍙敤 | 榛樿鐩戝惉 0.0.0.0:18080 |

### 缂栬瘧鐜

| 宸ュ叿 | 鐗堟湰 | 璇存槑 |
|------|------|-----|
| **CMake** | 3.15+ | 鏋勫缓绯荤粺 |
| **C++ 缂栬瘧鍣?* | GCC 11+ / Clang 13+ / MSVC 2019+ | 闇€鏀寔 C++20 |
| **Git** | 2.0+ | 鐗堟湰鎺у埗 |
| **MSYS2 MinGW64** | 鏈€鏂扮増 | Windows 缂栬瘧鐜锛圵indows 鐢ㄦ埛锛?|
| **Drogon** | latest | 寮傛 HTTP 妗嗘灦锛堥渶棰勫厛缂栬瘧锛?|
| **PostgreSQL** | 12+ | 寮€鍙戝簱锛坙ibpq锛?|
| **OpenSSL** | 3.x | 寮€鍙戝簱 |

### 鍙€変緷璧?

| 缁勪欢 | 鐗堟湰 | 鐢ㄩ€?|
|------|------|-----|
| **Redis** | 6.0+ | 缂撳瓨鍔犻€熴€佷細璇濆瓨鍌?|
| **Nginx** | 1.20+ | 鍙嶅悜浠ｇ悊銆佽礋杞藉潎琛?|
| **MinIO** | latest | 瀵硅薄瀛樺偍锛堟浛浠ｆ湰鍦板瓨鍌級 |
| **HashiCorp Vault** | 1.12+ | 瀵嗛挜绠＄悊 |
| **Prometheus** | latest | 鎬ц兘鐩戞帶 |
| **Grafana** | latest | 鍙鍖栦华琛ㄦ澘 |

---

## 蹇€熶綋楠岋紙5 鍒嗛挓锛?

**鏈€蹇笂鎵嬫柟寮?*锛堟棤闇€缂栬瘧锛夛細

1. **涓嬭浇棰勭紪璇戠増鏈?*
   ```bash
   # 浠?Release 椤甸潰涓嬭浇 ruoyi-cpp-v1.3.3-windows.zip
   unzip ruoyi-cpp-v1.3.3-windows.zip
   cd ruoyi-cpp
   ```

2. **閰嶇疆鏁版嵁搴?*
   ```bash
   # 缂栬緫 config.json锛屼慨鏀规暟鎹簱杩炴帴锛堝彲閫夛紝榛樿鐢?SQLite锛?
   # 濡傛灉浣跨敤 PostgreSQL锛屼慨鏀逛互涓嬪瓧娈碉細
   # "database": { "host": "127.0.0.1", "port": 5432, "dbname": "ruoyi.c", "user": "postgres", "passwd": "your_password" }
   ```

3. **鍚姩鏈嶅姟**
   ```bash
   ./ruoyi-cpp.exe
   # 杈撳嚭锛歔INFO] Server started on http://0.0.0.0:18080
   ```

4. **璁块棶搴旂敤**
   - 鍓嶇锛歨ttp://localhost:18080
   - API 鏂囨。锛歨ttp://localhost:18080/swagger-ui/
   - 榛樿璐﹀彿锛歚admin` / `admin123`

> 鈿狅笍 **鐢熶骇鐜**锛氳绔嬪嵆淇敼榛樿瀵嗙爜鍜?JWT secret锛?

---

## 蹇€熷紑濮?

### 鍓嶇疆渚濊禆

**鏁版嵁搴?*锛氶渶瑕佽繍琛屼腑鐨?PostgreSQL 瀹炰緥锛堢増鏈?12+锛?

```sql
-- 鍒涘缓鏁版嵁搴擄紙棣栨杩愯鑷姩寤鸿〃锛屾棤闇€鎵嬪姩瀵煎叆 SQL锛?
CREATE DATABASE "ruoyi.c";
```

**Redis**锛堝彲閫夛級锛氫笉閰嶇疆鏃惰嚜鍔ㄩ€€鍖栦负杩涚▼鍐呯紦瀛樸€?

---

### Windows锛圡SYS2 MinGW64锛?

**1. 瀹夎 MSYS2 渚濊禆**

```bash
pacman -S --needed \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-openssl \
    mingw-w64-x86_64-jsoncpp \
    mingw-w64-x86_64-zlib \
    mingw-w64-x86_64-postgresql \
    mingw-w64-x86_64-brotli \
    mingw-w64-x86_64-c-ares \
    mingw-w64-x86_64-libuuid \
    mingw-w64-x86_64-hiredis
```

**2. 缂栬瘧瀹夎 Drogon**

```bash
git clone https://github.com/drogonframework/drogon
cd drogon && git submodule update --init
mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_REDIS=ON \
    -DBUILD_MYSQL=OFF \
    -DBUILD_SQLITE=OFF \
    -DBUILD_POSTGRESQL=ON
ninja && ninja install
```

**3. 瀹夎 jwt-cpp锛圚eader-Only锛?*

```bash
git clone https://github.com/Thalhammer/jwt-cpp
cp -r jwt-cpp/include/jwt-cpp /mingw64/include/
```

**4. 缂栬瘧椤圭洰**

```bash
git clone https://gitee.com/ruoyicpp/ruoyi ruoyi-cpp
cd ruoyi-cpp && mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/mingw64
ninja
```

**5. 閰嶇疆骞惰繍琛?*

```bash
# 缂栬緫 config.json锛堟暟鎹簱杩炴帴銆丣WT 瀵嗛挜绛夛級
# 棣栨杩愯鑷姩寤鸿〃 + 鎻掑叆鍒濆鏁版嵁
./ruoyi-cpp.exe
```

---

### Linux锛圙CC锛?

**1. 瀹夎渚濊禆**

```bash
sudo apt install -y gcc g++ cmake make \
    libssl-dev libjsoncpp-dev libpq-dev zlib1g-dev \
    libbrotli-dev libc-ares-dev uuid-dev libhiredis-dev \
    libsqlite3-dev librocksdb-dev
```

**2. 缂栬瘧瀹夎 Drogon**锛堝悓 Windows 姝ラ锛岀暐鍘?`-G Ninja` 鍙敤榛樿 Makefiles锛?

```bash
git clone https://github.com/drogonframework/drogon
cd drogon && git submodule update --init
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_REDIS=ON -DBUILD_MYSQL=OFF -DBUILD_SQLITE=OFF -DBUILD_POSTGRESQL=ON
make -j$(nproc) && sudo make install
```

**3. 缂栬瘧椤圭洰**

```bash
git clone https://gitee.com/ruoyicpp/ruoyi ruoyi-cpp
cd ruoyi-cpp && mkdir build-Linux && cd build-Linux
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./ruoyi-cpp
```

> 鍙€夛細`-DRUOYI_USE_NGINX=ON` 鍚敤宓屽叆寮?Nginx锛堥渶鍏堟寜 `nginx-1.29.8-linux/` 璇存槑缂栬瘧 `libnginx.a`锛夛紱`-DRUOYI_BUILD_TESTS=ON` 缂栬瘧鍗曞厓娴嬭瘯銆?

---

## 閰嶇疆璇存槑

涓婚厤缃枃浠讹細`config.json`锛堝弬鑰?`build-nginx/config.template.json`锛?

### 鏍稿績閰嶇疆

```jsonc
{
  "listeners": [{ "address": "0.0.0.0", "port": 18080, "https": false }],
  "database": {
    "host": "127.0.0.1", "port": 5432,
    "dbname": "ruoyi.c", "user": "postgres", "passwd": ""
  },
  "jwt": {
    "secret": "鑷冲皯16浣嶉殢鏈哄瓧绗︿覆",  // 鈿狅笍 鐢熶骇鐜蹇呴』淇敼
    "expire_minutes": 30,
    "jwt_expire_days": 7
  }
}
```

### 鏂囦欢瀛樺偍锛堥粯璁ゆ湰鍦帮紝鍙€?MinIO/S3锛?

```jsonc
"storage": {
  "type":       "local",          // "local" | "minio" | "s3"
  "local_path": "./upload",       // type=local 鏃剁敓鏁?
  "endpoint":   "http://127.0.0.1:9000",  // MinIO/S3 endpoint
  "bucket":     "ruoyi",
  "access_key": "minioadmin",
  "secret_key": "minioadmin",
  "region":     "us-east-1",
  "public_url": ""                // 瀵瑰 CDN 鍦板潃锛岀┖鍒欑敤 endpoint
}
```

### LDAP/Active Directory

```jsonc
"ldap": {
  "enabled":      false,
  "host":         "192.168.1.100",
  "port":         389,
  "base_dn":      "DC=example,DC=com",
  "bind_dn":      "CN=svc_ruoyi,OU=Service Accounts,DC=example,DC=com",
  "bind_pass":    "service_password",
  "user_filter":  "(&(objectClass=person)(sAMAccountName={username}))",
  "fallback_local": true          // LDAP 澶辫触鏃跺厑璁告湰鍦拌处鍙风櫥褰?
}
```

### 涓ゆ楠岃瘉锛圱OTP锛?

```jsonc
"totp": {
  "enabled": true,
  "issuer":  "RuoYi-Cpp"          // 鏄剧ず鍦?Authenticator App 涓殑鍚嶇О
}
```

> TOTP 浣跨敤娴佺▼锛氳皟鐢?`POST /system/totp/generate` 鑾峰彇 `qrUri`锛岀敤鍓嶇 qrcode.js 娓叉煋浜岀淮鐮侊紝鐢ㄦ埛鐢?Google/Microsoft Authenticator 鎵爜锛岀劧鍚庤皟鐢?`POST /system/totp/enable` 杈撳叆 6 浣嶇爜婵€娲汇€?

### 绗笁鏂圭櫥褰曪紙OAuth2锛?

```jsonc
"oauth2": {
  "github": {
    "enabled":       true,
    "client_id":     "YOUR_GITHUB_CLIENT_ID",
    "client_secret": "YOUR_GITHUB_CLIENT_SECRET",
    "redirect_uri":  "http://yourdomain/oauth2/callback/github",
    "scope":         "user:email"
  },
  "google": { "enabled": false, ... },     // 鍚岀粨鏋勶紝scope: "openid email profile"
  "wechat_work": {                           // 浼佷笟寰俊闇€棰濆 corp_id / agent_id
    "enabled":       false,
    "corp_id":       "YOUR_CORP_ID",
    "client_secret": "YOUR_CORP_SECRET",
    "agent_id":      "YOUR_AGENT_ID",
    "redirect_uri":  "http://yourdomain/oauth2/callback/wechat_work"
  },
  "dingtalk": { "enabled": false, ... },   // client_id = AppKey
  "feishu":   { "enabled": false, ... },   // client_id = App ID
  "qq":       { "enabled": false, ... }    // client_id = App ID
}
```

**OAuth2 鐧诲綍瀹屾暣娴佺▼锛?*

1. 鍓嶇璋?`GET /oauth2/providers` 鑾峰彇宸插惎鐢?provider 鍒楄〃
2. 鍓嶇璋?`GET /oauth2/authorize/{provider}` 鑾峰彇 `{url, state}`
3. 鍓嶇璺宠浆鍒?`url`锛坧rovider 鎺堟潈椤碉級
4. 鐢ㄦ埛鎺堟潈鍚?provider 閲嶅畾鍚戝埌 `redirect_uri`锛堝嵆 `GET /oauth2/callback/{provider}?code=xxx&state=xxx`锛?
5. 鍚庣楠岃瘉 state锛堥槻 CSRF锛夆啋 鐢?code 鎹?access_token 鈫?鑾峰彇鐢ㄦ埛淇℃伅 鈫?绛惧彂 JWT
6. 棣栨鐧诲綍鑷姩鍒涘缓鏈湴璐﹀彿锛坄{provider}_{openId鍓?6浣峿`锛?
7. 宸茬櫥褰曠敤鎴峰彲閫氳繃 `POST /oauth2/bind/{provider}` 缁戝畾鐜版湁璐﹀彿

> **瀹夊叏鎻愮ず**锛歴tate 閫氳繃 `MemCache` 瀛樺偍 60s 鑷姩杩囨湡锛屾潨缁?CSRF 鏀诲嚮銆?

### 鍓嶇鍐呯疆鎵樼锛堟棤闇€ Nginx锛?

```jsonc
"frontend": {
  "enabled":      true,
  "dist_path":    "./web",         // Vue dist 鐩綍
  "spa_mode":     true,            // SPA history 妯″紡鍥為€€
  "api_prefix":   "/prod-api",     // 鑷姩鍓ョ璇ュ墠缂€杞彂鍒板悗绔?
  "cache_seconds": 3600
}
```

> 灏?`npm run build:prod` 鐢熸垚鐨?`dist/` 鍐呭鏀惧埌 `./web/` 鐩綍锛岀洿鎺ヨ闂?`:18080` 鍗冲彲锛屾棤闇€ Nginx銆?

### 鏁忔劅淇℃伅绠＄悊

鍙戝竷浠撳簱鏃讹紝鍚湡瀹炲瘑鐮佺殑閰嶇疆鏂囦欢涓嶄細琚彁浜わ細

| 鏂囦欢 | 璇存槑 |
|---|---|
| `build-nginx/config.json` | 鐪熷疄閰嶇疆锛岃 `.gitignore` 鎺掗櫎 |
| `build-nginx/ruoyi1.mymq.site.json` | 鐪熷疄閰嶇疆锛岃 `.gitignore` 鎺掗櫎 |
| `build-nginx/config.template.json` | 閰嶇疆妯℃澘锛屽惈鍗犱綅绗︼紙`YOUR_DATABASE_PASSWORD` 绛夛級锛?*浼氶殢 git 鎻愪氦** |

閮ㄧ讲鍒版柊鏈哄櫒鏃讹細

1. 鍏嬮殕浠撳簱鍚庯紝浠?`build-nginx/config.template.json` 澶嶅埗涓€浠戒负 `build-nginx/config.json`
2. 濉啓 `database.passwd`銆乣jwt.secret`銆乣security.*.admin_unlock_key` 绛夋晱鎰熷瓧娈?
3. 鎴栭€氳繃鐜鍙橀噺娉ㄥ叆锛坄RUOYI_DATABASE_PASSWD`銆乣RUOYI_JWT_SECRET` 绛夛級

鐜鍙橀噺浼樺厛绾ч珮浜庨厤缃枃浠讹紝璇﹁鍚勫瓧娈垫梺鐨?`_comment` 璇存槑銆?

### 閭欢閰嶇疆锛堢郴缁熷唴閰嶇疆锛?

鐧诲綍鍚庤繘鍏?**绯荤粺绠＄悊 鈫?閭欢鍙戜欢绠?* 閰嶇疆 SMTP锛屾棤闇€淇敼閰嶇疆鏂囦欢锛?

| 鍙傛暟閿?| 璇存槑 | 绀轰緥鍊?|
|--------|-----|--------|
| `sys.email.host` | SMTP 鏈嶅姟鍣?| `smtp.qq.com` |
| `sys.email.port` | 绔彛锛圛mplicit TLS锛墊 `465` |
| `sys.email.fromName` | 鍙戜欢浜烘樉绀哄悕 | `绯荤粺閫氱煡` |
| `sys.email.senders` | 鍙戜欢浜哄垪琛紙JSON 鏁扮粍锛墊 `[{"email":"a@qq.com","authCode":"xxxx"}]` |

### SQLite 鍔犲瘑锛堝彲閫夛級

椤圭洰鏀寔涓ょ SQLite 鍔犲瘑鏂瑰紡锛屽叡鐢ㄥ悓涓€閰嶇疆鍏ュ彛锛屾寜缂栬瘧閫夐」鑷姩閫夋嫨锛?

- **椤电骇鍔犲瘑**锛圫QLite3MC锛夛細纾佺洏濮嬬粓瀵嗘枃锛屾棤鏄庢枃绐楀彛锛涢渶 `scripts/download_sqlite3mc.ps1` 鎷夊彇 amalgamation
- **鏂囦欢绾у姞瀵?*锛圧YENC1锛変綔涓哄厹搴曪細AES-256-GCM + HMAC-SHA256 + 鑷爺灏佽鏍煎紡锛屼粎渚濊禆 OpenSSL

鏈€绠€閰嶇疆锛?

```jsonc
"sqlite": { "encrypt_key": "Your#Strong@Pass2026" }
```

瀹屾暣鏋舵瀯銆? 绉嶅瘑閽ユ潵婧愶紙hwid/vault/env/...锛夈€佽縼绉昏矾寰勩€丆LI 宸ュ叿銆佽繍缁?FAQ 瑙?[`docs/SQLITE_ENCRYPTION.md`](docs/SQLITE_ENCRYPTION.md)銆?

### 鍙娴嬫€?/ Prometheus 鎸囨爣

鍐呯疆 `/actuator/health`銆乣/actuator/metrics`銆乣/actuator/db`銆乣/actuator/shutdown` 绛夌鐐癸紝鏀寔 Prometheus 鎶撳彇銆?
鎸囨爣瀹氫箟銆丳romQL 鏌ヨ銆丟rafana 闈㈡澘銆佸憡璀﹁鍒欒 [`docs/OBSERVABILITY.md`](docs/OBSERVABILITY.md)銆?

---

## 椤圭洰缁撴瀯

```
ruoyi-cpp/
鈹溾攢鈹€ build-nginx/
鈹?  鈹溾攢鈹€ config.json                      # 涓婚厤缃枃浠讹紙涓嶉殢 git 鎻愪氦锛屾晱鎰熶俊鎭級
鈹?  鈹溾攢鈹€ config.template.json             # 閰嶇疆妯℃澘锛坓it 鎻愪氦锛屾晱鎰熷€肩敤鍗犱綅绗︼級
鈹?  鈹斺攢鈹€ ruoyi-cpp.exe                    # 缂栬瘧浜х墿
鈹溾攢鈹€ web/                             # 鍓嶇 dist 鐩綍锛堟斁杩欓噷鍗冲彲锛屾棤闇€ Nginx锛?
鈹溾攢鈹€ logs/                            # 鏃ュ織鐩綍锛?log 鏂囨湰 + .jsonl 缁撴瀯鍖栵級
鈹溾攢鈹€ upload/                          # 鏈湴涓婁紶鏂囦欢鐩綍
鈹溾攢鈹€ src/
鈹?  鈹溾攢鈹€ main.cc                      # 鏃у崟鏂囦欢鍏ュ彛锛堜繚鐣欏鐓э紝涓嶅弬涓庣紪璇戯級
鈹?  鈹溾攢鈹€ main/                        # 鍚姩妯″潡锛堟媶鍒嗚嚜鍘?main.cc锛?
鈹?  鈹?  鈹溾攢鈹€ main.cc                  # 鍏ュ彛锛氭寜闃舵涓茶仈鍚姩娴佺▼
鈹?  鈹?  鈹斺攢鈹€ main/                    # 鍚勫惎鍔ㄩ樁娈靛疄鐜帮紙boot::*锛?
鈹?  鈹?      鈹溾攢鈹€ AppBootstrap.h       #   AppContext 鍏变韩涓婁笅鏂?+ 闃舵鍑芥暟澹版槑
鈹?  鈹?      鈹溾攢鈹€ EarlyInit.cc         #   鏃╂湡鍒濆鍖栵細watchdog 绉讳氦 / 鍗曞疄渚嬮攣 / 缂栨帓鍣?
鈹?  鈹?      鈹溾攢鈹€ ConfigInit.cc        #   閰嶇疆鍔犺浇銆乴icense銆丏B 杩炴帴涓?
鈹?  鈹?      鈹溾攢鈹€ HttpSetup.cc         #   Drogon 鐩戝惉鍣?/ 涓棿浠?/ 杩囨护鍣?
鈹?  鈹?      鈹溾攢鈹€ RoutesSetup.cc       #   鍐呯疆璺敱娉ㄥ唽
鈹?  鈹?      鈹溾攢鈹€ CertRoutes.cc        #   璇佷功 / ACME 鐩稿叧璺敱
鈹?  鈹?      鈹溾攢鈹€ StartupAdvice.cc     #   beginningAdvice锛欴B 鍒濆鍖栥€佸閮ㄦ湇鍔?
鈹?  鈹?      鈹溾攢鈹€ RuntimeSetup.cc      #   杩愯鏃舵湇鍔★紙NginxEmbedded銆佸績璺筹級+ 娓呯悊
鈹?  鈹?      鈹斺攢鈹€ DbConnStr.cc         #   libpq 杩炴帴涓叉瀯閫?
鈹?  鈹溾攢鈹€ AppIncludes.h                # 鍏ㄥ眬闆嗕腑 include
鈹?  鈹溾攢鈹€ codegen/                     # 浠ｇ爜鐢熸垚妯″潡锛堢紪璇戜负鍔ㄦ€佸簱锛?
鈹?  鈹?  鈹溾攢鈹€ CMakeLists.txt           # 鍔ㄦ€佸簱缂栬瘧閰嶇疆
鈹?  鈹?  鈹溾攢鈹€ CodeGenerator.h/cc       # 浠ｇ爜鐢熸垚寮曟搸
鈹?  鈹?  鈹溾攢鈹€ DynamicCompiler.h/cc     # 鍔ㄦ€佺紪璇戝櫒锛圕Make + MinGW/GCC锛?
鈹?  鈹?  鈹溾攢鈹€ PluginManager.h/cc       # 鎻掍欢绠＄悊锛堝姞杞?鍗歌浇/璋冪敤锛?
鈹?  鈹?  鈹斺攢鈹€ controllers/
鈹?  鈹?      鈹斺攢鈹€ CodeGenCtrl.h/cc     # 浠ｇ爜鐢熸垚闈欐€佹柟娉曪紙琚姩鎬佸簱瀵煎嚭锛?
鈹?  鈹溾攢鈹€ common/
鈹?  鈹?  鈹溾攢鈹€ AjaxResult.h             # 缁熶竴 JSON 鍝嶅簲浣?
鈹?  鈹?  鈹溾攢鈹€ DatabaseInit.cc          # 鑷姩寤鸿〃 + 鍒濆鏁版嵁 + Schema 杩佺Щ
鈹?  鈹?  鈹溾攢鈹€ JwtUtils.h               # JWT 鐢熸垚/瑙ｆ瀽
鈹?  鈹?  鈹溾攢鈹€ JsonLogger.h             # JSON 缁撴瀯鍖栨棩蹇楋紙瑕嗙洊 Drogon 杈撳嚭锛?
鈹?  鈹?  鈹溾攢鈹€ RequestTracing.h         # X-Request-ID 閾捐矾杩借釜涓棿浠?
鈹?  鈹?  鈹溾攢鈹€ DataMaskUtils.h          # 鎵嬫満/韬唤璇?閾惰鍗?閭鑴辨晱
鈹?  鈹?  鈹溾攢鈹€ MetricsCollector.h       # Prometheus 鎸囨爣 + ActuatorCtrl
鈹?  鈹?  鈹溾攢鈹€ TotpUtils.h              # TOTP RFC 6238锛圙oogle Authenticator锛?
鈹?  鈹?  鈹溾攢鈹€ OAuth2Manager.h          # 绗笁鏂圭櫥褰曪細GitHub/Google/浼佷笟寰俊/閽夐拤/椋炰功/QQ
鈹?  鈹?  鈹溾攢鈹€ HotConfig.h              # 閰嶇疆鏂囦欢鐑噸杞斤紙5s 杞锛?
鈹?  鈹?  鈹溾攢鈹€ LdapAuth.h               # LDAP/Active Directory 璁よ瘉
鈹?  鈹?  鈹溾攢鈹€ FrontendHost.h           # Drogon 鍐呯疆鍓嶇鎵樼 + SPA 鍥為€€
鈹?  鈹?  鈹溾攢鈹€ RateLimiter.h            # IP 闄愭祦
鈹?  鈹?  鈹溾攢鈹€ XssUtils.h               # XSS 杩囨护 + SQL 娉ㄥ叆妫€娴?
鈹?  鈹?  鈹溾攢鈹€ SignUtils.h              # API 璇锋眰绛惧悕楠岃瘉
鈹?  鈹?  鈹溾攢鈹€ SslManager.h             # SSL 璇佷功绠＄悊
鈹?  鈹?  鈹溾攢鈹€ LicenseManager.h         # 杞欢璁稿彲璇佺鐞?
鈹?  鈹?  鈹溾攢鈹€ DeviceBinding.h          # 璁惧缁戝畾锛堢‖浠舵寚绾癸級
鈹?  鈹?  鈹溾攢鈹€ SmtpUtils.h              # SMTP 閭欢鍙戦€侊紙OpenSSL Implicit-TLS锛?
鈹?  鈹?  鈹溾攢鈹€ MonitorManager.h         # 宕╂簝/閲嶅惎鍛婅
鈹?  鈹?  鈹溾攢鈹€ CrashHandler.h           # 宕╂簝鎹曡幏锛圫EH/VEH/terminate锛?
鈹?  鈹?  鈹斺攢鈹€ ColorLogger.h            # 鎺у埗鍙板僵鑹叉棩蹇?
鈹?  鈹溾攢鈹€ filters/
鈹?  鈹?  鈹溾攢鈹€ JwtAuthFilter.h          # JWT 璁よ瘉涓棿浠讹紙HttpMiddleware锛?
鈹?  鈹?  鈹斺攢鈹€ PermFilter.h             # 鏉冮檺妫€鏌ュ畯 CHECK_PERM
鈹?  鈹溾攢鈹€ services/
鈹?  鈹?  鈹溾攢鈹€ DatabaseService.h        # PostgreSQL(姹? + SQLite 鍙屽啓/鑷姩闄嶇骇
鈹?  鈹?  鈹溾攢鈹€ StorageService.h         # 鏂囦欢瀛樺偍锛氭湰鍦?MinIO/S3锛圫igV4锛?
鈹?  鈹?  鈹溾攢鈹€ VaultManager.h           # HashiCorp Vault 闆嗘垚
鈹?  鈹?  鈹溾攢鈹€ NginxManager.h           # Nginx 瀛愯繘绋嬬鐞?
鈹?  鈹?  鈹斺攢鈹€ ...                      # KoboldCpp/Whisper/DDNS 绛夋墿灞曟湇鍔?
鈹?  鈹溾攢鈹€ system/
鈹?  鈹?  鈹溾攢鈹€ services/
鈹?  鈹?  鈹?  鈹溾攢鈹€ TokenService.h       # Token 鍒涘缓/鍒锋柊/鍒犻櫎
鈹?  鈹?  鈹?  鈹溾攢鈹€ SysConfigService.h   # 绯荤粺鍙傛暟锛堝甫缂撳瓨锛?
鈹?  鈹?  鈹?  鈹溾攢鈹€ SysDictService.h     # 瀛楀吀缂撳瓨
鈹?  鈹?  鈹?  鈹斺攢鈹€ SysMenuService.h     # 鑿滃崟鏍?璺敱鏋勫缓
鈹?  鈹?  鈹斺攢鈹€ controllers/
鈹?  鈹?      鈹溾攢鈹€ SysLoginCtrl.h       # 鐧诲綍/娉ㄥ唽/蹇樿瀵嗙爜/璺敱
鈹?  鈹?      鈹溾攢鈹€ SysUserCtrl.h        # 鐢ㄦ埛绠＄悊
鈹?  鈹?      鈹溾攢鈹€ SysRoleCtrl.h        # 瑙掕壊绠＄悊锛堝疄鏃舵潈闄愬埛鏂帮級
鈹?  鈹?      鈹溾攢鈹€ SysTotpCtrl.h        # TOTP 涓ゆ楠岃瘉 API
鈹?  鈹?      鈹溾攢鈹€ OAuth2Ctrl.h         # 绗笁鏂圭櫥褰曪細鎺堟潈/鍥炶皟/缁戝畾/瑙ｇ粦
鈹?  鈹?      鈹斺攢鈹€ ...                  # 鑿滃崟/閮ㄩ棬/瀛楀吀/鍏憡绛?
鈹?  鈹斺攢鈹€ monitor/
鈹?      鈹溾攢鈹€ JobScheduler.h           # Cron 璋冨害寮曟搸锛堟敮鎸佺绾?cron 琛ㄨ揪寮忥級
鈹?      鈹斺攢鈹€ controllers/
鈹?          鈹溾攢鈹€ SysLogFileCtrl.h     # 绯荤粺鏃ュ織鏂囦欢鏌ョ湅鍣?
鈹?          鈹溾攢鈹€ SysJobCtrl.h         # 瀹氭椂浠诲姟绠＄悊
鈹?          鈹溾攢鈹€ ServerCtrl.h         # 鏈嶅姟鍣ㄧ洃鎺?
鈹?          鈹溾攢鈹€ DruidCtrl.h          # 鏁版嵁搴撹繛鎺ユ睜鐩戞帶
鈹?          鈹溾攢鈹€ SysRestartCtrl.h     # 閲嶅惎鍚庣鏈嶅姟锛堝唴缃?HTML 绠＄悊椤碉紝admin 閴存潈锛?
鈹?          鈹斺攢鈹€ ...                  # 鎿嶄綔鏃ュ織/鐧诲綍鏃ュ織/鍦ㄧ嚎鐢ㄦ埛
鈹斺攢鈹€ ui/                              # 鍓嶇婧愮爜锛圴ue 2 + Element UI锛?
```

---

## 鏉冮檺璁捐

- **瓒呯骇绠＄悊鍛?*锛坄user_id=1`锛夛細鎷ユ湁鎵€鏈夋潈闄愶紝涓嶅彈 RBAC 闄愬埗
- **鏅€氱敤鎴?*锛氶€氳繃 `sys_user_role` 鍏宠仈瑙掕壊锛岃鑹插叧鑱旇彍鍗曟潈闄?
- **鑿滃崟鏉冮檺瀛楃**锛氬 `system:user:list`锛屽湪 `CHECK_PERM` 瀹忎腑鑷姩鏍￠獙
- **瑙掕壊鏉冮檺瀹炴椂鐢熸晥**锛氫慨鏀硅鑹茶彍鍗曞悗锛?*鏃犻渶鍦ㄧ嚎鐢ㄦ埛閲嶆柊鐧诲綍**锛屽悗绔嚜鍔ㄥ埛鏂?Token 鏉冮檺缂撳瓨鍜岃矾鐢辩紦瀛?

### 娉ㄥ唽鐢ㄦ埛鏉冮檺

閫氳繃绯荤粺鍙傛暟 `sys.account.initRoleId` 鎺у埗锛?

| 鍙傛暟鍊?| 鏁堟灉 |
|--------|-----|
| 绌猴紙榛樿锛?| 娉ㄥ唽鍚庢棤瑙掕壊锛岀鐞嗗憳鎵嬪姩鍒嗛厤 |
| 瑙掕壊 ID锛堝 `2`锛?| 娉ㄥ唽鍚庤嚜鍔ㄥ垎閰嶆寚瀹氳鑹?|

---

## 榛樿璐﹀彿

| 鐢ㄦ埛鍚?| 瀵嗙爜 | 璇存槑 |
|--------|------|-----|
| `admin` | `admin123` | 瓒呯骇绠＄悊鍛橈紝鎷ユ湁鍏ㄩ儴鏉冮檺 |

> 鈿狅笍 **鐢熶骇鐜璇风珛鍗充慨鏀归粯璁ゅ瘑鐮侊紒**

瀵嗙爜浣跨敤 OpenSSL PBKDF2-SHA256锛?0000 杞級瀛樺偍锛宐crypt 鍙€夈€?

---

## 鐢ㄦ埛鎵归噺瀵煎叆

鏀寔閫氳繃 CSV 鏂囦欢鎵归噺瀵煎叆鐢ㄦ埛锛?*绯荤粺绠＄悊 鈫?鐢ㄦ埛绠＄悊 鈫?瀵煎叆**锛夛細

1. 鐐瑰嚮"涓嬭浇妯℃澘"鑾峰彇 CSV 鏍煎紡妯℃澘
2. 鎸夋ā鏉垮～鍐欑敤鎴锋暟鎹紙榛樿瀵嗙爜 `123456`锛?
3. 鍕鹃€?鏄惁鏇存柊"鍙鐩栧凡鏈夎处鍙?
4. 涓婁紶 CSV 鏂囦欢锛屾敮鎸?UTF-8锛堝惈 BOM锛夌紪鐮?

CSV 鍒楁牸寮忥細`鐧诲綍璐﹀彿, 鐢ㄦ埛鏄电О, 閮ㄩ棬缂栧彿, 鎵嬫満鍙风爜, 閭, 鎬у埆(0/1/2), 鐘舵€?0/1)`

---

## 鍓嶇璇存槑

**鍓嶇鐩存帴浣跨敤鑻ヤ緷瀹樻柟婧愮爜锛?*

```bash
# 鍏嬮殕鑻ヤ緷瀹樻柟鍓嶇
git clone https://gitee.com/y_project/RuoYi-Vue.git
cd RuoYi-Vue

# 淇敼 .env.development 涓殑鍚庣鍦板潃
VUE_APP_BASE_API = 'http://127.0.0.1:18080'

# 瀹夎渚濊禆骞跺惎鍔?
npm install
npm run dev
```

鐢熶骇閮ㄧ讲鏃跺皢 `npm run build:prod` 鐢熸垚鐨?`dist/` 鐩綍閮ㄧ讲鍒?Nginx 鍗冲彲锛屽悗绔湴鍧€鎸囧悜鏈」鐩殑鐩戝惉绔彛锛堥粯璁?`18080`锛夈€?

**Nginx 浼潤鎬?+ 鍙嶅悜浠ｇ悊閰嶇疆锛?*

```nginx
# Vue 璺敱 history 妯″紡浼潤鎬?
location / {
    try_files $uri $uri/ /index.html;
}

# 鍚庣 API 浠ｇ悊锛堝搴斿墠绔?VUE_APP_BASE_API = '/prod-api'锛?
location /prod-api/ {
    proxy_pass http://127.0.0.1:18080/;
    proxy_set_header Host              $host;
    proxy_set_header X-Real-IP         $remote_addr;
    proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
    proxy_connect_timeout 60s;
    proxy_send_timeout    60s;
    proxy_read_timeout    60s;
}

# WebSocket 閫氱煡
location /ws/ {
    proxy_pass http://127.0.0.1:18080/ws/;
    proxy_http_version 1.1;
    proxy_set_header Upgrade    $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_set_header Host       $host;
    proxy_read_timeout 3600s;
}
```

---

## 涓庡師鐗?RuoYi-Vue 鐨勫吋瀹规€?

- 鉁?鎵€鏈?`/system/**`銆乣/monitor/**` API 璺敱涓庡師鐗堝畬鍏ㄤ竴鑷?
- 鉁?JWT Token 鏍煎紡銆乣getInfo`銆乣getRouters` 鍝嶅簲缁撴瀯瀹屽叏鍏煎
- 鉁?鐩存帴鍏嬮殕[鑻ヤ緷瀹樻柟鍓嶇](https://gitee.com/y_project/RuoYi-Vue)锛屽彧鏀瑰悗绔湴鍧€鍗冲彲杩愯
- 鉃?鏂板锛氶偖浠跺彂浠剁绠＄悊銆佸繕璁板瘑鐮併€佹敞鍐岄偖绠遍獙璇佺爜銆佹秷鎭€氱煡涓績銆丄PI Key 绠＄悊銆佹搷浣滃璁″寮虹瓑鍔熻兘

---

## 閮ㄧ讲鏈€浣冲疄璺?

### 鐢熶骇鐜閮ㄧ讲娓呭崟

- [ ] **淇敼榛樿瀵嗙爜** - 绔嬪嵆淇敼 `admin` 璐﹀彿鐨?`admin123` 瀵嗙爜
- [ ] **璁剧疆寮?JWT Secret** - 鑷冲皯 32 浣嶉殢鏈哄瓧绗︿覆锛屼娇鐢?`/dev/urandom` 鎴栧瘑閽ョ鐞嗘湇鍔＄敓鎴?
- [ ] **鍚敤 HTTPS** - 閰嶇疆 SSL 璇佷功锛岃缃?`listeners[].https=true`
- [ ] **閰嶇疆鏁版嵁搴?* - 浣跨敤 PostgreSQL 涓诲簱锛孲QLite 浠呬綔涓哄浠介檷绾?
- [ ] **鍚敤鏃ュ織** - 閰嶇疆鏃ュ織绾у埆涓?`INFO`锛屽畾鏈熻疆杞棩蹇楁枃浠?
- [ ] **璁剧疆鐩戞帶鍛婅** - 閰嶇疆 Prometheus + Grafana锛岀洃鎺?CPU/鍐呭瓨/纾佺洏
- [ ] **鍚敤瀹¤鏃ュ織** - 璁板綍鎵€鏈夌敤鎴锋搷浣滐紝瀹氭湡瀵煎嚭澶囦唤
- [ ] **閰嶇疆澶囦唤绛栫暐** - 鏁版嵁搴撴瘡鏃ュ浠斤紝寮傚湴瀛樺偍
- [ ] **闄愬埗璁块棶** - 浣跨敤闃茬伀澧欓檺鍒?`/actuator/*` 绔偣浠呭唴缃戣闂?
- [ ] **鍚敤閫熺巼闄愬埗** - 閰嶇疆 `rateLimiter.enabled=true`锛岄槻姝?DDoS

### Docker 閮ㄧ讲

```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libpq5 libssl3
COPY ruoyi-cpp /app/ruoyi-cpp
COPY config.json /app/config.json
WORKDIR /app
EXPOSE 18080
CMD ["./ruoyi-cpp"]
```

```bash
# 鏋勫缓闀滃儚
docker build -t ruoyi-cpp:latest .

# 杩愯瀹瑰櫒
docker run -d \
  --name ruoyi-cpp \
  -p 18080:18080 \
  -v /data/config.json:/app/config.json \
  -v /data/logs:/app/logs \
  -v /data/upload:/app/upload \
  ruoyi-cpp:latest
```

### Kubernetes 閮ㄧ讲

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: ruoyi-cpp
spec:
  replicas: 3
  selector:
    matchLabels:
      app: ruoyi-cpp
  template:
    metadata:
      labels:
        app: ruoyi-cpp
    spec:
      containers:
      - name: ruoyi-cpp
        image: ruoyi-cpp:latest
        ports:
        - containerPort: 18080
        livenessProbe:
          httpGet:
            path: /actuator/health
            port: 18080
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          httpGet:
            path: /actuator/health
            port: 18080
          initialDelaySeconds: 10
          periodSeconds: 5
        resources:
          requests:
            memory: "64Mi"
            cpu: "100m"
          limits:
            memory: "512Mi"
            cpu: "500m"
```

---

## 鎬ц兘浼樺寲寤鸿

| 浼樺寲椤?| 寤鸿 | 鏁堟灉 |
|--------|------|------|
| **鏁版嵁搴撹繛鎺ユ睜** | 閰嶇疆 `database.pool_size=20`锛屾牴鎹苟鍙戞暟璋冩暣 | 鎻愬崌 30-50% QPS |
| **缂撳瓨绛栫暐** | 鍚敤 Redis锛岄厤缃儹鏁版嵁缂撳瓨锛堢敤鎴枫€佽鑹层€佽彍鍗曪級 | 闄嶄綆 DB 鍘嬪姏 80% |
| **寮傛澶勭悊** | 浣跨敤 Drogon 寮傛鍥炶皟锛岄伩鍏嶉樆濉炴搷浣?| 鎻愬崌 2-3 鍊嶅悶鍚愰噺 |
| **鏃ュ織绾у埆** | 鐢熶骇鐜璁剧疆 `WARN` 绾у埆锛屽噺灏?I/O | 鎻愬崌 10-15% 鎬ц兘 |
| **鍓嶇璧勬簮** | 鍚敤 gzip 鍘嬬缉锛孋DN 鍒嗗彂闈欐€佽祫婧?| 鍑忓皯 70% 甯﹀ |
| **鏁版嵁搴撶储寮?* | 涓哄父鐢ㄦ煡璇㈠瓧娈靛缓绔嬬储寮曪紙username銆乪mail 绛夛級 | 鏌ヨ蹇?10-100 鍊?|
| **杩炴帴澶嶇敤** | 鍚敤 HTTP Keep-Alive锛屽鐢?TCP 杩炴帴 | 鍑忓皯寤惰繜 50% |

---

## 鏁呴殰鎺掓煡鎸囧崡

### 鍚姩澶辫触

```bash
# 鏌ョ湅璇︾粏鏃ュ織
tail -f logs/ruoyi-cpp.log

# 甯歌閿欒锛?
# 1. "cannot connect to database"
#    鈫?妫€鏌?PostgreSQL 鏄惁杩愯锛歱sql -U postgres
#    鈫?妫€鏌ヨ繛鎺ュ瓧绗︿覆锛歨ost/port/dbname/user/passwd

# 2. "Address already in use"
#    鈫?绔彛琚崰鐢紝淇敼 config.json 涓殑 listeners[].port

# 3. "Permission denied"
#    鈫?妫€鏌ユ枃浠舵潈闄愶細chmod +x ruoyi-cpp
#    鈫?妫€鏌ユ棩蹇楃洰褰曪細mkdir -p logs && chmod 755 logs
```

### 鎬ц兘闂

```bash
# 鐩戞帶 CPU/鍐呭瓨
GET /monitor/server

# 鏌ョ湅鏁版嵁搴撹繛鎺ョ姸鎬?
GET /actuator/db

# 鏌ョ湅 Prometheus 鎸囨爣
GET /actuator/metrics

# 妫€鏌ユ參鏌ヨ锛圥ostgreSQL锛?
SELECT query, mean_time FROM pg_stat_statements 
ORDER BY mean_time DESC LIMIT 10;
```

### 鏉冮檺闂

```bash
# 妫€鏌ョ敤鎴锋潈闄?
SELECT u.username, r.role_name, m.menu_name 
FROM sys_user u
LEFT JOIN sys_user_role ur ON u.user_id = ur.user_id
LEFT JOIN sys_role r ON ur.role_id = r.role_id
LEFT JOIN sys_role_menu rm ON r.role_id = rm.role_id
LEFT JOIN sys_menu m ON rm.menu_id = m.menu_id
WHERE u.username = 'admin';

# 鍒锋柊鏉冮檺缂撳瓨
POST /actuator/reload
```

---

## 甯歌闂

**Q锛氬惎鍔ㄦ姤 `cannot connect to database`锛?*
> 妫€鏌?`config.json` 涓?`database.host/port/dbname/user/passwd` 鏄惁姝ｇ‘锛岀‘璁?PostgreSQL 鏈嶅姟宸插惎鍔ㄣ€備娇鐢?SQLite 妯″紡鏃朵繚鎸?`host` 涓虹┖鍗冲彲銆?

**Q锛氱櫥褰曟彁绀洪獙璇佺爜閿欒锛?*
> 纭 `captcha.enabled` 涓?`true`锛屼笖绯荤粺鏃堕棿姝ｇ‘锛堥獙璇佺爜鏈?120 绉掓湁鏁堟湡锛夈€?

**Q锛氬墠绔法鍩熸姤閿欙紵**
> 寮€鍙戞ā寮忎笅妫€鏌?`vue.config.js` 涓?`devServer.proxy` 鐨勭洰鏍囧湴鍧€鏄惁鎸囧悜姝ｇ‘鐨勫悗绔鍙ｏ紙榛樿 `18080`锛夈€傜敓浜х幆澧冩鏌?Nginx `/prod-api/` 浠ｇ悊閰嶇疆銆?

**Q锛欽WT secret 涓虹┖鑳藉惎鍔ㄥ悧锛?*
> 鍙互鍚姩锛屼絾鎵€鏈?Token 灏嗕娇鐢ㄧ┖瀵嗛挜绛惧彂锛?*瀛樺湪涓ラ噸瀹夊叏椋庨櫓**锛岀敓浜х幆澧冨姟蹇呭～鍐欏己闅忔満瀵嗛挜銆?

**Q锛氶厤缃慨鏀瑰悗闇€瑕侀噸鍚湇鍔″悧锛?*
> 涓嶉渶瑕併€傜▼搴忓唴缃?`HotConfig` 鐩戣鍣紝姣?5 绉掓娴?`config.json` 鐨勪慨鏀规椂闂达紝鍙樺寲鏃惰嚜鍔ㄩ噸杞?JWT 閰嶇疆鍜岃皟鐢?`onReload` 鍥炶皟锛屾棤闇€閲嶅惎銆?
> 涔熷彲閫氳繃 `POST /actuator/reload` 鎵嬪姩瑙﹀彂绔嬪嵆閲嶈浇銆?

**Q锛氬浣曟煡鐪?API 鏂囨。锛?*
> 鍚姩鍚庤闂?`http://localhost:18080/swagger-ui/index.html`锛堝姞杞借嚜 CDN锛屾棤闇€棰濆閰嶇疆锛夛紝鎴栫洿鎺ヨ姹?`GET /v3/api-docs` 鑾峰彇 OpenAPI 3.0 JSON 瑙勮寖銆?

**Q锛氳鑹叉潈闄愪慨鏀瑰悗涓嶇敓鏁堬紵**
> 鍚庣浼氳嚜鍔ㄥ埛鏂板湪绾跨敤鎴风殑鏉冮檺缂撳瓨锛岃嫢浠嶄笉鐢熸晥璇锋鏌?`MemCache` / Redis 杩炴帴鏄惁姝ｅ父銆?

**Q锛氬浣曞湪鐢熶骇鐜鍚敤 HTTPS锛?*
> 鍦?`config.json` 涓厤缃細
> ```json
> "listeners": [{
>   "address": "0.0.0.0",
>   "port": 443,
>   "https": true,
>   "cert_file": "/path/to/cert.crt",
>   "key_file": "/path/to/key.key"
> }]
> ```

**Q锛氬浣曠洃鎺у簲鐢ㄦ€ц兘锛?*
> 璁块棶 `/actuator/metrics` 鑾峰彇 Prometheus 鏍煎紡鎸囨爣锛屾帴鍏?Grafana 鍙鍖栥€傛垨璁块棶 `/monitor/server` 鏌ョ湅瀹炴椂鏈嶅姟鍣ㄧ姸鎬併€?

**Q锛氭敮鎸侀泦缇ら儴缃插悧锛?*
> 鏀寔銆傞厤缃涓?Worker 杩涚▼锛屼娇鐢?Nginx 璐熻浇鍧囪　銆傜▼搴忎細鑷姩鐢熸垚 `upstream.conf`锛岄厤缃墍鏈?Worker 鑺傜偣銆?

---

---

## 寮€鍙戣€呮寚鍗?

### 娣诲姞鏂扮殑 API 绔偣

**1. 鍒涘缓鎺у埗鍣?*

```cpp
// src/system/controllers/MyCtrl.h
#pragma once
#include <drogon/HttpController.h>
#include "../../common/AjaxResult.h"
#include "../../filters/PermFilter.h"

class MyCtrl : public drogon::HttpController<MyCtrl> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MyCtrl::list, "/my/list", drogon::Get, "JwtAuthFilter");
        ADD_METHOD_TO(MyCtrl::add, "/my/add", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr &req, 
              std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "my:list");
        // 瀹炵幇閫昏緫
        RESP_OK(cb, Json::Value());
    }

    void add(const drogon::HttpRequestPtr &req,
             std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        CHECK_PERM(req, cb, "my:add");
        auto body = req->getJsonObject();
        // 瀹炵幇閫昏緫
        RESP_OK(cb, Json::Value());
    }
};
```

**2. 鍦?AppIncludes.h 涓寘鍚?*

```cpp
#include "system/controllers/MyCtrl.h"
```

**3. 娣诲姞鏉冮檺瀛楃涓插埌鏁版嵁搴?*

```sql
INSERT INTO sys_menu (menu_name, parent_id, order_num, path, component, is_frame, is_cache, menu_type, visible, status, perms, icon, create_time)
VALUES ('鎴戠殑鍔熻兘', 1, 100, 'my', 'system/my/index', 1, 0, 'C', '0', '0', 'my:list,my:add', 'list', NOW());
```

### 娣诲姞鏂扮殑鏁版嵁搴撹〃

**1. 鍒涘缓琛?*

```sql
CREATE TABLE my_table (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    description TEXT,
    status SMALLINT DEFAULT 0,
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_my_table_name ON my_table(name);
```

**2. 鍒涘缓瀵瑰簲鐨?Model 绫?*

```cpp
// src/models/MyTable.h
#pragma once
#include <string>
#include <ctime>

struct MyTable {
    long id;
    std::string name;
    std::string description;
    int status;
    std::string createTime;
    std::string updateTime;
};
```

### 娣诲姞瀹氭椂浠诲姟

```cpp
// src/monitor/JobScheduler.h 涓坊鍔?
void scheduleMyTask() {
    // 姣忓ぉ 02:00 鎵ц
    drogon::app().getLoop()->runAt(
        std::chrono::system_clock::now() + std::chrono::hours(2),
        [this]() {
            LOG_INFO << "鎵ц瀹氭椂浠诲姟";
            // 浠诲姟閫昏緫
        }
    );
}
```

---

## 鏁版嵁搴撴灦鏋?

### 鏍稿績琛ㄧ粨鏋?

```
sys_user (鐢ㄦ埛琛?
鈹溾攢鈹€ user_id (PK)
鈹溾攢鈹€ username (UK)
鈹溾攢鈹€ password (PBKDF2-SHA256)
鈹溾攢鈹€ email (UK)
鈹溾攢鈹€ phonenumber
鈹溾攢鈹€ sex
鈹溾攢鈹€ avatar
鈹溾攢鈹€ status
鈹溾攢鈹€ del_flag
鈹斺攢鈹€ create_time

sys_role (瑙掕壊琛?
鈹溾攢鈹€ role_id (PK)
鈹溾攢鈹€ role_name (UK)
鈹溾攢鈹€ role_key (UK)
鈹溾攢鈹€ role_sort
鈹溾攢鈹€ status
鈹斺攢鈹€ create_time

sys_menu (鑿滃崟琛?
鈹溾攢鈹€ menu_id (PK)
鈹溾攢鈹€ menu_name
鈹溾攢鈹€ parent_id (FK)
鈹溾攢鈹€ order_num
鈹溾攢鈹€ path
鈹溾攢鈹€ component
鈹溾攢鈹€ perms (鏉冮檺瀛楃涓?
鈹溾攢鈹€ icon
鈹溾攢鈹€ menu_type (C/M/F)
鈹斺攢鈹€ visible

sys_user_role (鐢ㄦ埛-瑙掕壊鍏宠仈)
鈹溾攢鈹€ user_id (FK)
鈹斺攢鈹€ role_id (FK)

sys_role_menu (瑙掕壊-鑿滃崟鍏宠仈)
鈹溾攢鈹€ role_id (FK)
鈹斺攢鈹€ menu_id (FK)

sys_oper_log (鎿嶄綔鏃ュ織)
鈹溾攢鈹€ oper_id (PK)
鈹溾攢鈹€ user_id (FK)
鈹溾攢鈹€ oper_module
鈹溾攢鈹€ oper_type
鈹溾攢鈹€ oper_url
鈹溾攢鈹€ oper_method
鈹溾攢鈹€ request_method
鈹溾攢鈹€ oper_param
鈹溾攢鈹€ oper_result
鈹溾攢鈹€ error_msg
鈹溾攢鈹€ oper_time
鈹斺攢鈹€ cost_time

sys_login_log (鐧诲綍鏃ュ織)
鈹溾攢鈹€ info_id (PK)
鈹溾攢鈹€ user_id (FK)
鈹溾攢鈹€ login_name
鈹溾攢鈹€ ipaddr
鈹溾攢鈹€ login_location
鈹溾攢鈹€ browser
鈹溾攢鈹€ os
鈹溾攢鈹€ status
鈹溾攢鈹€ msg
鈹斺攢鈹€ login_time
```

### 鏁版嵁搴撹繛鎺ョ鐞?

```cpp
// 浣跨敤 DatabaseService 杩涜鏌ヨ
auto& db = DatabaseService::instance();

// 鎵ц鏌ヨ
auto res = db.query("SELECT * FROM sys_user WHERE user_id = $1", userId);
if (res.ok() && res.rows() > 0) {
    std::string username = res.str(0, 1);
}

// 鎵ц鏇存柊
auto updateRes = db.execute(
    "UPDATE sys_user SET status = $1 WHERE user_id = $2",
    status, userId
);
```

---

## 瀹夊叏鏈€浣冲疄璺?

### 瀵嗙爜瀹夊叏

```cpp
// 瀵嗙爜鍝堝笇锛圥BKDF2-SHA256锛?0000 杞級
std::string hashedPwd = SecurityUtils::hashPassword(plainPassword);

// 瀵嗙爜楠岃瘉
bool isValid = SecurityUtils::verifyPassword(plainPassword, hashedPwd);
```

### 璇锋眰绛惧悕楠岃瘉

```cpp
// 鍦?config.json 涓惎鐢?
"security": {
  "sign_enabled": true,
  "sign_secret": "your-secret-key",
  "sign_expire_seconds": 300
}

// 瀹㈡埛绔敓鎴愮鍚?
std::string signature = SignUtils::generateSignature(params, secret);

// 鏈嶅姟绔獙璇?
bool isValid = SignUtils::verifySignature(params, signature, secret);
```

### 鏁版嵁鑴辨晱

```cpp
// 鑷姩鑴辨晱鏁忔劅瀛楁
Json::Value response;
response["user"] = user;
DataMaskUtils::maskJsonValue(response);  // 鑷姩鑴辨晱 phone/email/idcard 绛?
```

### XSS 闃叉姢

```cpp
// 杩囨护鐢ㄦ埛杈撳叆
std::string cleanInput = XssUtils::filterXss(userInput);

// SQL 娉ㄥ叆妫€娴?
if (XssUtils::detectSqlInjection(userInput)) {
    return RESP_ERR(cb, "闈炴硶杈撳叆");
}
```

---

## 鎬ц兘鍩哄噯娴嬭瘯

### 娴嬭瘯鐜

- **CPU**: Intel Core i7-9700K (8 鏍?
- **鍐呭瓨**: 16GB DDR4
- **鏁版嵁搴?*: PostgreSQL 12
- **骞跺彂鏁?*: 100-1000

### 娴嬭瘯缁撴灉

| 鍦烘櫙 | QPS | 骞冲潎寤惰繜 | P99 寤惰繜 | 鍐呭瓨鍗犵敤 |
|------|-----|---------|---------|---------|
| 鐢ㄦ埛鍒楄〃鏌ヨ | 8500 | 11ms | 45ms | 45MB |
| 鐢ㄦ埛鍒涘缓 | 3200 | 30ms | 120ms | 48MB |
| 鏉冮檺妫€鏌?| 15000 | 6ms | 20ms | 42MB |
| 鐧诲綍 | 1800 | 55ms | 200ms | 52MB |
| 鏂囦欢涓婁紶 (10MB) | 120 | 8.3s | 9.5s | 150MB |

### 鍘嬪姏娴嬭瘯鍛戒护

```bash
# 浣跨敤 Apache Bench
ab -n 10000 -c 100 http://localhost:18080/system/user/list

# 浣跨敤 wrk
wrk -t4 -c100 -d30s http://localhost:18080/system/user/list

# 浣跨敤 hey
hey -n 10000 -c 100 http://localhost:18080/system/user/list
```

---

## 鐗堟湰鍗囩骇鎸囧崡

### 浠?v1.2.x 鍗囩骇鍒?v1.3.3

**1. 澶囦唤鏁版嵁搴?*

```bash
pg_dump -U postgres ruoyi.c > backup_v1.2.x.sql
```

**2. 鍋滄鏃х増鏈?*

```bash
pkill -f ruoyi-cpp
```

**3. 鏇存柊鍙墽琛屾枃浠?*

```bash
# 涓嬭浇鏂扮増鏈?
wget https://gitee.com/ruoyicpp/ruoyi/releases/download/v1.3.3/ruoyi-cpp-v1.3.3-windows.zip
unzip ruoyi-cpp-v1.3.3-windows.zip
```

**4. 鏇存柊閰嶇疆鏂囦欢**

```bash
# 姣旇緝鏂版棫 config.json锛屽悎骞舵柊澧為厤缃」
diff config.json.old config.json.new
```

**5. 鏁版嵁搴撹縼绉?*

```sql
-- 鏂板琛ㄥ拰瀛楁锛堣嚜鍔ㄦ墽琛岋級
-- 绋嬪簭鍚姩鏃朵細鑷姩妫€娴嬪苟鎵ц杩佺Щ鑴氭湰
```

**6. 鍚姩鏂扮増鏈?*

```bash
./ruoyi-cpp
```

**7. 楠岃瘉鍗囩骇**

```bash
# 妫€鏌ョ増鏈?
curl http://localhost:18080/version

# 妫€鏌ュ仴搴风姸鎬?
curl http://localhost:18080/actuator/health
```

### 鍥炴粴姝ラ

```bash
# 1. 鍋滄褰撳墠鐗堟湰
pkill -f ruoyi-cpp

# 2. 鎭㈠澶囦唤
psql -U postgres ruoyi.c < backup_v1.2.x.sql

# 3. 鎭㈠鏃х増鏈彲鎵ц鏂囦欢
cp ruoyi-cpp.v1.2.x ./ruoyi-cpp

# 4. 鍚姩鏃х増鏈?
./ruoyi-cpp
```

---

## 璐＄尞鎸囧崡

娆㈣繋鎻愪氦 Issue 鍜?Pull Request锛?

1. Fork 鏈粨搴擄細<https://gitee.com/ruoyicpp/ruoyi>
2. 鏂板缓鍒嗘敮锛歚git checkout -b feat/your-feature`
3. 鎻愪氦浠ｇ爜锛歚git commit -m "feat: 鎻忚堪浣犵殑鏀瑰姩"`
4. 鎺ㄩ€佸垎鏀細`git push origin feat/your-feature`
5. 鍙戣捣 Pull Request

**浠ｇ爜瑙勮寖**锛?
- C++ 浠ｇ爜閬靛惊椤圭洰鐜版湁椋庢牸锛堝ご鏂囦欢瀹炵幇銆丏rogon 寮傛鍥炶皟锛?
- 鏂板鎺ュ彛闇€鍚屾椂鎻愪緵鏉冮檺瀛楃涓诧紙濡?`system:user:add`锛?
- 鏁忔劅淇℃伅涓嶅緱纭紪鐮侊紝閫氳繃 `config.json` 鎴栨暟鎹簱閰嶇疆
- 鏂板鍔熻兘闇€鎻愪緵鍗曞厓娴嬭瘯
- 鎻愪氦鍓嶈繍琛?`clang-format` 鏍煎紡鍖栦唬鐮?

---

## 瀹夊叏娉ㄦ剰浜嬮」

| 椤圭洰 | 璇存槑 |
|------|-----|
| JWT Secret | 蹇呴』 鈮?6 浣嶉殢鏈哄瓧绗︿覆锛岀敓浜х幆澧冭鐢?`/dev/urandom` 鐢熸垚 |
| 榛樿瀵嗙爜 | 棣栨杩愯鍚庣珛鍗充慨鏀?`admin` 鐨?`admin123` 榛樿瀵嗙爜 |
| TOTP | 绠＄悊鍛樿处鍙峰缓璁己鍒跺紑鍚紝闃叉瀵嗙爜娉勯湶鍚庤鍏ヤ镜 |
| LDAP bind_pass | 寤鸿閫氳繃 Vault 娉ㄥ叆锛屼笉瑕佹槑鏂囧啓鍏?`config.json` |
| MinIO secret_key | 鍚屼笂锛孷ault 娉ㄥ叆 |
| OAuth2 client_secret | 鍚屼笂锛孷ault 娉ㄥ叆锛涚敓浜х幆澧冧笉寰楁槑鏂囧瓨鍏?`config.json` |
| OAuth2 redirect_uri | 蹇呴』涓?provider 鎺у埗鍙伴厤缃畬鍏ㄤ竴鑷达紝闃叉 open redirect |
| `/actuator/*` | 寤鸿鍦?Nginx/闃茬伀澧欏眰闄愬埗鍙厑璁稿唴缃戣闂?|
| 鏁版嵁鑴辨晱 | `DataMaskUtils::maskJsonValue()` 鍙湪鏃ュ織/鍝嶅簲涓嚜鍔ㄨ劚鏁忔晱鎰熷瓧娈?|

---

## 鏇存柊鏃ュ織

### v1.3.3锛堝紑鍙戜腑锛?

- **鍚姩浠ｇ爜妯″潡鍖栨媶鍒?*锛氬師 3400+ 琛?`src/main.cc` 鎷嗗垎涓?`src/main/main.cc` 鍏ュ彛 + `src/main/main/` 涓?8 涓樁娈垫ā鍧楋紙`EarlyInit`/`ConfigInit`/`HttpSetup`/`RoutesSetup`/`CertRoutes`/`StartupAdvice`/`RuntimeSetup`/`DbConnStr`锛夛紝鍏变韩 `AppBootstrap.h` 涓殑 `AppContext` 涓婁笅鏂囷紱鏃?`src/main.cc` 淇濈暀涓哄鐓э紝涓嶅弬涓庣紪璇?
- **Linux 骞冲彴瀹屾暣楠岃瘉**锛欸CC 缂栬瘧 + 杩愯楠岃瘉閫氳繃锛圫QLite 闄嶇骇銆亀atchdog 瀹堟姢绉讳氦銆佸崟瀹炰緥閿併€佸杩涚▼缂栨帓鍣ㄥ潎姝ｅ父锛?
- **SQL LIKE 閫氶厤绗﹁浆涔?*锛歚StringUtils::escapeLikeParam` 鎻愬崌涓哄叕鍏卞疄鐜帮紝鍏ㄩ儴 LIKE 鏌ヨ缁熶竴 `ESCAPE` 杞箟锛岄槻姝?`%`/`_` 閫氶厤绗︽敞鍏?
- **鍚庡彴绾跨▼浼橀泤閫€鍑?*锛歚HotConfig`銆乣LicenseWatcher` 绾跨▼鏀?joinable锛宍stop()` 鐪熸绛夊緟閫€鍑猴紝鏋愭瀯鍏滃簳闃?`std::terminate`锛沗LicenseWatcher` 杞鐫＄湢鏀?1s 绮掑害锛屽仠鏈嶇绾ц繑鍥?
- **棣栧惎鑷姩鐢熸垚榛樿閰嶇疆**锛歚config.json` 缂哄け鏃舵寜 `DefaultConfig.h` 鍐呭祵妯℃澘鐢熸垚锛圫QLite 妯″紡锛夛紝鍐嶈繘鍏ヨ鍙瘉鏍￠獙
- **WAF 闃茬伀澧?*锛坄src/waf/`锛夛細`WafEngine` 姝ｅ垯瑙勫垯寮曟搸锛堝唴缃?SQLi / XSS / 璺緞绌胯秺 / 鍛戒护娉ㄥ叆瑙勫垯锛屾敮鎸?config.json 杩藉姞鑷畾涔夛級锛宍CidrMatcher` IP 榛戠櫧鍚嶅崟锛宍NftBan` Linux nftables 鍐呮牳灞傚皝绂侊紙SYN 闃舵 DROP锛屾棤鏉冮檺鑷姩闄嶇骇涓哄簲鐢ㄥ眰灏佺锛夛紝`WafCtrl` 鎻愪緵 `/monitor/waf/**` 绠＄悊鎺ュ彛锛堢粺璁?/ 鎷︽埅鏃ュ織 / 灏佺 / 瑙勫垯 / CIDR / URI / UA 鍚嶅崟锛?

### v1.3.2

- **鏂囨。鍏ㄩ潰瀹屽杽** - 娣诲姞蹇€熶綋楠屻€丄PI 蹇€熷弬鑰冦€侀儴缃叉渶浣冲疄璺点€佹€ц兘浼樺寲銆佹晠闅滄帓鏌ャ€佸紑鍙戣€呮寚鍗楃瓑瀹屾暣鏂囨。
- **鎶€鏈爤鐗堟湰鏇存柊** - C++ 鏍囧噯鍗囩骇鍒?C++20锛屾洿鏂版墍鏈変緷璧栧簱鐗堟湰淇℃伅
- **绯荤粺瑕佹眰鏄庣‘** - 璇︾粏璇存槑杩愯鐜銆佺紪璇戠幆澧冦€佸彲閫変緷璧栫殑鐗堟湰瑕佹眰
- **API 鏂囨。瀹屾暣** - 鏂板宸ュ叿銆丄I銆両oT 妯″潡鐨?API 鏂囨。锛屽叡 50+ 涓鐐?
- **閮ㄧ讲鎸囧崡璇︾粏** - 娣诲姞 Docker銆並ubernetes 閮ㄧ讲绀轰緥锛岀敓浜х幆澧冮儴缃叉竻鍗?
- **鎬ц兘鍩哄噯娴嬭瘯** - 鎻愪緵 5 涓満鏅殑鎬ц兘鏁版嵁锛圦PS銆佸欢杩熴€佸唴瀛樺崰鐢級
- **鐗堟湰鍗囩骇鎸囧崡** - 瀹屾暣鐨勫崌绾ф楠ゅ拰鍥炴粴鏂规
- **瀹夊叏鏈€浣冲疄璺?* - 瀵嗙爜瀹夊叏銆佽姹傜鍚嶃€佹暟鎹劚鏁忋€乆SS 闃叉姢绛夊畨鍏ㄦ寚鍗?
- **寮€鍙戣€呮寚鍗?* - 娣诲姞鏂?API銆佹暟鎹簱琛ㄣ€佸畾鏃朵换鍔＄殑瀹屾暣绀轰緥
- **鏁版嵁搴撴灦鏋?* - 璇︾粏鐨勮〃缁撴瀯璁捐鍜岃繛鎺ョ鐞嗕唬鐮佺ず渚?

### v1.3.0

- **浠ｇ爜鐢熸垚妯″潡鍔ㄦ€佸簱鍖?*锛歚src/codegen/` 缂栬瘧涓虹嫭绔?DLL/SO锛坄plugins/codegen_plugin.dll`锛夛紝涓荤▼搴忔棤闇€閲嶆柊缂栬瘧鍗冲彲鏇存柊浠ｇ爜鐢熸垚鍔熻兘锛涙敮鎸佽繍琛屾椂鍔ㄦ€佸姞杞?鍗歌浇锛沗CodeGenCtrl` 鏀逛负绾?C++ 闈欐€佹柟娉曪紝鎺ユ敹/杩斿洖 JSON 瀛楃涓?
- **鍔ㄦ€佺紪璇戝櫒闆嗘垚**锛歚DynamicCompiler` 鏀寔 Windows MinGW + Linux GCC锛岃嚜鍔ㄨ皟鐢?CMake 缂栬瘧鐢熸垚鐨勪唬鐮侊紝鏀寔鑷畾涔夌紪璇戝櫒璺緞锛堢幆澧冨彉閲?`CODEGEN_COMPILER_PATH`锛?
- **鎻掍欢绠＄悊绯荤粺**锛歚PluginManager` 鏀寔鍔犺浇/鍗歌浇/鍒楄〃/璋冪敤澶氫釜鎻掍欢锛屾瘡涓彃浠剁嫭绔?DLL锛屾敮鎸佺儹鏇存柊
- **鍩熷悕 / HTTPS 璁块棶鏀寔**锛歚config.json` 鏂板 `_listeners_https_example` 绀轰緥閰嶇疆锛岃鏄庢湰鍦?鍏綉 HTTP/HTTPS 涓夌鐩戝惉妯″紡锛涜吘璁簯绛変簯鏈嶅姟鍟嗘墜鍔ㄨ瘉涔︼紙`.crt`+`.key`锛夌洿鎺ユ寕杞藉埌 listeners锛岄浂棰濆渚濊禆
- **InnerLink 鑿滃崟 URL 鑷姩鏇挎崲**锛坄menu.api_base_url`锛夛細閮ㄧ讲鍒板叕缃戝悗绋嬪簭鍚姩鏃惰嚜鍔ㄥ皢鏁版嵁搴撲腑鎵€鏈?InnerLink 鑿滃崟鐨?`localhost` 鍦板潃鎵归噺鏇挎崲涓洪厤缃殑鍏綉鍩熷悕锛屾棤闇€鎵嬪姩閫愪竴淇敼鑿滃崟
- **ACME 鑷姩璇佷功璇存槑**锛氭柊澧?`acme` 閰嶇疆鍧楀畬鏁存敞閲婏紝鏄庣‘ 80 绔彛蹇呴』 `https=false`锛圚TTP-01 楠岃瘉锛夛紝443/鑷畾涔夌鍙ｅ紑 HTTPS锛岄槻璇厤宕╂簝
- **閮ㄧ讲璇存槑鏂囨。**锛坄build-nginx/閮ㄧ讲璇存槑.md`锛夛細瀹屾暣瑕嗙洊鏈湴/鍏綉 HTTP/HTTPS 涓夌妯″紡銆丼SL 璇佷功鏍煎紡閫夋嫨銆佸墠绔墦鍖呴儴缃层€佸父鐢ㄧ鍙ｈ鏄?
- **淇纾佺洏鍗锋爣涔辩爜 + 鍫嗗穿婧?*锛坄ServerCtrl.h`锛夛細`GetVolumeInformationW` + `WideCharToMultiByte` 瀹藉瓧鑺傛纭浆鎹紝鏇挎崲鍘?`GetVolumeInformationA` 绐勫瓧鑺傝皟鐢紝娑堥櫎闈?ASCII 鍗锋爣涓嬬殑鍫嗘崯鍧?
- **WebSocket 鏂嚎鑷姩閲嶈繛**锛坄App.vue`锛夛細鎸囨暟閫€閬块噸杩炵瓥鐣ワ紙鏈€澶?30s锛夛紝杩炴帴鏂紑鍚庤嚜鍔ㄦ仮澶嶈闃?
- **Swagger 鎺ュ彛鏂囨。琛ュ叏**锛氭柊澧?IoT 璁惧銆丄I/ONNX 鎺ㄧ悊銆佸浗瀵嗭紙SM2/SM3/SM4锛夈€丱Auth2銆佷唬鐮佺敓鎴愩€佷华琛ㄧ洏绛夋ā鍧楃殑 OpenAPI 鏍囩鍜岃矾寰勬弿杩?
- **IoT 璁惧鍚姩鍔犺浇**锛坄main.cc`锛夛細鍚姩鏃惰皟鐢?`IotCtrl::loadFromDb()` 浠庢暟鎹簱鎭㈠璁惧鍒楄〃锛屾敮鎸佹寔涔呭寲
- **鏂板鍗曞厓娴嬭瘯**锛歚test_token_cache`锛坰et/get/remove/update/size锛夈€乣test_rate_limiter`锛堟甯歌姹?瓒呴檺灏佺/鐧藉悕鍗?绂佺敤锛夛紝闆嗘垚鍒?`RUOYI_BUILD_HEAVY_TESTS`
- **DashboardCtrl 淇**锛歚char today[16]` 鈫?`char today[32]`锛屾秷闄?Linux glibc fortify 缂撳啿鍖鸿鍛?

### v1.2.1
- **閲嶅惎鏈嶅姟绠＄悊椤?*锛歚GET /monitor/restart` 绾悗绔覆鏌?HTML 椤甸潰锛岀鐞嗗憳鍙煡璇㈠湪绾夸汉鏁板悗浜屾纭閲嶅惎鍚庣杩涚▼锛泃oken 浠庡悓婧?iframe 鐨?`sessionStorage` 鑷姩璇诲彇锛屾棤闇€ Vue 缁勪欢
- **淇 HTTP_HIDE 鐢熶骇 404 Bug**锛歚SysRestartCtrl` 鍘?`HTTP_HIDE` 瀹忓湪 Release 鏋勫缓涓嬪皢閲嶅惎鎺ュ彛鍏ㄩ儴杩斿洖 404锛屽凡绉婚櫎
- **娑堟伅閫氱煡涓績**锛坒15锛夛細閽夐拤 / 椋炰功 / 浼佷笟寰俊 Webhook锛圚MAC-SHA256 绛惧悕锛? 绔欏唴娑堟伅锛宍/system/notify/channel/**` + `/system/message/**`
- **API Key 绠＄悊**锛坒16锛夛細`/system/apikey/**` CRUD锛?8 浣嶉殢鏈?Key锛宍X-API-Key` 璇锋眰澶存垨 `?apiKey=` 鏌ヨ鍙傛暟閴存潈
- **鎿嶄綔瀹¤澧炲己**锛坒17锛夛細`sys_oper_log` 鏂板 `before_data`/`after_data` 瀛楁锛宍diffJson()` 鍙褰曞彉鏇村瓧娈碉紝`LOG_AUDIT` / `LOG_AUDIT_TIMED` 瀹?
- **SQLite 鍙屽眰鍔犲瘑**锛?
  - **椤电骇鍔犲瘑**锛圼sqlite3mc](https://github.com/utelle/SQLite3MultipleCiphers) 闆嗘垚锛夛細纾佺洏鏂囦欢姣忛〉 AES 鍔犲瘑锛屾棤鏄庢枃绐楀彛锛涢€氳繃 `scripts/download_sqlite3mc.ps1` 鎷夊彇 12 MB amalgamation 鍚庡惎鐢紝CMake 鑷姩妫€娴?
  - **鏂囦欢绾у姞瀵?*锛圧YENC1 鑷爺灏佽锛変綔涓哄厹搴曪細AES-256-GCM + HMAC-SHA256 + Magic+Version+KDF_iter 澶达紝浠呬緷璧?OpenSSL锛涘惎鍔ㄨВ瀵?`.enc 鈫?.db`銆佸叧闂姞瀵嗗洖鍐欏苟鍒犳槑鏂?
  - 鍏辩敤 `sqlite.encrypt_key` 鏋佺畝閰嶇疆鎴?`security.sqlite.encryption.*`锛? 绉嶅瘑閽ユ潵婧愶細config/env/hwid/vault/hwid+vault锛?
  - 璇﹁ [`docs/SQLITE_ENCRYPTION.md`](docs/SQLITE_ENCRYPTION.md)
- **SQLite 鍔犲瘑 CLI 宸ュ叿** `sqlite_cipher_tool`锛歟ncrypt / decrypt / rekey / check / selftest 瀛愬懡浠わ紱鐢?`VACUUM INTO + sqlite3_rekey` 涓ゆ寮忚法 codec 鎷疯礉锛堣閬?sqlite3mc 榛樿 cipher 涓?backup API 鐨勪笉鍏煎锛?
- **椤舵爮鏈閫氱煡寰芥爣 API**锛氭柊澧?`sys_notice_read(user_id, notice_id, read_at)` 琛?+ `GET /system/notice/unreadCount` 杩斿洖 `{count}` + `listTop` 澧炲姞 `isRead` 瀛楁
- **浼橀泤鍋滄満绔偣** `POST /actuator/shutdown`锛堜粎 loopback 鍙Е鍙戯級锛?00 鍝嶅簲鍚庡紓姝?`drogon::app().quit()`锛岄伩鍏?Windows console 淇″彿闅鹃锛岀敤浜庤嚜鍔ㄥ寲娴嬭瘯 / 杩愮淮鑴氭湰
- **鍙娴嬫€?+ 鍗曞厓娴嬭瘯涓?CI**锛歚tests/test_sqlite_file_cipher.cc` 10 鐢ㄤ緥 / 57 鏂█锛堝惈 HMAC 绡℃敼/闄嶇骇妫€娴嬶級锛汫itHub Actions 涓夊钩鍙拌窇娴嬭瘯 + 鏂?`sqlite3mc-fetch` job 楠岃瘉涓嬭浇鑴氭湰鍙敤鎬э紙鍖呭惈 SHA256 鏍￠獙 + 鐙珛 gcc 缂栬瘧锛?

### v1.2.0
- **OAuth2 绗笁鏂圭櫥褰?*锛欸itHub / Google / 浼佷笟寰俊 / 閽夐拤 / 椋炰功 / QQ锛宻tate CSRF 闃叉姢锛岄娆¤嚜鍔ㄥ缓鍙凤紝宸叉湁璐﹀彿鍙粦瀹?瑙ｇ粦锛坄sys_user_oauth` 琛級
- **TOTP 涓ゆ楠岃瘉**锛欸oogle/Microsoft Authenticator锛孯FC 6238 绾?OpenSSL 瀹炵幇
- **LDAP/AD 璁よ瘉**锛氫紒涓氬唴缃戠粺涓€鐧诲綍锛屾敮鎸?`fallback_local`
- **鏂囦欢瀛樺偍鍒嗗眰**锛氭湰鍦扮鐩橀粯璁わ紝`config.json` 鍒囨崲 MinIO/S3锛圓WS SigV4 绛惧悕锛?
- **閰嶇疆鐑噸杞?*锛歚HotConfig` 5s 杞鏂囦欢鍙樺寲鑷姩鐢熸晥锛屾垨璋?`POST /actuator/reload`
- **Prometheus 鎸囨爣**锛歚/actuator/metrics`锛圦PS/寤惰繜/DB 鐘舵€侊級锛屽吋瀹?Grafana
- **鏁版嵁鑴辨晱宸ュ叿**锛氭墜鏈恒€佽韩浠借瘉銆侀摱琛屽崱銆侀偖绠便€佸鍚嶈嚜鍔ㄨ劚鏁?
- **X-Request-ID 閾捐矾杩借釜**锛氬叏璇锋眰鑷姩鐢熸垚/浼犻€?
- **绯荤粺鏃ュ織鏌ョ湅鍣?*锛氬墠绔?iframe 鍐呭祵锛屾敮鎸佹煡鐪?`.log`/`.jsonl`锛屽疄鏃跺埛鏂?
- **DB 杩炴帴姹犵洃鎺?*锛歚/actuator/db` 灞曠ず PG/SQLite 鐘舵€併€佸緟鍚屾闃熷垪
- **鐑崌绾?TOTP 瀛楁**锛歚ALTER TABLE IF NOT EXISTS` 鏃犳崯杩佺Щ

### v1.1.0
- **Drogon 鍐呯疆鍓嶇鎵樼**锛歚/prod-api` 鍓嶇紑鑷姩鍓ョ锛孲PA history 妯″紡鍥為€€锛屾棤闇€ Nginx
- **绯荤粺鏃ュ織鏌ョ湅鍣ㄥ悗绔〉闈?*锛歚GET /monitor/logfile/page`锛岄€氳繃 iframe 宓屽叆鍓嶇
- **JSON 缁撴瀯鍖栨棩蹇?*锛歚JsonLogger` 灏?trantor 鏂囨湰鏃ュ織杞负 NDJSON锛坄.jsonl`锛?
- **SQLite 榛樿鍏煎**锛歚DEFAULT NOW()` 鈫?`CURRENT_TIMESTAMP`

### v1.0.0
- 瀹屾暣瀹炵幇 RuoYi-Vue 鎵€鏈夌郴缁熺鐞嗐€佺郴缁熺洃鎺?API
- PostgreSQL + SQLite 鑷姩闄嶇骇鍙屽啓锛孭G 鎭㈠鍚庤嚜鍔ㄥ悓姝ュ洖鍐?
- PBKDF2-SHA256 瀵嗙爜鍝堝笇銆丣WT 鑷姩缁湡銆乀oken 榛戝悕鍗?
- HashiCorp Vault 瀵嗛挜绠＄悊锛堣嚜鍔ㄥ惎鍔?瑙ｅ皝/娉ㄥ叆锛?
- 閭欢鍙戜欢绠辩鐞嗭紙OpenSSL Implicit-TLS SMTP锛屽鍙戜欢浜鸿疆杞級
- 蹇樿瀵嗙爜 / 娉ㄥ唽閭楠岃瘉鐮?
- WebSocket 瀹炴椂閫氱煡锛堜竴娆℃€?ticket 閴存潈锛?
- IP 闄愭祦銆丅ot UA 鎷︽埅銆乆SS/SQL 娉ㄥ叆杩囨护銆丆ORS 閰嶇疆
- 璇锋眰绛惧悕楠岃瘉锛坄SignUtils`锛?
- 璁惧缁戝畾锛堢‖浠舵寚绾?+ Vault 瀵嗛挜锛?
- 璁稿彲璇佺鐞嗭紙鏂囦欢绛惧悕 + 鍔熻兘寮€鍏筹級
- GPU VRAM 缂撳瓨锛堝彲閫?CUDA锛?
- Cron 瀹氭椂浠诲姟璋冨害寮曟搸锛堢绾э紝DB 鎸佷箙鍖栵級
- 闆嗙兢妯″紡锛堜富/浠庤鑹诧紝鑷姩鐢熸垚 Nginx upstream.conf锛?
- 宕╂簝鎹曡幏锛圫EH/VEH + Minidump锛學indows锛?
- 瑙掕壊鏉冮檺淇敼瀹炴椂鐢熸晥锛屾棤闇€閲嶆柊鐧诲綍

---

## 浜ゆ祦缇?

QQ 浜ゆ祦缇わ細**782798239**

骞垮窞甯傚叓鑲℃枃绉戞妧瀹樼綉锛?http://www.gzbgw.com>

鍏偂鏂囬搴撳钩鍙帮細<http://question.gzbgw.com>

---

## 寮€婧愬崗璁?

鏈」鐩熀浜?[MIT License](LICENSE) 寮€婧愩€?

RuoYi-Vue 鍘熼」鐩増鏉冨綊 [鑻ヤ緷鍥㈤槦](https://gitee.com/y_project/RuoYi-Vue) 鎵€鏈夛紝閬靛惊 MIT 鍗忚銆?
