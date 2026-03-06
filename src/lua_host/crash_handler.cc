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

#include <cstdio>
#include <cstdlib>
#include <ctime>
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

static std::string get_crash_base_dir()
{
#ifdef _WIN32
    if (!s_steam_path.empty()) return s_steam_path + "\\ext\\crash_dumps";
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) appdata = "C:\\";
    return std::string(appdata) + "\\millennium\\crash_dumps";
#else
    const char* xdg_state = std::getenv("XDG_STATE_HOME");
    if (xdg_state && xdg_state[0]) return std::string(xdg_state) + "/millennium/crash_dumps";
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.local/state/millennium/crash_dumps";
#endif
}

static std::string make_crash_dir()
{
    char timestamp[64];
    time_t now = time(nullptr);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", localtime(&now));

    std::string dir = get_crash_base_dir()
#ifdef _WIN32
                      + "\\"
#else
                      + "/"
#endif
                      + s_plugin_name + "-" + timestamp;

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
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

void install_crash_handler(const char* plugin_name, const char* backend_file, const char* steam_path)
{
    s_plugin_name = plugin_name;
    s_backend_file = backend_file;
    s_steam_path = steam_path;

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
    sigaction(SIGBUS,  &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);
    sigaction(SIGSYS,  &sa, nullptr);
}

#else

#include <windows.h>
#include <dbghelp.h>

#pragma comment(lib, "dbghelp.lib")

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
        case EXCEPTION_BREAKPOINT:
            return "EXCEPTION_BREAKPOINT";
        default:
            return "Unknown exception";
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

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpWithDataSegs, &mei, nullptr, nullptr);
    CloseHandle(hFile);
}

static LONG WINAPI crash_exception_handler(EXCEPTION_POINTERS* ep)
{
    std::string dir = make_crash_dir();
    std::string log_path = dir + "\\crash.log";
    std::string dmp_path = dir + "\\crash.dmp";

    DWORD code = ep->ExceptionRecord->ExceptionCode;

    /* write minidump first — most valuable artifact */
    write_minidump(ep, dmp_path);

    FILE* f = fopen(log_path.c_str(), "w");
    if (!f) f = stderr;

    fprintf(f, "=== Millennium Plugin Crash Report ===\n\n");
    fprintf(f, "Millennium Version : %s\n", MILLENNIUM_VERSION);
    fprintf(f, "Git Commit         : %s\n", GIT_COMMIT_HASH);
    fprintf(f, "Plugin             : %s\n", s_plugin_name.c_str());
    fprintf(f, "Backend File       : %s\n", s_backend_file.c_str());
    fprintf(f, "Exception          : %s (0x%08lX)\n", exception_name(code), code);
    fprintf(f, "Address            : 0x%p\n", ep->ExceptionRecord->ExceptionAddress);
    fprintf(f, "PID                : %lu\n\n", GetCurrentProcessId());

    /* C stack trace via DbgHelp */
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

    fprintf(f, "\nMinidump: crash.dmp (in same directory)\n");
    fprintf(f, "Open the .dmp file in Visual Studio for full debugging.\n");
    fprintf(f, "\n--- End of Crash Report ---\n");

    if (f != stderr) {
        fclose(f);
        fprintf(stderr, "\n[lua-host] CRASH: Plugin '%s' — %s\n", s_plugin_name.c_str(), exception_name(code));
        fprintf(stderr, "[lua-host] Crash dump: %s\n", dir.c_str());
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

void install_crash_handler(const char* plugin_name, const char* backend_file, const char* steam_path)
{
    s_plugin_name = plugin_name;
    s_backend_file = backend_file;
    s_steam_path = steam_path;

    SetUnhandledExceptionFilter(crash_exception_handler);
}

#endif
