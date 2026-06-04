cmake_minimum_required(VERSION 3.15)

# ── 插件独立构建脚本 ──────────────────────────────────────────────
#
# 用法：
#   mkdir build-hello && cd build-hello
#   cmake -DPLUGIN_NAME=hello_plugin \
#         -DPLUGIN_SOURCE_DIR=../plugins/hello_plugin \
#         -DDROGON_INSTALL_PREFIX=g:/back/recovered/drogon-master/need/install-static \
#         -DVCPKG_INSTALLED=g:/back/recovered/drogon-master/vcpkg-master/installed/x64-mingw-static \
#         -DMSYS_PREFIX=H:/msys64/msys64/ucrt64 \
#         -DCMAKE_BUILD_TYPE=Release ..
#   cmake --build . --parallel
#
# 输出：
#   plugins/hello_plugin/hello_plugin.dll   ← 前端 import 此路径

set(PLUGIN_NAME "hello_plugin" CACHE STRING "Plugin name")
set(PLUGIN_SOURCE_DIR "" CACHE PATH "Plugin source directory")
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")

if(NOT PLUGIN_SOURCE_DIR)
    message(FATAL_ERROR "PLUGIN_SOURCE_DIR not set")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FS /utf-8 /EHsc")
elseif(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -finput-charset=UTF-8 -fexec-charset=UTF-8")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

# ── Droon 路径 ────────────────────────────────────────────────────
if(NOT DROGON_INSTALL_PREFIX)
    set(DROGON_INSTALL_PREFIX "g:/back/recovered/drogon-master/need/install-static")
endif()
set(DROGON_INCLUDE_DIRS "${DROGON_INSTALL_PREFIX}/include")

if(NOT VCPKG_INSTALLED)
    set(VCPKG_INSTALLED "g:/back/recovered/drogon-master/vcpkg-master/installed/x64-mingw-static")
endif()
if(NOT MSYS_PREFIX)
    set(MSYS_PREFIX "H:/msys64/msys64/ucrt64")
endif()

# ── 查找库 ────────────────────────────────────────────────────────
macro(find_lib NAME VAR)
    find_library(${VAR} NAMES ${ARGV2} PATHS "${VCPKG_INSTALLED}/lib" "${DROGON_INSTALL_PREFIX}/lib"
        "${MSYS_PREFIX}/lib" NO_DEFAULT_PATH)
    if(NOT ${VAR})
        message(FATAL_ERROR "${NAME} not found")
    endif()
endmacro()

find_lib("drogon"     DROGON_LIB    drogon libdrogon)
find_lib("trantor"    TRANTOR_LIB   trantor libtrantor)
find_lib("jsoncpp"    JSONCPP_LIB   jsoncpp libjsoncpp)
find_lib("ssl"        SSL_LIB       ssl libssl)
find_lib("crypto"     CRYPTO_LIB    crypto libcrypto)
find_lib("z"          ZLIB_LIB      z libz)
find_lib("curl"       CURL_LIB      curl libcurl)
find_lib("c-ares"     CARES_LIB     libcares.a cares)
find_lib("brotlienc"  BROTLIENC_LIB libbrotlienc.a)
find_lib("brotlidec"  BROTLIDEC_LIB libbrotlidec.a)
find_lib("brotlicom"  BROTLICOMMON_LIB libbrotlicommon.a)
find_lib("pq"         PQ_LIB        pq libpq)
find_lib("hiredis"    HIREDIS_LIB   hiredis libhiredis)

message(STATUS "DROGON_LIB: ${DROGON_LIB}")

# ── 源文件 ────────────────────────────────────────────────────────
file(GLOB_RECURSE PLUGIN_SOURCES "${PLUGIN_SOURCE_DIR}/*.cc" "${PLUGIN_SOURCE_DIR}/*.cpp")
list(FILTER PLUGIN_SOURCES EXCLUDE REGEX ".*build.*")

add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES})

# ── 输出 DLL 到插件目录 ──────────────────────────────────────────
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/plugins/${PLUGIN_NAME}")
if(WIN32)
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIR}"
        OUTPUT_NAME "${PLUGIN_NAME}"
        SUFFIX ".dll")
else()
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        PREFIX "lib"
        LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}"
        OUTPUT_NAME "${PLUGIN_NAME}"
        SUFFIX ".so")
endif()

# ── 包含目录 ────────────────────────────────────────────────────
target_include_directories(${PLUGIN_NAME} PRIVATE
    "${PLUGIN_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/src"           # libs/plugin/IPlugin.h 等
    "${DROGON_INCLUDE_DIRS}"
    "${VCPKG_INSTALLED}/include"
    "${MSYS_PREFIX}/include"
)

# ── 链接库 ──────────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(${PLUGIN_NAME} PRIVATE
        "${DROGON_LIB}"
        "${TRANTOR_LIB}"
        "${JSONCPP_LIB}"
        "${SSL_LIB}"
        "${CRYPTO_LIB}"
        "${ZLIB_LIB}"
        "${CURL_LIB}"
        "${CARES_LIB}"
        "${BROTLIENC_LIB}"
        "${BROTLIDEC_LIB}"
        "${BROTLICOMMON_LIB}"
        "${PQ_LIB}"
        "${HIREDIS_LIB}"
        ws2_32 wsock32 crypt32 secur32 bcrypt winhttp
        iphlpapi userenv rpcrt4 shlwapi
    )
else()
    target_link_libraries(${PLUGIN_NAME} PRIVATE
        "${DROGON_LIB}"
        "${TRANTOR_LIB}"
        "${JSONCPP_LIB}"
        ssl crypto z curl pthread dl
    )
endif()

# ── 编译宏 ────────────────────────────────────────────────────────
target_compile_definitions(${PLUGIN_NAME} PRIVATE
    RUOYI_PLUGIN_EXPORTS
    WIN32_LEAN_AND_MEAN NOMINMAX
    DROGON_STATIC_DEFINE
    TRANTOR_STATIC_DEFINE
    JSONCPP_STATIC_DEFINE
)

if(MINGW)
    target_compile_options(${PLUGIN_NAME} PRIVATE -Wa,-mbig-obj)
    target_link_options(${PLUGIN_NAME} PRIVATE -Wl,--allow-multiple-definition)
endif()

# ── 复制运行时 DLL ───────────────────────────────────────────────
if(WIN32)
    get_filename_component(_cxx_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_msys_bin "${MSYS_PREFIX}" DIRECTORY)
    # 上级 bin 目录（H:/msys64/msys64/bin）
    string(REGEX REPLACE "/ucrt64" "/bin" _msys_parent_bin "${MSYS_PREFIX}")
    _msys_parent_bin="${_msys_parent_bin}/bin"

    set(_dlls_to_copy
        libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll
        libjsoncpp.dll libz.dll libpq.dll libhiredis.dll
        libssl-3-x64.dll libcrypto-3-x64.dll
        libbrotlicommon.dll libbrotlidec.dll libbrotlienc.dll
        libcurl-4.dll
    )
    foreach(_dll ${_dlls_to_copy})
        foreach(_dir "${_cxx_bin}" "${_msys_bin}" "${_msys_parent_bin}")
            if(EXISTS "${_dir}/${_dll}")
                file(COPY "${_dir}/${_dll}" DESTINATION "${OUTPUT_DIR}")
                break()
            endif()
        endforeach()
    endforeach()
endif()

message(STATUS "[Plugin] Output: ${OUTPUT_DIR}")
message(STATUS "[Plugin] Sources: ${PLUGIN_SOURCES}")
