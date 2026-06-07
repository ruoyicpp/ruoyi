cmake_minimum_required(VERSION 3.15)
project(ruoyi-cpp C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── 编译选项 ──────────────────────────────────────────────────
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FS /utf-8")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -finput-charset=UTF-8 -fexec-charset=UTF-8")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

# ── 路径定义 ──────────────────────────────────────────────────
# Drogon 已用 CMake 安装到 need/install-static/ 前缀，直接复用其 include/lib
set(DROGON_INSTALL_PREFIX "g:/back/recovered/drogon-master/drogon-master/need/install-static"
    CACHE PATH "Path where drogon was installed (prefix containing include/ and lib/)")

set(DROGON_INCLUDE_DIRS
    "${DROGON_INSTALL_PREFIX}/include"
)

if(WIN32)
    # ── Windows：使用 Drogon install 前缀 + msys64 UCRT64 库 ──
    # 必须使用 UCRT64 工具链，因为预编译的 libdrogon/libtrantor.dll 链接的是
    # UCRT 版 libstdc++-6.dll；mingw64(msvcrt) 版 libstdc++ ABI 不兼容会导致
    # LoadLibrary 报 ENTRYPOINT_NOT_FOUND (0xC0000139)。
    set(MSYS_PREFIX "H:/msys64/msys64/ucrt64" CACHE PATH "MSYS2 ucrt64 root")
    set(VCPKG_INCLUDE_DIR "${MSYS_PREFIX}/include")
    set(VCPKG_LIB_DIR     "${MSYS_PREFIX}/lib")
    
    # ── 唐僧叨叨 IM 系统 DLL 路径 ──
    set(TANGSENGDAODAO_DLL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/im/im/TangSengDaoDaoServer-main/cgo-lib/build/tangsengdaodao.dll"
        CACHE PATH "TangSengDaoDaoDLL path")
    set(TANGSENGDAODAO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/im/im/TangSengDaoDaoServer-main/cgo-lib/build"
        CACHE PATH "TangSengDaoDaoDLL header path")

    # PostgreSQL libpq 头文件（从 ucrt64 复制，避免系统头文件冲突）
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/libpq_include")
    foreach(_hdr libpq-fe.h libpq-events.h postgres_ext.h)
        if(EXISTS "${MSYS_PREFIX}/include/${_hdr}")
            file(COPY "${MSYS_PREFIX}/include/${_hdr}"
                 DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/libpq_include")
        endif()
    endforeach()
    set(POSTGRESQL_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/libpq_include")
    set(POSTGRESQL_LIB_DIR "${MSYS_PREFIX}/lib")

    find_library(DROGON_LIB   NAMES drogon  libdrogon  PATHS "${DROGON_INSTALL_PREFIX}/lib" ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(TRANTOR_LIB  NAMES trantor libtrantor PATHS "${DROGON_INSTALL_PREFIX}/lib" ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(JSONCPP_LIB  NAMES jsoncpp libjsoncpp PATHS "${DROGON_INSTALL_PREFIX}/lib" ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(ZLIB_LIB     NAMES z zlib libz libzlib PATHS "${DROGON_INSTALL_PREFIX}/lib" ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(SSL_LIB      NAMES ssl     libssl     PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(CRYPTO_LIB   NAMES crypto  libcrypto  PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(HIREDIS_LIB  NAMES hiredis libhiredis PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(CURL_LIB     NAMES curl    libcurl    PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    # c-ares：trantor 启用 TLS 重编后引入此依赖（异步 DNS 解析）
    # 优先匹配静态库 .a 避免运行时 DLL 依赖
    unset(CARES_LIB CACHE)
    find_library(CARES_LIB NAMES libcares.a cares libcares PATHS "${MSYS_PREFIX}/lib" ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    message(STATUS "[c-ares] ${CARES_LIB}")
    # brotli：优先匹配静态库 .a（避免运行时依赖 libbrotli*.dll）
    # MinGW 下 find_library 默认 prefer .dll.a (import lib)，必须把 .a 放第一位
    # unset 强制重新查找（避免命中 CMakeCache 旧值）
    unset(BROTLIENC_LIB    CACHE)
    unset(BROTLIDEC_LIB    CACHE)
    unset(BROTLICOMMON_LIB CACHE)
    find_library(BROTLIENC_LIB    NAMES libbrotlienc.a    brotlienc    libbrotlienc    PATHS "${MSYS_PREFIX}/lib" "${DROGON_INSTALL_PREFIX}/lib" ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(BROTLIDEC_LIB    NAMES libbrotlidec.a    brotlidec    libbrotlidec    PATHS "${MSYS_PREFIX}/lib" "${DROGON_INSTALL_PREFIX}/lib" ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(BROTLICOMMON_LIB NAMES libbrotlicommon.a brotlicommon libbrotlicommon PATHS "${MSYS_PREFIX}/lib" "${DROGON_INSTALL_PREFIX}/lib" ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    message(STATUS "[Brotli] enc=${BROTLIENC_LIB}")
    message(STATUS "[Brotli] dec=${BROTLIDEC_LIB}")
    message(STATUS "[Brotli] common=${BROTLICOMMON_LIB}")
    find_library(PQ_LIB       NAMES pq libpq   PATHS ${POSTGRESQL_LIB_DIR} ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(SQLITE3_LIB  NAMES sqlite3 libsqlite3 PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(MARIADB_LIB  NAMES mariadbclient libmariadbclient mariadb libmariadb PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
else()
    # ── Linux / WSL：使用 pkg-config + 系统库 ────────────────
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSONCPP  REQUIRED jsoncpp)
    pkg_check_modules(OPENSSL  REQUIRED openssl)

    find_library(DROGON_LIB  NAMES drogon
        PATHS "${DROGON_ROOT}/build" /usr/local/lib /usr/lib)
    find_library(TRANTOR_LIB NAMES trantor
        PATHS "${DROGON_ROOT}/build/trantor" /usr/local/lib /usr/lib)
    find_library(PQ_LIB      NAMES pq)
    find_library(HIREDIS_LIB NAMES hiredis)
    find_library(SQLITE3_LIB NAMES sqlite3)
    find_library(ZLIB_LIB    NAMES z)
    find_library(UUID_LIB    NAMES uuid)
    find_library(CURL_LIB    NAMES curl)
    find_library(BROTLIENC_LIB    NAMES brotlienc)
    find_library(BROTLIDEC_LIB    NAMES brotlidec)
    find_library(BROTLICOMMON_LIB NAMES brotlicommon)

    set(VCPKG_INCLUDE_DIR "/usr/local/include")
    set(POSTGRESQL_INCLUDE_DIR "")
endif()

if(NOT DROGON_LIB)
    message(FATAL_ERROR "libdrogon not found. Build Drogon first or install it to /usr/local.")
endif()
if(NOT TRANTOR_LIB)
    message(FATAL_ERROR "libtrantor not found.")
endif()

message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Drogon:   ${DROGON_LIB}")
message(STATUS "Trantor:  ${TRANTOR_LIB}")
message(STATUS "libpq:    ${PQ_LIB}")

# ── SQLite 加密（sqlite3mc，MIT，MinGW 兼容）─────────────────
option(ENABLE_SQLITE_ENCRYPTION "使用 sqlite3mc 编译 SQLite 加密支持" ON)
option(RUOYI_DEV_BYPASS_LICENSE "开发模式：跳过许可证校验（仅供二次开发使用，发布时必须关闭）" ON)
if(RUOYI_DEV_BYPASS_LICENSE)
    add_compile_definitions(RUOYI_DEV_BYPASS_LICENSE=1)
    message(WARNING "RUOYI_DEV_BYPASS_LICENSE=ON：许可证校验已绕过，仅限开发环境")
endif()
set(SQLITE3MC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/sqlite3mc")

if(ENABLE_SQLITE_ENCRYPTION AND EXISTS "${SQLITE3MC_DIR}/sqlite3mc_amalgamation.c")
    set(RUOYI_SQLITE3MC_ENABLED ON)
    add_library(sqlite3mc STATIC "${SQLITE3MC_DIR}/sqlite3mc_amalgamation.c")
    target_compile_options(sqlite3mc PRIVATE
        -DSQLITE_HAS_CODEC
        -DSQLITE3MC_USE_MINIZ=0
        -finput-charset=UTF-8
    )
    # sqlite3mc 自身编译时也需要这个宏，避免 shim 头文件 #include_next <sqlite3.h> 找不到
    target_compile_definitions(sqlite3mc PUBLIC HAVE_SQLCIPHER=1)
    target_include_directories(sqlite3mc PUBLIC "${SQLITE3MC_DIR}")
    if(WIN32)
        target_link_libraries(sqlite3mc PRIVATE bcrypt)
    endif()
    message(STATUS "SQLite3MC:     BUILD FROM SOURCE -> ${SQLITE3MC_DIR}")
else()
    set(RUOYI_SQLITE3MC_ENABLED OFF)
    if(ENABLE_SQLITE_ENCRYPTION)
        message(WARNING "sqlite3mc amalgamation not found at ${SQLITE3MC_DIR}, falling back to system sqlite3")
    endif()
endif()

# ── drogon_pg_driver 自研驱动（可选）──────────────────────────
# 默认 OFF，使用官方 libpq；ON 时走 src/third_party/drogon_pg_driver
# （连接池 3~24 + 熔断器 + 独占事务 + 异步写队列）
option(RUOYI_USE_PG_DRIVER "使用自研 drogon_pg_driver 替代官方 libpq" OFF)
if(RUOYI_USE_PG_DRIVER)
    set(_drogon_pg_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/drogon_pg_driver")
    if(NOT EXISTS "${_drogon_pg_dir}/CMakeLists.txt")
        message(FATAL_ERROR "RUOYI_USE_PG_DRIVER=ON 但 ${_drogon_pg_dir} 不存在")
    endif()
    if(WIN32)
        # 透传 PostgreSQL 路径给子项目，避免重复 find_package
        set(PostgreSQL_INCLUDE_DIRS "${POSTGRESQL_INCLUDE_DIR}" CACHE INTERNAL "")
        set(PostgreSQL_LIBRARIES    "${PQ_LIB}"                 CACHE INTERNAL "")
    endif()
    add_subdirectory(${_drogon_pg_dir} ${CMAKE_CURRENT_BINARY_DIR}/drogon_pg_driver EXCLUDE_FROM_ALL)
    message(STATUS "drogon_pg_driver: ENABLED -> ${_drogon_pg_dir}")
endif()

# ── minio-cpp（可选，S3/MinIO 对象存储 SDK）────────────────
# 优先用 vcpkg 安装版 find_package；找不到则回退到项目内源码编译
option(RUOYI_USE_MINIO_CPP "集成 minio-cpp SDK（需要 vcpkg install minio-cpp 或 G:/back/recovered/minio-cpp）" ON)

# ── 进程内 nginx 静态库（可选）──────────────────────────────────
# 默认 OFF，开启后把 nginx-1.29.8 静态库嵌入 ruoyi-cpp.exe 内：
#   - nginx 在独立线程跑（IOCP 事件循环）
#   - 监听 80/443 + reverse_proxy 到 drogon 后端 (127.0.0.1:18080)
#   - HTTPS/HTTP2/gzip/缓存/反代 全套 nginx 能力
# 注意：启用后会用 nginx 自带的 OpenSSL 静态库替代 msys2 的 OpenSSL DLL，
#       drogon 也走相同的 OpenSSL（避免双份冲突）
option(RUOYI_USE_NGINX "嵌入 nginx 静态库做反向代理 / HTTPS 终止" ON)
set(NGINX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/nginx-1.29.8"
    CACHE PATH "nginx 源码根目录（已编译产生 objs/libnginx.a）")
if(RUOYI_USE_NGINX)
    set(NGINX_LIB    "${NGINX_ROOT}/objs/libnginx.a")
    set(NGINX_SSL    "${NGINX_ROOT}/deps/openssl-3.3.2/libssl.a")
    set(NGINX_CRYPTO "${NGINX_ROOT}/deps/openssl-3.3.2/libcrypto.a")
    set(NGINX_PCRE2  "${NGINX_ROOT}/deps/pcre2-10.45/.libs/libpcre2-8.a")
    set(NGINX_ZLIB   "${NGINX_ROOT}/deps/zlib-1.3.1/libz.a")
    set(NGINX_INC    "${NGINX_ROOT}/src/sgs")
    foreach(_lib NGINX_LIB NGINX_SSL NGINX_CRYPTO NGINX_PCRE2 NGINX_ZLIB)
        if(NOT EXISTS "${${_lib}}")
            message(FATAL_ERROR "RUOYI_USE_NGINX=ON 但缺少 ${${_lib}} (请先编译 nginx)")
        endif()
    endforeach()
    # 用 nginx 自带 OpenSSL/zlib 替换 msys2 版本，避免双份冲突
    set(SSL_LIB    "${NGINX_SSL}"    CACHE FILEPATH "" FORCE)
    set(CRYPTO_LIB "${NGINX_CRYPTO}" CACHE FILEPATH "" FORCE)
    set(ZLIB_LIB   "${NGINX_ZLIB}"   CACHE FILEPATH "" FORCE)
    add_compile_definitions(RUOYI_USE_NGINX=1)
    message(STATUS "nginx:    ENABLED -> ${NGINX_LIB}")
    message(STATUS "  SSL:    ${NGINX_SSL}")
    message(STATUS "  CRYPTO: ${NGINX_CRYPTO}")
    message(STATUS "  PCRE2:  ${NGINX_PCRE2}")
    message(STATUS "  ZLIB:   ${NGINX_ZLIB}")
endif()

# ── 源文件 ────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    src/main.cc
    src/common/*.cc
    src/services/*.cc
    src/system/services/*.cc
    src/system/controllers/*.cc
    src/ai/controllers/*.cc
    src/filters/*.cc
    src/libs/plugin/*.cc
    src/taskqueue/*.cc
    src/monitor/*.cc
    src/cache/*.cc
)

# 添加 IM 集成文件（排除 src/im/111 中的 protobuf 文件）
file(GLOB IM_SOURCES src/im/*.cc)
list(APPEND SOURCES ${IM_SOURCES})
message(STATUS "[IM] IM sources: ${IM_SOURCES}")

add_executable(${PROJECT_NAME} ${SOURCES})

# 支持大对象文件（main.cc 包含太多头文件）
if(MINGW)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wa,-mbig-obj)
endif()

# ── minio-cpp（接 RUOYI_USE_MINIO_CPP=ON 时启用）────────────
if(RUOYI_USE_MINIO_CPP)
    if(WIN32)
        # ── Windows：直接 find_library（参考 wepay-cpp vcpkg x64-mingw-static 方案）──
        set(VCPKG_INSTALLED "G:/back/recovered/drogon-master/vcpkg-master/installed/x64-mingw-static")
        find_library(MINIOCPP_LIB NAMES miniocpp libminiocpp
            PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
        find_library(CURLPP_LIB NAMES curlpp libcurlpp
            PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
        find_library(INIH_LIB NAMES inih libinih
            PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
        find_library(PUGIXML_LIB NAMES pugixml libpugixml
            PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
        find_library(INIREADER_LIB NAMES INIReader libINIReader
            PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
        if(MINIOCPP_LIB)
            target_include_directories(${PROJECT_NAME} PRIVATE
                "${VCPKG_INSTALLED}/include"
                "${VCPKG_INSTALLED}/include/miniocpp"
            )
            target_link_libraries(${PROJECT_NAME} PRIVATE ${MINIOCPP_LIB})
            if(PUGIXML_LIB)
                target_link_libraries(${PROJECT_NAME} PRIVATE ${PUGIXML_LIB})
            endif()
            if(INIH_LIB)
                target_link_libraries(${PROJECT_NAME} PRIVATE ${INIH_LIB})
            endif()
            if(CURLPP_LIB)
                target_link_libraries(${PROJECT_NAME} PRIVATE ${CURLPP_LIB})
            endif()
            if(INIREADER_LIB)
                target_link_libraries(${PROJECT_NAME} PRIVATE ${INIREADER_LIB} ${INIH_LIB})
            endif()
            target_compile_definitions(${PROJECT_NAME} PRIVATE RUOYI_USE_MINIO_CPP=1)
            message(STATUS "minio-cpp: ENABLED (Windows) -> ${MINIOCPP_LIB}")
        else()
            message(WARNING "minio-cpp: libminiocpp.a NOT FOUND at ${VCPKG_INSTALLED}/lib")
        endif()
    else()
        # ── Linux / WSL：优先用 vcpkg cmake config（参考 wepay-cpp）─────────────
        find_package(miniocpp CONFIG QUIET)
        if(miniocpp_FOUND)
            target_link_libraries(${PROJECT_NAME} PRIVATE miniocpp::miniocpp)
            find_library(INIREADER_LIB NAMES INIReader)
            if(INIREADER_LIB)
                target_link_libraries(${PROJECT_NAME} PRIVATE ${INIREADER_LIB})
            endif()
            target_compile_definitions(${PROJECT_NAME} PRIVATE RUOYI_USE_MINIO_CPP=1)
            message(STATUS "minio-cpp: ENABLED (Linux) via find_package")
        else()
            # 回退：在常见系统路径搜索静态库
            find_library(MINIOCPP_LIB NAMES miniocpp libminiocpp
                PATHS /usr/local/lib /usr/lib /opt/vcpkg/installed/x64-linux/lib
                     ${CMAKE_PREFIX_PATH}/lib)
            if(MINIOCPP_LIB)
                target_link_libraries(${PROJECT_NAME} PRIVATE ${MINIOCPP_LIB})
                find_library(INIH_LIB NAMES inih)
                if(INIH_LIB)
                    target_link_libraries(${PROJECT_NAME} PRIVATE ${INIH_LIB})
                endif()
                target_include_directories(${PROJECT_NAME} PRIVATE
                    /usr/local/include /usr/include /opt/vcpkg/installed/x64-linux/include)
                target_compile_definitions(${PROJECT_NAME} PRIVATE RUOYI_USE_MINIO_CPP=1)
                message(STATUS "minio-cpp: ENABLED (Linux fallback) -> ${MINIOCPP_LIB}")
            else()
                message(WARNING "minio-cpp: NOT FOUND on Linux, falling back to SigV4 for type=minio")
            endif()
        endif()
    endif()
endif()

if(RUOYI_USE_PG_DRIVER)
    target_link_libraries(${PROJECT_NAME} PRIVATE drogon_pg_driver)
    target_compile_definitions(${PROJECT_NAME} PRIVATE RUOYI_USE_PG_DRIVER=1)
endif()

# ── 前端嵌入模式（可选，参考 wepay-cpp 设计）──────────────────
# 默认 OFF，使用 config.json 的 frontend.enabled 外部目录托管
# ON 时把前端 dist 编译进 exe，运行时从内存提供服务（单文件分发）
# 用法：cmake -DRUOYI_EMBED_FRONTEND=ON -DRUOYI_EMBED_FRONTEND_DIR=./web ..
option(RUOYI_EMBED_FRONTEND "把前端 dist 编译进 exe（单文件分发）" OFF)
set(RUOYI_EMBED_FRONTEND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/web"
    CACHE PATH "需嵌入的前端 dist 目录")
if(RUOYI_EMBED_FRONTEND)
    if(NOT EXISTS "${RUOYI_EMBED_FRONTEND_DIR}")
        message(FATAL_ERROR "RUOYI_EMBED_FRONTEND=ON 但 ${RUOYI_EMBED_FRONTEND_DIR} 不存在")
    endif()
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedResources.cmake)
    embed_web_resources("${RUOYI_EMBED_FRONTEND_DIR}"
                        "${CMAKE_BINARY_DIR}/embed_generated")
    target_sources(${PROJECT_NAME} PRIVATE ${EMBED_GENERATED_FILES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE RUOYI_USE_EMBEDDED_FRONTEND=1)
    message(STATUS "[Frontend] 嵌入模式: ON  src=${RUOYI_EMBED_FRONTEND_DIR}"
                   "  files=${EMBED_FILE_COUNT}  bytes=${EMBED_TOTAL_BYTES}")
else()
    message(STATUS "[Frontend] 嵌入模式: OFF（外部 ./web 目录或 nginx 反代）")
endif()

# ── 包含目录 ──────────────────────────────────────────────────
if(WIN32)
    # jwt-cpp 头文件（header-only，从 void-engine 工程复用）
    set(JWT_CPP_INCLUDE "g:/back/recovered/drogon-master/void-engine/third_party/jwt-cpp/include"
        CACHE PATH "jwt-cpp header-only include directory")
    target_include_directories(${PROJECT_NAME} PRIVATE
        $<$<BOOL:${RUOYI_SQLITE3MC_ENABLED}>:${SQLITE3MC_DIR}>
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${DROGON_INCLUDE_DIRS}
        ${JWT_CPP_INCLUDE}
        ${VCPKG_INCLUDE_DIR}
        ${POSTGRESQL_INCLUDE_DIR}
        ${TANGSENGDAODAO_INCLUDE_DIR}
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX DROGON_STATIC_DEFINE TRANTOR_STATIC_DEFINE JSONCPP_STATIC_DEFINE)
    if(RUOYI_SQLITE3MC_ENABLED)
        target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_SQLCIPHER=1)
    endif()
else()
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${SQLITE3MC_DIR}                          # 同上，优先级最高
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${DROGON_INCLUDE_DIRS}
        ${JSONCPP_INCLUDE_DIRS}
        /usr/local/include
        /usr/include
        /usr/include/postgresql
    )
    if(RUOYI_SQLITE3MC_ENABLED)
        target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_SQLCIPHER=1)
    endif()
endif()

# ── 链接库 ────────────────────────────────────────────────────
if(WIN32)
    # 检查唐僧叨叨 DLL 是否存在
    if(EXISTS "${TANGSENGDAODAO_DLL_PATH}")
        message(STATUS "TangSengDaoDao DLL found: ${TANGSENGDAODAO_DLL_PATH}")
    else()
        message(WARNING "TangSengDaoDao DLL not found at ${TANGSENGDAODAO_DLL_PATH}")
    endif()
    
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${DROGON_LIB}
        ${TRANTOR_LIB}
        ${CARES_LIB}
        ${PQ_LIB}
        ${HIREDIS_LIB}
        ${MARIADB_LIB}
        $<IF:$<BOOL:${RUOYI_SQLITE3MC_ENABLED}>,sqlite3mc,${SQLITE3_LIB}>
        ${JSONCPP_LIB}
        ${SSL_LIB}
        ${CRYPTO_LIB}
        ${CURL_LIB}
        ${BROTLIENC_LIB}
        ${BROTLIDEC_LIB}
        ${BROTLICOMMON_LIB}
        ${ZLIB_LIB}
        ws2_32 wsock32 crypt32 secur32 bcrypt winhttp
        iphlpapi userenv rpcrt4 shlwapi winpthread psapi dbghelp
    )
    # nginx 静态库（启用时追加；放在 SSL_LIB 之后让 nginx 自带 OpenSSL 优先）
    if(RUOYI_USE_NGINX)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            ${NGINX_LIB}
            ${NGINX_PCRE2}
            # nginx 引用的 Windows API：socket / IOCP / EventLog / 加密
            # 必须出现在 nginx 库之后（GNU ld 单遍解析）
            ws2_32 wsock32 mswsock crypt32 secur32
            ole32 advapi32 user32 gdi32 iphlpapi userenv psapi
        )
        target_include_directories(${PROJECT_NAME} PRIVATE ${NGINX_INC})
        # main.cc 加 nginx 头后模板展开节数过多，PE/COFF 默认上限 32K
        # -Wa,-mbig-obj 启用 bigobj 格式（节数上限 4G+）
        # -Wl,--allow-multiple-definition 兜底防 OpenSSL 双份符号冲突
        target_compile_options(${PROJECT_NAME} PRIVATE -Wa,-mbig-obj)
        target_link_options(${PROJECT_NAME} PRIVATE -Wl,--allow-multiple-definition)
    endif()

    # ── 自动复制运行时 DLL 到 build 目录（让双击直接能跑）──
    # Drogon/Trantor 使用静态库，不再复制 libdrogon.dll/libtrantor.dll。
    # 1) Drogon install 前缀中的第三方 DLL（如存在：jsoncpp/brotli/z）
    set(_drogon_bin "${DROGON_INSTALL_PREFIX}/bin")
    # brotli 已通过 .a 静态链接进 exe，不再复制 DLL；libz.dll 保留兼容旧 nginx OFF 路径
    set(_drogon_dlls
        libjsoncpp.dll libz.dll
    )
    # 2) C++ 运行时（winlibs：与 Drogon ABI 匹配）
    get_filename_component(_cxx_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_cxx_dlls libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
    # 3) C 库（来自 ucrt64 / MSYS_PREFIX）
    set(_msys_bin "${MSYS_PREFIX}/bin")
    # jsoncpp 在 ucrt64/bin（drogon install/bin 没有这个 DLL）
    # brotli 已静态链入，不再复制 DLL
    set(_msys_jsoncpp libjsoncpp.dll)
    if(RUOYI_USE_NGINX)
        # 启用 nginx 时 OpenSSL/zlib 已静态链接进 exe，不复制 DLL
        set(_msys_dlls
            libpq.dll libhiredis.dll libsqlite3-0.dll
            libiconv-2.dll libintl-8.dll
            ${_msys_jsoncpp}
        )
        # 同时把 drogon_dlls 中的 libz.dll 移除（nginx 内嵌 zlib 已统一）
        list(REMOVE_ITEM _drogon_dlls libz.dll)
    else()
        set(_msys_dlls
            libpq.dll libhiredis.dll libsqlite3-0.dll
            libssl-3-x64.dll libcrypto-3-x64.dll libz.dll
            libiconv-2.dll libintl-8.dll
            ${_msys_jsoncpp}
        )
    endif()
    # libcurl 和 brotli DLL（即使 .a 静态链入了 exe，某些间接依赖项仍可能需要运行时 DLL）
    set(_msys_curl_dlls  libcurl-4.dll)
    set(_msys_brotli_dlls libbrotlicommon.dll libbrotlidec.dll libbrotlienc.dll)
    foreach(_dll IN LISTS _drogon_dlls)
        if(EXISTS "${_drogon_bin}/${_dll}")
            list(APPEND _runtime_dlls "${_drogon_bin}/${_dll}")
        endif()
    endforeach()
    foreach(_dll IN LISTS _cxx_dlls)
        if(EXISTS "${_cxx_bin}/${_dll}")
            list(APPEND _runtime_dlls "${_cxx_bin}/${_dll}")
        endif()
    endforeach()
    foreach(_dll IN LISTS _msys_dlls)
        if(EXISTS "${_msys_bin}/${_dll}")
            list(APPEND _runtime_dlls "${_msys_bin}/${_dll}")
        endif()
    endforeach()
    foreach(_dll IN LISTS _msys_curl_dlls)
        if(EXISTS "${_msys_bin}/${_dll}")
            list(APPEND _runtime_dlls "${_msys_bin}/${_dll}")
        endif()
    endforeach()
    foreach(_dll IN LISTS _msys_brotli_dlls)
        if(EXISTS "${_msys_bin}/${_dll}")
            list(APPEND _runtime_dlls "${_msys_bin}/${_dll}")
        endif()
    endforeach()

    # libpq 还会拖几个 GSSAPI/Kerberos/SASL 间接依赖，扫整个 ucrt64\bin 把
    # libpq.dll 自身导入的 DLL 都拷过来（仅常见命名前缀）。
    file(GLOB _msys_extra
        "${_msys_bin}/libgssapi*.dll"
        "${_msys_bin}/libkrb5*.dll"
        "${_msys_bin}/libk5crypto*.dll"
        "${_msys_bin}/libcom_err*.dll"
        "${_msys_bin}/liblber*.dll"
        "${_msys_bin}/libldap*.dll"
        "${_msys_bin}/libsasl2*.dll"
        "${_msys_bin}/libidn2*.dll"
        "${_msys_bin}/libunistring*.dll"
        "${_msys_bin}/libffi*.dll"
        "${_msys_bin}/libtasn1*.dll"
        "${_msys_bin}/libnettle*.dll"
        "${_msys_bin}/libhogweed*.dll"
        "${_msys_bin}/libgmp*.dll"
        "${_msys_bin}/libp11-kit*.dll"
        "${_msys_bin}/libzstd*.dll"
        "${_msys_bin}/libgnutls*.dll"
    )
    list(APPEND _runtime_dlls ${_msys_extra})
    
    # 4) certmanager.dll（ACME 证书管理，可选）
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/certmanager.dll")
        list(APPEND _runtime_dlls "${CMAKE_CURRENT_SOURCE_DIR}/certmanager.dll")
    endif()
    # 5) minio-cpp 运行时 DLL（vcpkg 安装 curlpp/inih/nlohmann-json/pugixml 时需要）
    foreach(_minio_dll libcurlpp.dll libinih.dll)
        if(EXISTS "${_msys_bin}/${_minio_dll}")
            list(APPEND _runtime_dlls "${_msys_bin}/${_minio_dll}")
        endif()
    endforeach()
    # 6)  IM 系统 DLL
    if(EXISTS "${TANGSENGDAODAO_DLL_PATH}")
        list(APPEND _runtime_dlls "${TANGSENGDAODAO_DLL_PATH}")
    endif()
    
    list(REMOVE_DUPLICATES _runtime_dlls)

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${_runtime_dlls}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND_EXPAND_LISTS
        VERBATIM
        COMMENT "复制运行时 DLL 到 $<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${DROGON_LIB}
        ${TRANTOR_LIB}
        ${PQ_LIB}
        ${HIREDIS_LIB}
        $<IF:$<BOOL:${RUOYI_SQLITE3MC_ENABLED}>,sqlite3mc,${SQLITE3_LIB}>
        ${JSONCPP_LIBRARIES}
        ssl crypto
        ${CURL_LIB}
        ${ZLIB_LIB}
        ${UUID_LIB}
        cares pthread dl
    )
    foreach(_brotli BROTLIENC_LIB BROTLIDEC_LIB BROTLICOMMON_LIB)
        if(${_brotli})
            target_link_libraries(${PROJECT_NAME} PRIVATE ${${_brotli}})
        endif()
    endforeach()
endif()

# ── 守护进程（watchdog）──────────────────────────────────────────
# 零依赖跨平台守护进程，用于监控和自动重启 ruoyi-cpp.exe
add_executable(watchdog watchdog/watchdog.cc)
target_compile_options(watchdog PRIVATE -std=c++17 -O2)
if(MINGW)
    target_compile_options(watchdog PRIVATE -static-libgcc -static-libstdc++)
endif()

# ── SQLite 加密工具（独立可执行文件）────────────────────────────
# encrypt/decrypt/rekey/check/selftest，仅在启用 sqlite3mc 时编译
if(RUOYI_SQLITE3MC_ENABLED)
    add_executable(sqlite_cipher_tool tools/sqlite_cipher_tool.cc)
    target_compile_options(sqlite_cipher_tool PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
    target_include_directories(sqlite_cipher_tool PRIVATE
        ${SQLITE3MC_DIR}
    )
    target_link_libraries(sqlite_cipher_tool PRIVATE sqlite3mc)
    if(WIN32)
        target_link_libraries(sqlite_cipher_tool PRIVATE bcrypt)
    endif()
endif()

# ── 复制配置文件 ──────────────────────────────────────────────
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.json")
    configure_file(config.json           ${CMAKE_BINARY_DIR}/config.json           COPYONLY)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/drogon_ctl_model.json")
    configure_file(drogon_ctl_model.json ${CMAKE_BINARY_DIR}/drogon_ctl_model.json COPYONLY)
endif()

# ── 安装目标（`cmake --install build-nginx --prefix /usr/local`）────────────────
install(TARGETS ${PROJECT_NAME} watchdog
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(TARGETS sqlite_cipher_tool
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    OPTIONAL
)
# 配置文件：仅安装模板，不安装含密码的 config.json
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/build-nginx/config.template.json"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/ruoyi-cpp"
    RENAME config.json.example
)
# 示例 Nginx 反代配置
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/NGINX_PROXY_EXAMPLE.conf"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/ruoyi-cpp"
    OPTIONAL
)

# ── 单元测试（默认关闭，CI 中 -DRUOYI_BUILD_TESTS=ON 开启）─────────────────
option(RUOYI_BUILD_TESTS "Build unit tests under tests/" OFF)
if (RUOYI_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "[ruoyi-cpp] Unit tests enabled (run: ctest --output-on-failure)")
endif()
