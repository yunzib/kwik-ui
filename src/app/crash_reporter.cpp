// ============================================================================
// crash_reporter.cpp — minidump 落盘实现（win32）
//
// SetUnhandledExceptionFilter 捕获未处理异常 → MiniDumpWriteDump 写入
// crashes/<时间戳>.dmp。dump 含异常指针与间接引用内存，够定位空指针/
// 越界类崩溃；返回 EXECUTE_HANDLER 让进程按默认流程退出。
// MiniDumpWriteDump 经 LoadLibrary 动态绑定（llvm-mingw 的 dbghelp.h 无
// 此声明，动态绑定同时免去 dbghelp 链接）。
// ============================================================================
module;
// C printf 族桥声明（GCC16 std 模块边界，见头注释）
#include <kwik/c_bridge.h>
// GCC16 模块边界守卫：C 标准头直取 mingw-w64 纯 C 版本（include_next），
// 绕开 libstdc++ 的 C++ 包装头——否则 windows.h→stdlib.h→mm_malloc→xmmintrin
// 的文本链与 import std 的同名声明产生语言链接冲突
#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS 1
// 内在函数头同因：mingw intrin.h 将 x86intrin 包进 extern "C"（文本侧 C 链接），
// 与 std 模块内 C++ 链接的同名声明冲突——预置 guard 使文本路径落空（声明仍由
// import std 提供，用户代码无感）
#define _MM_MALLOC_H_INCLUDED
#define _X86INTRIN_H_INCLUDED
#define _IMMINTRIN_H_INCLUDED
#define _XMMINTRIN_H_INCLUDED
#define _EMMINTRIN_H_INCLUDED
#define _MMINTRIN_H_INCLUDED
#define _PMMINTRIN_H_INCLUDED
#define _CPUID_H_INCLUDED
#include <windows.h>
// localtime_s 为平台扩展（std 模块不含）；time.h 无 libstdc++ C++ 包装，纯 C 直通
#include <time.h>

module kwik.app.crash_reporter;
import kwik.core.log;

import std;

#if defined(_WIN32)

// MSDN 签名的本地最小声明（DumpType 传值 0x4 = MiniDumpNormal |
// MiniDumpWithIndirectlyReferencedMemory）
using MiniDumpWriteDumpFn = BOOL(WINAPI *)(HANDLE hProcess, DWORD ProcessId, HANDLE hFile, DWORD DumpType,
                                           PVOID ExceptionParam, PVOID UserStreamParam, PVOID CallbackParam);

struct CrashDumpExceptionInfo {
    DWORD ThreadId;
    EXCEPTION_POINTERS *ExceptionPointers;
    BOOL ClientPointers;
};

// 崩溃过滤器：写 minidump 后交还默认处理（进程退出）
static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep) {
    std::error_code ec;
    std::filesystem::create_directories("crashes", ec);
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    char name[64];
    kwik_snprintf(name, sizeof(name), "crashes/kwik_%04d%02d%02d_%02d%02d%02d.dmp",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    HMODULE dbg = LoadLibraryA("dbghelp.dll");
    if (!dbg) {
        Log::error("崩溃转储失败: dbghelp.dll 加载失败");
        return EXCEPTION_EXECUTE_HANDLER;
    }
    auto writeDump = reinterpret_cast<MiniDumpWriteDumpFn>(GetProcAddress(dbg, "MiniDumpWriteDump"));
    if (!writeDump) {
        Log::error("崩溃转储失败: MiniDumpWriteDump 解析失败");
        FreeLibrary(dbg);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    HANDLE file = CreateFileA(name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        Log::error("崩溃转储创建失败: {} (GetLastError={})", name, (int)GetLastError());
        FreeLibrary(dbg);
        return EXCEPTION_EXECUTE_HANDLER;
    }
    CrashDumpExceptionInfo info{GetCurrentThreadId(), ep, FALSE};
    BOOL ok = writeDump(GetCurrentProcess(), GetCurrentProcessId(), file, 0x4, &info, nullptr, nullptr);
    CloseHandle(file);
    FreeLibrary(dbg);
    Log::error("崩溃转储已写入: {} ({})", name, ok ? "ok" : "failed");
    return EXCEPTION_EXECUTE_HANDLER;
}

// 受控自检：延迟触发空指针写（验证转储链路；仅 KWIK_CRASH_TEST=1 时）
static void crash_self_test() {
    std::thread([] {
        Sleep(3000);
        Log::warn("KWIK_CRASH_TEST：触发受控崩溃");
        volatile int *p = nullptr;
        *p = 1;
    }).detach();
}

void install_crash_reporter() {
    SetUnhandledExceptionFilter(crash_filter);
    if (std::getenv("KWIK_CRASH_TEST")) crash_self_test();
}

#else    // 非 win32：占位（POSIX 信号 + coredump 方案随平台层立项）

void install_crash_reporter() {}

#endif
