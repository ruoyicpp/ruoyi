/*
** sqlite3.h shim — 让所有 #include <sqlite3.h> 透明命中 sqlite3mc
**
** 本文件替代系统/vcpkg 的 sqlite3.h。CMakeLists.txt 把此目录加入最高优先级
** include 路径，确保编译器先命中这里而不是 vcpkg 的 sqlite3.h。
** 若未启用加密（HAVE_SQLCIPHER 未定义），则直接使用系统 sqlite3.h。
*/
#pragma once
#ifdef HAVE_SQLCIPHER
#  include "sqlite3mc_amalgamation.h"
#else
#  include_next <sqlite3.h>
#endif
