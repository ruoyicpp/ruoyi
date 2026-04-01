cmake_minimum_required(VERSION 3.15)
project(ruoyi-cpp CXX)

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
set(DROGON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

set(DROGON_INCLUDE_DIRS
    "${DROGON_ROOT}/lib/inc"
    "${DROGON_ROOT}/orm_lib/inc"
    "${DROGON_ROOT}/nosql_lib/redis/inc"
    "${DROGON_ROOT}/trantor"
    "${DROGON_ROOT}/build"
    "${DROGON_ROOT}/build/trantor"
    "${DROGON_ROOT}/build/trantor/exports"
    "${DROGON_ROOT}/build/exports"
)

if(WIN32)
    # ── Windows：使用 vcpkg + msys64 库 ──────────────────────
    set(VCPKG_INCLUDE_DIR "${DROGON_ROOT}/vcpkg-master/installed/x64-mingw-static/include")
    set(VCPKG_LIB_DIR     "${DROGON_ROOT}/vcpkg-master/installed/x64-mingw-static/lib")

    # PostgreSQL libpq 头文件（从 msys64 复制，避免系统头文件冲突）
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/libpq_include")
    foreach(_hdr libpq-fe.h libpq-events.h postgres_ext.h)
        if(EXISTS "H:/msys64/msys64/mingw64/include/${_hdr}")
            file(COPY "H:/msys64/msys64/mingw64/include/${_hdr}"
                 DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/libpq_include")
        endif()
    endforeach()
    set(POSTGRESQL_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/libpq_include")
    set(POSTGRESQL_LIB_DIR "H:/msys64/msys64/mingw64/lib")

    find_library(DROGON_LIB   NAMES drogon  libdrogon  PATHS "${DROGON_ROOT}/build"         NO_DEFAULT_PATH)
    find_library(TRANTOR_LIB  NAMES trantor libtrantor PATHS "${DROGON_ROOT}/build/trantor" NO_DEFAULT_PATH)
    find_library(JSONCPP_LIB  NAMES jsoncpp libjsoncpp PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(ZLIB_LIB     NAMES zlib    libzlib    PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(SSL_LIB      NAMES ssl     libssl     PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(CRYPTO_LIB   NAMES crypto  libcrypto  PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(HIREDIS_LIB  NAMES hiredis libhiredis PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(BROTLIENC_LIB    NAMES brotlienc    libbrotlienc    PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(BROTLIDEC_LIB    NAMES brotlidec    libbrotlidec    PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(BROTLICOMMON_LIB NAMES brotlicommon libbrotlicommon PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(PQ_LIB       NAMES pq libpq   PATHS ${POSTGRESQL_LIB_DIR} ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(SQLITE3_LIB  NAMES sqlite3 libsqlite3 PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
    find_library(MARIADB_LIB  NAMES mariadbclient libmariadbclient PATHS ${VCPKG_LIB_DIR} NO_DEFAULT_PATH)
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

# ── 源文件 ────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    src/main.cc
    src/common/*.cc
    src/services/*.cc
    src/system/services/*.cc
    src/system/controllers/*.cc
    src/ai/controllers/*.cc
    src/filters/*.cc
)

add_executable(${PROJECT_NAME} ${SOURCES})

# ── 包含目录 ──────────────────────────────────────────────────
if(WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${DROGON_INCLUDE_DIRS}
        ${VCPKG_INCLUDE_DIR}
        ${POSTGRESQL_INCLUDE_DIR}
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${DROGON_INCLUDE_DIRS}
        ${JSONCPP_INCLUDE_DIRS}
        /usr/local/include
        /usr/include
        /usr/include/postgresql
    )
endif()

# ── 链接库 ────────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${DROGON_LIB}
        ${TRANTOR_LIB}
        ${PQ_LIB}
        ${HIREDIS_LIB}
        ${MARIADB_LIB}
        ${SQLITE3_LIB}
        ${JSONCPP_LIB}
        ${SSL_LIB}
        ${CRYPTO_LIB}
        ${BROTLIENC_LIB}
        ${BROTLIDEC_LIB}
        ${BROTLICOMMON_LIB}
        ${ZLIB_LIB}
        ws2_32 wsock32 crypt32 secur32 bcrypt
        iphlpapi userenv rpcrt4 shlwapi winpthread psapi
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${DROGON_LIB}
        ${TRANTOR_LIB}
        ${PQ_LIB}
        ${HIREDIS_LIB}
        ${SQLITE3_LIB}
        ${JSONCPP_LIBRARIES}
        ssl crypto
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

# ── 复制配置文件 ──────────────────────────────────────────────
configure_file(config.json           ${CMAKE_BINARY_DIR}/config.json           COPYONLY)
configure_file(drogon_ctl_model.json ${CMAKE_BINARY_DIR}/drogon_ctl_model.json COPYONLY)
