/**
 * crash_handler.cc — cross-platform crash reporting for mllnm_rtb_sandbox.
 *
 * Linux/macOS: signal handler for SIGSEGV/SIGABRT/SIGBUS/SIGFPE
 *   - writes crash report to ~/.local/state/millennium/crash_dumps/<plugin>-<ts>/crash.log
 *   - enables core dumps (RLIMIT_CORE)
 *   - uses backtrace() + backtrace_symbols() for C stack trace
 *
 * Windows: SetUnhandledExceptionFilter
 *   - writes crash report to <steam>/ext/crash_dumps/<plugin>-<ts>/crash.log
 *   - writes a minidump .dmp alongside the log
 *   - uses CaptureStackBackTrace + SymFromAddr for C stack trace
 */

#include "crash_handler.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <string>

#ifndef MILLENNIUM_VERSION
#define MILLENNIUM_VERSION "unknown"
#endif
#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "unknown"
#endif

static std::string s_plugin_name;
static std::string s_backend_file;
static std::string s_steam_path;
static std::string s_crash_dump_dir;

/** Create the crash dump directory (pre-determined by the parent process). */
static std::string make_crash_dir()
{
    std::error_code ec;
    std::filesystem::create_directories(s_crash_dump_dir, ec);
    return s_crash_dump_dir;
}

#ifndef _WIN32

#include <execinfo.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>

static const char* signal_name(int sig)
{
    switch (sig) {
        case SIGSEGV:
            return "SIGSEGV (Segmentation fault)";
        case SIGABRT:
            return "SIGABRT (Aborted)";
        case SIGBUS:
            return "SIGBUS (Bus error)";
        case SIGFPE:
            return "SIGFPE (Floating-point exception)";
        case SIGILL:
            return "SIGILL (Illegal instruction)";
        case SIGSYS:
            return "SIGSYS (Bad system call)";
        default:
            return "Unknown signal";
    }
}

static void crash_signal_handler(int sig)
{
    std::string dir = make_crash_dir();
    std::string log_path = dir + "/crash.log";

    FILE* f = fopen(log_path.c_str(), "w");
    if (!f) f = stderr;

    fprintf(f, "=== Millennium Plugin Crash Report ===\n\n");
    fprintf(f, "Millennium Version : %s\n", MILLENNIUM_VERSION);
    fprintf(f, "Git Commit         : %s\n", GIT_COMMIT_HASH);
    fprintf(f, "Plugin             : %s\n", s_plugin_name.c_str());
    fprintf(f, "Backend File       : %s\n", s_backend_file.c_str());
    fprintf(f, "Signal             : %s (%d)\n", signal_name(sig), sig);
    fprintf(f, "PID                : %d\n\n", getpid());

    fprintf(f, "--- C Stack Trace ---\n");

    void* frames[128];
    int count = backtrace(frames, 128);
    char** symbols = backtrace_symbols(frames, count);

    if (symbols) {
        for (int i = 0; i < count; ++i) {
            fprintf(f, "  #%d  %s\n", i, symbols[i]);
        }
        free(symbols);
    } else {
        fprintf(f, "  (backtrace_symbols failed)\n");
    }

    fprintf(f, "\n--- End of Crash Report ---\n");
    fprintf(f, "Core dump may be available if ulimit was set.\n");

    if (f != stderr) {
        fclose(f);
        fprintf(stderr, "\n[lua-host] CRASH: Plugin '%s' received %s\n", s_plugin_name.c_str(), signal_name(sig));
        fprintf(stderr, "[lua-host] Crash report: %s\n", log_path.c_str());
    }

    /* re-raise with default handler so the OS produces a core dump */
    signal(sig, SIG_DFL);
    raise(sig);
}

void install_crash_handler(const char* plugin_name, const char* backend_file, const char* steam_path, const char* crash_dump_dir)
{
    s_plugin_name = plugin_name;
    s_backend_file = backend_file;
    s_steam_path = steam_path;
    s_crash_dump_dir = crash_dump_dir;

    /* enable core dumps */
    struct rlimit rl;
    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &rl);

    struct sigaction sa{};
    sa.sa_handler = crash_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND; /* one-shot — re-raise goes to default */

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGSYS, &sa, nullptr);
}

#else

#include <windows.h>
#include <dbghelp.h>

#pragma comment(lib, "dbghelp.lib")

static std::atomic<bool> s_crash_in_progress{ false };

static const char* exception_name(DWORD code)
{
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
            return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_STACK_OVERFLOW:
            return "EXCEPTION_STACK_OVERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_DATATYPE_MISALIGNMENT:
            return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_IN_PAGE_ERROR:
            return "EXCEPTION_IN_PAGE_ERROR";
        case EXCEPTION_PRIV_INSTRUCTION:
            return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        case 0xC0000409:
            return "STATUS_STACK_BUFFER_OVERRUN (/GS)";
        case 0xE0000001:
            return "std::terminate / abort()";
        default:
            return "Unknown exception";
    }
}

static bool is_fatal_exception(DWORD code)
{
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        case EXCEPTION_DATATYPE_MISALIGNMENT:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        case 0xC0000409: /* STATUS_STACK_BUFFER_OVERRUN */
        case 0xE0000001: /* our custom terminate code */
            return true;
        default:
            return false;
    }
}

static void write_minidump(EXCEPTION_POINTERS* ep, const std::string& path)
{
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpWithDataSegs, ep ? &mei : nullptr, nullptr, nullptr);
    CloseHandle(hFile);
}

static void append_plugin_log(FILE* f)
{
    const char* logs_path = std::getenv("MILLENNIUM__LOGS_PATH");
    if (!logs_path || !logs_path[0]) return;

    std::string plugin_log = std::string(logs_path) + "\\" + s_plugin_name + "_log.log";
    FILE* lf = fopen(plugin_log.c_str(), "r");
    if (!lf) return;

    fprintf(f, "\n--- Plugin Log (%s) ---\n", plugin_log.c_str());
    char buf[4096];
    while (fgets(buf, sizeof(buf), lf))
        fputs(buf, f);
    fclose(lf);
    fprintf(f, "--- End of Plugin Log ---\n");
}

static void do_write_crash_report(EXCEPTION_POINTERS* ep)
{
    std::string dir = make_crash_dir();
    std::string log_path = dir + "\\crash.log";
    std::string dmp_path = dir + "\\crash.dmp";

    DWORD code = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionCode : 0xE0000001;

    write_minidump(ep, dmp_path);

    FILE* f = fopen(log_path.c_str(), "w");
    if (!f) f = stderr;

    fprintf(f, "=== Millennium Plugin Crash Report ===\n\n");
    fprintf(f, "Millennium Version : %s\n", MILLENNIUM_VERSION);
    fprintf(f, "Git Commit         : %s\n", GIT_COMMIT_HASH);
    fprintf(f, "Plugin             : %s\n", s_plugin_name.c_str());
    fprintf(f, "Backend File       : %s\n", s_backend_file.c_str());
    fprintf(f, "Exception          : %s (0x%08lX)\n", exception_name(code), code);
    if (ep && ep->ExceptionRecord) fprintf(f, "Address            : %p\n", ep->ExceptionRecord->ExceptionAddress);
    fprintf(f, "PID                : %lu\n\n", GetCurrentProcessId());

    fprintf(f, "--- C Stack Trace ---\n");

    HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, TRUE);

    void* frames[128];
    WORD count = CaptureStackBackTrace(0, 128, frames, nullptr);

    char sym_buf[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(sym_buf);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;

    for (WORD i = 0; i < count; ++i) {
        DWORD64 addr = reinterpret_cast<DWORD64>(frames[i]);
        if (SymFromAddr(process, addr, nullptr, sym)) {
            fprintf(f, "  #%d  0x%llX  %s\n", i, (unsigned long long)addr, sym->Name);
        } else {
            fprintf(f, "  #%d  0x%llX  (unknown)\n", i, (unsigned long long)addr);
        }
    }

    SymCleanup(process);
    fprintf(f, "\n--- End of Crash Report ---\n");

    append_plugin_log(f);

    if (f != stderr) {
        fclose(f);
        fprintf(stderr, "\n[lua-host] CRASH: Plugin '%s' — %s\n", s_plugin_name.c_str(), exception_name(code));
        fprintf(stderr, "[lua-host] Crash dump: %s\n", dir.c_str());
    }
}

/* for EXCEPTION_STACK_OVERFLOW we have no usable stack on the faulting thread,
   so we write the report from a fresh thread. */
struct crash_thread_params
{
    EXCEPTION_POINTERS* ep;
    HANDLE done;
};

static DWORD WINAPI crash_writer_thread(LPVOID param)
{
    auto* p = static_cast<crash_thread_params*>(param);
    do_write_crash_report(p->ep);
    SetEvent(p->done);
    return 0;
}

static LONG WINAPI crash_exception_handler(EXCEPTION_POINTERS* ep)
{
    if (!ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;

    DWORD code = ep->ExceptionRecord->ExceptionCode;

    /* let the C++ runtime handle its own exceptions normally */
    if (!is_fatal_exception(code)) return EXCEPTION_CONTINUE_SEARCH;

    /* prevent re-entrancy (e.g. crash inside the crash handler) */
    if (s_crash_in_progress.exchange(true)) return EXCEPTION_CONTINUE_SEARCH;

    if (code == EXCEPTION_STACK_OVERFLOW) {
        /* faulting thread's stack is exhausted — write from a new thread */
        crash_thread_params params{ ep, CreateEvent(nullptr, TRUE, FALSE, nullptr) };
        HANDLE t = CreateThread(nullptr, 0, crash_writer_thread, &params, 0, nullptr);
        if (t) {
            WaitForSingleObject(params.done, 10000);
            CloseHandle(t);
        }
        if (params.done) CloseHandle(params.done);
    } else {
        do_write_crash_report(ep);
    }

    TerminateProcess(GetCurrentProcess(), code);
    return EXCEPTION_CONTINUE_SEARCH; /* unreachable */
}

static void on_terminate()
{
    /* std::terminate / abort() — no EXCEPTION_POINTERS available */
    if (s_crash_in_progress.exchange(true)) return;
    do_write_crash_report(nullptr);
    TerminateProcess(GetCurrentProcess(), 1);
}

void install_crash_handler(const char* plugin_name, const char* backend_file, const char* steam_path, const char* crash_dump_dir)
{
    s_plugin_name = plugin_name;
    s_backend_file = backend_file;
    s_steam_path = steam_path;
    s_crash_dump_dir = crash_dump_dir;

    /* AddVectoredExceptionHandler fires before SEH unwind and is not
       overridden by the MSVC CRT, unlike SetUnhandledExceptionFilter. */
    AddVectoredExceptionHandler(1, crash_exception_handler);

    /* catch abort() / std::terminate() paths that bypass SEH entirely */
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    std::set_terminate(on_terminate);
}

#endif
