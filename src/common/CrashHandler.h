#pragma once
/*
 * CrashHandler.h  ——  Windows 全场景崩溃捕获
 *
 * 覆盖：
 *   SEH 异常        (空指针/越界/除零/非法指令/堆损坏...)
 *   C++ 未捕获异常  (std::terminate / throw 无 catch)
 *   信号             SIGABRT / SIGSEGV / SIGFPE / SIGILL / SIGTERM
 *   纯虚函数调用    _set_purecall_handler
 *   CRT 非法参数    _set_invalid_parameter_handler
 *   栈溢出          单独辅助线程写日志（原线程栈已满无法使用）
 *
 * 输出：
 *   crash_YYYYMMDD_HHMMSS.log  —— 可读文本（进程/系统/线程/栈/模块）
 *   crash_YYYYMMDD_HHMMSS.dmp  —— Minidump（WinDbg / VS 可分析）
 *
 * 使用：
 *   在 main() 第一行调用 CrashHandler::install("./logs");
 */
#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <signal.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <exception>

namespace CrashHandler {

// ── 全局状态（均为 POD / atomic，避免崩溃期间构造/析构） ─────────
static std::atomic<int>  g_handling{0};          // 重入保护
static char   g_logDir[512]   = "./logs";        // 日志目录
static char   g_exePath[1024] = "";              // exe 完整路径
static DWORD  g_mainThreadId  = 0;               // 主线程 ID（install 时记录）
static ULONGLONG g_startTick  = 0;               // GetTickCount64 at install

// ── 静态输出缓冲（崩溃期间不分配堆内存） ─────────────────────────
static char g_buf[65536];   // 64 KB 文本缓冲
static int  g_bufLen = 0;

static void bufReset() { g_bufLen = 0; g_buf[0] = '\0'; }
static void bufAppend(const char *s) {
    int rem = (int)sizeof(g_buf) - g_bufLen - 1;
    if (rem <= 0) return;
    int n = (int)strnlen(s, (size_t)rem);
    memcpy(g_buf + g_bufLen, s, (size_t)n);
    g_bufLen += n;
    g_buf[g_bufLen] = '\0';
}
static void bufAppendf(const char *fmt, ...) {
    char tmp[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    bufAppend(tmp);
}

// ── 时间戳 ───────────────────────────────────────────────────────
static void formatTimestamp(char *out, size_t sz) {
    SYSTEMTIME st; GetLocalTime(&st);
    snprintf(out, sz, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}
static void formatTimestampFile(char *out, size_t sz) {
    SYSTEMTIME st; GetLocalTime(&st);
    snprintf(out, sz, "%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond);
}

// ── OS 版本 ──────────────────────────────────────────────────────
static void appendOsVersion() {
    OSVERSIONINFOEXW ovi{}; ovi.dwOSVersionInfoSize = sizeof(ovi);
    // RtlGetVersion bypasses the compatibility shim that GetVersionExW uses
    auto *fn = (LONG(*)(OSVERSIONINFOEXW *))
               (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    if (fn) fn(&ovi);
    else    GetVersionExW((OSVERSIONINFOW *)&ovi);
    bufAppendf("OS       : Windows %lu.%lu (Build %lu)\n",
               ovi.dwMajorVersion, ovi.dwMinorVersion, ovi.dwBuildNumber);
}

// ── 内存信息 ─────────────────────────────────────────────────────
static void appendMemInfo() {
    MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    bufAppendf("RAM      : %llu MB free / %llu MB total\n",
               ms.ullAvailPhys / 1048576ULL,
               ms.ullTotalPhys / 1048576ULL);

    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        bufAppendf("Proc Mem : %.1f MB (WorkingSet)\n",
                   pmc.WorkingSetSize / 1048576.0);
}

// ── CPU 核数 ─────────────────────────────────────────────────────
static void appendCpuInfo() {
    SYSTEM_INFO si{}; GetSystemInfo(&si);
    bufAppendf("CPU      : %u logical cores\n", (unsigned)si.dwNumberOfProcessors);
}

// ── 运行时长 ─────────────────────────────────────────────────────
static void appendUptime() {
    if (!g_startTick) return;
    ULONGLONG sec = (GetTickCount64() - g_startTick) / 1000ULL;
    bufAppendf("Uptime   : %llud %lluh %llum %llus\n",
               sec/86400, (sec%86400)/3600, (sec%3600)/60, sec%60);
}

// ── 线程信息 ─────────────────────────────────────────────────────
static void appendThreadInfo() {
    bufAppendf("ThreadId : %lu\n", (unsigned long)GetCurrentThreadId());
    bufAppendf("MainTid  : %lu\n", (unsigned long)g_mainThreadId);
}

// ── 命令行 ───────────────────────────────────────────────────────
static void appendCmdLine() {
    char tmp[1024];
    WideCharToMultiByte(CP_UTF8, 0, GetCommandLineW(), -1, tmp, sizeof(tmp), nullptr, nullptr);
    bufAppendf("CmdLine  : %s\n", tmp);
}

// ── 已加载模块列表 ───────────────────────────────────────────────
static void appendModules() {
    bufAppend("\n[Loaded Modules]\n");
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (hSnap == INVALID_HANDLE_VALUE) { bufAppend("  (unavailable)\n"); return; }
    MODULEENTRY32W me{}; me.dwSize = sizeof(me);
    if (Module32FirstW(hSnap, &me)) {
        do {
            char name[256];
            WideCharToMultiByte(CP_UTF8, 0, me.szModule, -1, name, sizeof(name), nullptr, nullptr);
            bufAppendf("  %-30s  base=0x%016llx  size=%5lu KB\n",
                       name,
                       (unsigned long long)(uintptr_t)me.modBaseAddr,
                       (unsigned long)(me.modBaseSize / 1024));
        } while (Module32NextW(hSnap, &me));
    }
    CloseHandle(hSnap);
}

// ── 调用栈（DbgHelp + 降级到原始地址） ──────────────────────────
static void appendStackTrace(CONTEXT *ctx) {
    bufAppend("\n[Stack Trace]\n");

    // 1. 先用 RtlCaptureStackBackTrace 快速拿地址（不需要 ctx）
    void *frames[64] = {};
    USHORT nFrames = 0;
    if (ctx) {
        // 从 ctx 重建 STACKFRAME64
        HANDLE proc = GetCurrentProcess();
        HANDLE thr  = GetCurrentThread();
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        SymInitialize(proc, nullptr, TRUE);

        STACKFRAME64 sf{};
#ifdef _M_X64
        DWORD machType = IMAGE_FILE_MACHINE_AMD64;
        sf.AddrPC.Offset    = ctx->Rip;
        sf.AddrFrame.Offset = ctx->Rbp;
        sf.AddrStack.Offset = ctx->Rsp;
#else
        DWORD machType = IMAGE_FILE_MACHINE_I386;
        sf.AddrPC.Offset    = ctx->Eip;
        sf.AddrFrame.Offset = ctx->Ebp;
        sf.AddrStack.Offset = ctx->Esp;
#endif
        sf.AddrPC.Mode = sf.AddrFrame.Mode = sf.AddrStack.Mode = AddrModeFlat;

        char symBuf[sizeof(SYMBOL_INFO) + 256]{};
        auto *sym = reinterpret_cast<SYMBOL_INFO *>(symBuf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen   = 255;

        IMAGEHLP_LINE64 line{}; line.SizeOfStruct = sizeof(line);

        for (int i = 0; i < 62; ++i) {
            if (!StackWalk64(machType, proc, thr, &sf, ctx,
                             nullptr, SymFunctionTableAccess64,
                             SymGetModuleBase64, nullptr)) break;
            if (!sf.AddrPC.Offset) break;

            DWORD64 disp64 = 0;
            bool hasName = SymFromAddr(proc, sf.AddrPC.Offset, &disp64, sym) != 0;
            DWORD  disp32 = 0;
            bool hasLine = SymGetLineFromAddr64(proc, sf.AddrPC.Offset, &disp32, &line) != 0;

            // 获取所属模块
            IMAGEHLP_MODULE64 mod{}; mod.SizeOfStruct = sizeof(mod);
            SymGetModuleInfo64(proc, sf.AddrPC.Offset, &mod);

            if (hasName) {
                bufAppendf("  #%02d  %-50s +%llu", i, sym->Name, (unsigned long long)disp64);
            } else {
                DWORD64 base = SymGetModuleBase64(proc, sf.AddrPC.Offset);
                bufAppendf("  #%02d  0x%016llx (base=0x%llx +0x%llx)",
                           i,
                           (unsigned long long)sf.AddrPC.Offset,
                           (unsigned long long)base,
                           (unsigned long long)(sf.AddrPC.Offset - base));
            }
            if (hasLine)
                bufAppendf("  @ %s:%lu", line.FileName, (unsigned long)line.LineNumber);
            if (mod.ImageName[0])
                bufAppendf("  [%s]", mod.ModuleName);
            bufAppend("\n");

            frames[nFrames++] = (void *)(uintptr_t)sf.AddrPC.Offset;
        }
        SymCleanup(proc);
    } else {
        // 无 ctx（信号/terminate/纯虚），直接用 RtlCaptureStackBackTrace
        nFrames = RtlCaptureStackBackTrace(1, 62, frames, nullptr);
        for (USHORT i = 0; i < nFrames; ++i) {
            bufAppendf("  #%02d  0x%016llx\n",
                       (int)i, (unsigned long long)(uintptr_t)frames[i]);
        }
    }

    // addr2line 离线分析提示
    bufAppend("\n[addr2line command (run in build dir)]\n  addr2line -e ruoyi-cpp.exe -f -C -p");
    for (USHORT i = 0; i < nFrames && i < 32; ++i)
        bufAppendf(" 0x%llx", (unsigned long long)(uintptr_t)frames[i]);
    bufAppend("\n");
}

// ── 写文件（低级 CreateFile/WriteFile，无 CRT 缓冲） ─────────────
static void flushToFile(const char *suffix, const char *ext) {
    char ts[32]; formatTimestampFile(ts, sizeof(ts));
    char path[1024];
    snprintf(path, sizeof(path), "%s/crash_%s%s%s", g_logDir, ts, suffix, ext);
    HANDLE hf = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(hf, g_buf, (DWORD)g_bufLen, &written, nullptr);
    FlushFileBuffers(hf);
    CloseHandle(hf);
    // stderr 也输出路径
    fprintf(stderr, "\n[CRASH] log written: %s\n", path);
    fflush(stderr);
}

// ── 写 Minidump ──────────────────────────────────────────────────
static void writeMiniDump(EXCEPTION_POINTERS *ep, const char *suffix) {
    char ts[32]; formatTimestampFile(ts, sizeof(ts));
    char path[1024];
    snprintf(path, sizeof(path), "%s/crash_%s%s.dmp", g_logDir, ts, suffix);
    HANDLE hf = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return;

    MINIDUMP_TYPE dmpType = (MINIDUMP_TYPE)(
        MiniDumpWithDataSegs |
        MiniDumpWithHandleData |
        MiniDumpWithUnloadedModules |
        MiniDumpWithIndirectlyReferencedMemory |
        MiniDumpWithProcessThreadData);

    if (ep) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId          = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers    = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                          hf, dmpType, &mei, nullptr, nullptr);
    } else {
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                          hf, dmpType, nullptr, nullptr, nullptr);
    }
    CloseHandle(hf);
    fprintf(stderr, "[CRASH] minidump: %s\n", path);
    fflush(stderr);
}

// ── 汇集所有通用信息 ─────────────────────────────────────────────
static void buildHeader(const char *reason, const char *detail) {
    char ts[64]; formatTimestamp(ts, sizeof(ts));

    bufAppend("=========================================================\n");
    bufAppend("              RuoYi-Cpp  CRASH  REPORT\n");
    bufAppend("=========================================================\n");
    bufAppendf("Time     : %s\n", ts);
    appendUptime();
    bufAppendf("Reason   : %s\n", reason);
    if (detail && detail[0])
        bufAppendf("Detail   : %s\n", detail);
    bufAppend("\n[Process]\n");
    bufAppendf("PID      : %lu\n",  (unsigned long)GetCurrentProcessId());
    bufAppendf("EXE      : %s\n",   g_exePath);
    appendCmdLine();
    bufAppend("\n[System]\n");
    appendOsVersion();
    appendCpuInfo();
    appendMemInfo();
    bufAppend("\n[Thread]\n");
    appendThreadInfo();
}

// ── 结构化异常描述 ───────────────────────────────────────────────
static void describeException(EXCEPTION_POINTERS *ep,
                              char *reason, size_t rsz,
                              char *detail, size_t dsz) {
    if (!ep) { strncpy(reason, "SEH (no info)", rsz); detail[0]='\0'; return; }
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    const char *name = "Unknown";
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:         name = "Access Violation";        break;
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    name = "Array Bounds Exceeded";   break;
        case EXCEPTION_BREAKPOINT:               name = "Breakpoint";              break;
        case EXCEPTION_DATATYPE_MISALIGNMENT:    name = "Datatype Misalignment";   break;
        case EXCEPTION_FLT_DENORMAL_OPERAND:     name = "FLT Denormal Operand";    break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       name = "FLT Divide by Zero";      break;
        case EXCEPTION_FLT_INEXACT_RESULT:       name = "FLT Inexact Result";      break;
        case EXCEPTION_FLT_INVALID_OPERATION:    name = "FLT Invalid Operation";   break;
        case EXCEPTION_FLT_OVERFLOW:             name = "FLT Overflow";            break;
        case EXCEPTION_FLT_STACK_CHECK:          name = "FLT Stack Check";         break;
        case EXCEPTION_FLT_UNDERFLOW:            name = "FLT Underflow";           break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:      name = "Illegal Instruction";     break;
        case EXCEPTION_IN_PAGE_ERROR:            name = "In-Page Error";           break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       name = "INT Divide by Zero";      break;
        case EXCEPTION_INT_OVERFLOW:             name = "INT Overflow";            break;
        case EXCEPTION_INVALID_DISPOSITION:      name = "Invalid Disposition";     break;
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: name = "Noncontinuable";          break;
        case EXCEPTION_PRIV_INSTRUCTION:         name = "Privileged Instruction";  break;
        case EXCEPTION_SINGLE_STEP:              name = "Single Step";             break;
        case EXCEPTION_STACK_OVERFLOW:           name = "Stack Overflow";          break;
        case 0xE06D7363:                         name = "C++ Exception";           break;
        case 0xC0000374:                         name = "Heap Corruption";         break;
    }
    snprintf(reason, rsz, "SEH 0x%08lX (%s)", (unsigned long)code, name);

    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2)
        snprintf(detail, dsz, "op=%s  address=0x%016llx",
                 ep->ExceptionRecord->ExceptionInformation[0] ? "WRITE" : "READ",
                 (unsigned long long)ep->ExceptionRecord->ExceptionInformation[1]);
    else if (ep->ExceptionRecord->ExceptionAddress)
        snprintf(detail, dsz, "at 0x%016llx",
                 (unsigned long long)(uintptr_t)ep->ExceptionRecord->ExceptionAddress);
    else
        detail[0] = '\0';
}

// ── 核心写崩溃报告（在辅助线程中执行，避免栈溢出限制） ──────────
struct CrashTask {
    EXCEPTION_POINTERS *ep;
    char reason[256];
    char detail[512];
};
static CrashTask g_task;

static DWORD WINAPI crashWriterThread(LPVOID) {
    bufReset();
    buildHeader(g_task.reason, g_task.detail);
    appendStackTrace(g_task.ep ? g_task.ep->ContextRecord : nullptr);
    appendModules();
    bufAppend("\n=========================================================\n");
    flushToFile("", ".log");
    writeMiniDump(g_task.ep, "");
    return 0;
}

static void dispatchCrash(EXCEPTION_POINTERS *ep,
                          const char *reason, const char *detail) {
    // 重入保护：同一进程只处理一次崩溃
    if (g_handling.exchange(1) != 0) return;

    strncpy(g_task.reason, reason,  sizeof(g_task.reason) - 1);
    strncpy(g_task.detail, detail,  sizeof(g_task.detail) - 1);
    g_task.ep = ep;

    // 创建独立线程写日志（栈溢出时原线程栈不可用）
    HANDLE ht = CreateThread(nullptr, 1024*1024, crashWriterThread, nullptr, 0, nullptr);
    if (ht) {
        WaitForSingleObject(ht, 8000); // 最多等 8 秒
        CloseHandle(ht);
    }
}

// ── SEH 过滤器 ───────────────────────────────────────────────────
static LONG WINAPI sehFilter(EXCEPTION_POINTERS *ep) {
    // 跳过调试断点、单步
    if (ep && (ep->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT ||
               ep->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP))
        return EXCEPTION_CONTINUE_SEARCH;

    char reason[256], detail[512];
    describeException(ep, reason, sizeof(reason), detail, sizeof(detail));
    dispatchCrash(ep, reason, detail);
    return EXCEPTION_CONTINUE_SEARCH;
}

// ── C++ std::terminate ───────────────────────────────────────────
static void terminateHandler() {
    char detail[512] = "";
    try {
        auto ep = std::current_exception();
        if (ep) {
            try { std::rethrow_exception(ep); }
            catch (const std::exception &e) {
                snprintf(detail, sizeof(detail), "%s", e.what()); }
            catch (...) {
                strncpy(detail, "unknown exception type", sizeof(detail)-1); }
        }
    } catch (...) {}
    dispatchCrash(nullptr, "std::terminate() — unhandled C++ exception", detail);
    std::abort();
}

// ── Signal 处理器 ────────────────────────────────────────────────
static void signalHandler(int sig) {
    const char *name = "Unknown Signal";
    switch (sig) {
        case SIGABRT: name = "SIGABRT (abort / assert / pure-virt)";  break;
        case SIGSEGV: name = "SIGSEGV (segmentation fault)";          break;
        case SIGFPE:  name = "SIGFPE  (floating point exception)";     break;
        case SIGILL:  name = "SIGILL  (illegal instruction)";          break;
        case SIGTERM: name = "SIGTERM (terminate request)";            break;
    }
    dispatchCrash(nullptr, name, "");
    // 恢复默认并重发，让系统记录正确退出码
    signal(sig, SIG_DFL);
    raise(sig);
}

// ── 纯虚函数调用 ─────────────────────────────────────────────────
static void purecallHandler() {
    dispatchCrash(nullptr, "Pure virtual function call", "");
    std::abort();
}

// ── CRT 非法参数（如 printf 的 nullptr format） ──────────────────
static void invalidParamHandler(const wchar_t *expr, const wchar_t *func,
                                const wchar_t *file, unsigned int line,
                                uintptr_t /*reserved*/) {
    char detail[512] = "";
    if (func && file)
        snprintf(detail, sizeof(detail), "func=%ls  file=%ls:%u", func, file, line);
    else if (expr)
        snprintf(detail, sizeof(detail), "expr=%ls", expr);
    dispatchCrash(nullptr, "CRT invalid parameter", detail);
    std::abort();
}

// ── VEH：专门捕获 ntdll 堆损坏（绕过 SetUnhandledExceptionFilter 的崩溃）──
// 仅对已知"不可恢复"异常码写日志，其余全部透传
static LONG CALLBACK vehFilter(EXCEPTION_POINTERS *ep) {
    if (!ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
    DWORD code = ep->ExceptionRecord->ExceptionCode;

    // 只处理这几类 ntdll 直接触发的致命异常（其余留给 sehFilter 或正常 catch 处理）
    switch (code) {
        case 0xC0000374:  // STATUS_HEAP_CORRUPTION
        case 0xC0000409:  // STATUS_STACK_BUFFER_OVERRUN (fast-fail)
        case 0xC0000602:  // STATUS_CORRUPT_SYSTEM_FILE
            break;
        default:
            return EXCEPTION_CONTINUE_SEARCH;
    }

    char reason[64];
    snprintf(reason, sizeof(reason), "VEH 0x%08lX", (unsigned long)code);
    if (code == 0xC0000374) strncat(reason, " (Heap Corruption)", sizeof(reason) - strlen(reason) - 1);
    if (code == 0xC0000409) strncat(reason, " (Stack Buffer Overrun)", sizeof(reason) - strlen(reason) - 1);

    char detail[128] = "";
    if (ep->ExceptionRecord->ExceptionAddress)
        snprintf(detail, sizeof(detail), "Address=0x%p", ep->ExceptionRecord->ExceptionAddress);

    dispatchCrash(ep, reason, detail);
    return EXCEPTION_CONTINUE_SEARCH;
}

// ── 一键安装（main() 第一行调用）────────────────────────────────
inline void install(const std::string &logDir = "./logs") {
    // 保存状态
    strncpy(g_logDir, logDir.c_str(), sizeof(g_logDir) - 1);
    g_startTick    = GetTickCount64();
    g_mainThreadId = GetCurrentThreadId();

    // 获取 exe 路径
    wchar_t wpath[1024]{};
    GetModuleFileNameW(nullptr, wpath, 1024);
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, g_exePath, sizeof(g_exePath), nullptr, nullptr);

    // 确保日志目录存在
    CreateDirectoryA(logDir.c_str(), nullptr);

    // VEH 注册在最前（捕获 ntdll 直接触发的堆损坏）
    AddVectoredExceptionHandler(1, vehFilter);

    // 注册各类处理器
    SetUnhandledExceptionFilter(sehFilter);
    std::set_terminate(terminateHandler);

    signal(SIGABRT, signalHandler);
    signal(SIGSEGV, signalHandler);
    signal(SIGFPE,  signalHandler);
    signal(SIGILL,  signalHandler);
    signal(SIGTERM, signalHandler);

    _set_purecall_handler(purecallHandler);
    _set_invalid_parameter_handler(invalidParamHandler);
}

} // namespace CrashHandler

#else
// ── 非 Windows 平台空实现 ─────────────────────────────────────────
#include <csignal>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <fstream>
#include <string>
#include <atomic>
#include <ctime>

namespace CrashHandler {

static std::atomic<int> g_handling{0};
static std::string g_logDir = "./logs";

static void writeSimpleLog(const char *reason, const char *detail = "") {
    if (g_handling.exchange(1) != 0) return;
    time_t t = time(nullptr);
    char ts[32]; strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", localtime(&t));
    std::string path = g_logDir + "/crash_" + ts + ".log";
    if (FILE *f = fopen(path.c_str(), "w")) {
        fprintf(f, "========= CRASH =========\n");
        fprintf(f, "Time   : %s\n", ts);
        fprintf(f, "Reason : %s\n", reason);
        if (detail && *detail) fprintf(f, "Detail : %s\n", detail);
        fclose(f);
    }
    fprintf(stderr, "[CRASH] %s  log=%s\n", reason, path.c_str());
}

static void sigHandler(int sig) {
    const char *name = sig==SIGABRT?"SIGABRT":sig==SIGSEGV?"SIGSEGV":
                       sig==SIGFPE?"SIGFPE":sig==SIGILL?"SIGILL":"signal";
    writeSimpleLog(name);
    signal(sig, SIG_DFL); raise(sig);
}

static void termHandler() {
    char detail[256] = "";
    try {
        auto ep = std::current_exception();
        if (ep) { try { std::rethrow_exception(ep); }
                  catch (const std::exception &e) { strncpy(detail,e.what(),255); }
                  catch (...) { strncpy(detail,"unknown",255); } }
    } catch (...) {}
    writeSimpleLog("std::terminate()", detail);
    std::abort();
}

inline void install(const std::string &logDir = "./logs") {
    g_logDir = logDir;
    std::set_terminate(termHandler);
    signal(SIGABRT, sigHandler);
    signal(SIGSEGV, sigHandler);
    signal(SIGFPE,  sigHandler);
    signal(SIGILL,  sigHandler);
    signal(SIGTERM, sigHandler);
}

} // namespace CrashHandler
#endif
