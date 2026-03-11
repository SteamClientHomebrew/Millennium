/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "shared/crash_report.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#ifndef MILLENNIUM_VERSION
#define MILLENNIUM_VERSION "unknown"
#endif
#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "unknown"
#endif

#ifdef _WIN32
#pragma comment(lib, "dbghelp.lib")

const char* crash_report_exception_name(DWORD code)
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

bool crash_report_is_fatal(DWORD code)
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
        case 0xE0000001: /* custom terminate code */
            return true;
        default:
            return false;
    }
}

void crash_report_write_minidump(EXCEPTION_POINTERS* ep, const std::string& path)
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

static void write_stack_trace_win32(FILE* f)
{
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
}

void crash_report_write(const crash_report_info& info, const std::string& dir, EXCEPTION_POINTERS* ep)
{
    std::string log_path = dir + "\\crash.log";
    std::string dmp_path = dir + "\\crash.dmp";

    DWORD code = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionCode : 0xE0000001;

    crash_report_write_minidump(ep, dmp_path);

    FILE* f = fopen(log_path.c_str(), "w");
    if (!f) f = stderr;

    fprintf(f, "=== %s ===\n\n", info.title);
    fprintf(f, "Millennium Version : %s\n", MILLENNIUM_VERSION);
    fprintf(f, "Git Commit         : %s\n", GIT_COMMIT_HASH);

    if (info.plugin_name) {
        fprintf(f, "Plugin             : %s\n", info.plugin_name);
        if (info.backend_file) fprintf(f, "Backend File       : %s\n", info.backend_file);
    } else if (info.component) {
        fprintf(f, "Component          : %s\n", info.component);
    }

    fprintf(f, "Exception          : %s (0x%08lX)\n", crash_report_exception_name(code), code);
    if (ep && ep->ExceptionRecord) fprintf(f, "Address            : %p\n", ep->ExceptionRecord->ExceptionAddress);
    fprintf(f, "PID                : %lu\n\n", GetCurrentProcessId());

    fprintf(f, "--- C Stack Trace ---\n");
    write_stack_trace_win32(f);
    fprintf(f, "\n--- End of Crash Report ---\n");

    if (f != stderr) {
        fclose(f);
        fprintf(stderr, "\n%s CRASH: %s (0x%08lX)\n", info.log_prefix, crash_report_exception_name(code), code);
        fprintf(stderr, "%s Crash dump: %s\n", info.log_prefix, dir.c_str());
    }
}

struct crash_dispatch_params
{
    EXCEPTION_POINTERS* ep;
    void (*writer)(EXCEPTION_POINTERS*);
    HANDLE done;
};

static DWORD WINAPI crash_dispatch_thread(LPVOID param)
{
    auto* p = static_cast<crash_dispatch_params*>(param);
    p->writer(p->ep);
    SetEvent(p->done);
    return 0;
}

void crash_report_dispatch(EXCEPTION_POINTERS* ep, void (*writer)(EXCEPTION_POINTERS*))
{
    DWORD code = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionCode : 0;

    if (code == EXCEPTION_STACK_OVERFLOW) {
        crash_dispatch_params params{ ep, writer, CreateEvent(nullptr, TRUE, FALSE, nullptr) };
        HANDLE t = CreateThread(nullptr, 0, crash_dispatch_thread, &params, 0, nullptr);
        if (t) {
            WaitForSingleObject(params.done, 10000);
            CloseHandle(t);
        }
        if (params.done) CloseHandle(params.done);
    } else {
        writer(ep);
    }
}
#else
#include <execinfo.h>
#include <unistd.h>

const char* crash_report_signal_name(int sig)
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
#ifdef SIGSYS
        case SIGSYS:
            return "SIGSYS (Bad system call)";
#endif
        default:
            return "Unknown signal";
    }
}

static void write_stack_trace_posix(FILE* f)
{
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
}

void crash_report_write(const crash_report_info& info, const std::string& dir, int sig)
{
    std::string log_path = dir + "/crash.log";

    FILE* f = fopen(log_path.c_str(), "w");
    if (!f) f = stderr;

    fprintf(f, "=== %s ===\n\n", info.title);
    fprintf(f, "Millennium Version : %s\n", MILLENNIUM_VERSION);
    fprintf(f, "Git Commit         : %s\n", GIT_COMMIT_HASH);

    if (info.plugin_name) {
        fprintf(f, "Plugin             : %s\n", info.plugin_name);
        if (info.backend_file) fprintf(f, "Backend File       : %s\n", info.backend_file);
    } else if (info.component) {
        fprintf(f, "Component          : %s\n", info.component);
    }

    fprintf(f, "Signal             : %s (%d)\n", crash_report_signal_name(sig), sig);
    fprintf(f, "PID                : %d\n\n", getpid());

    fprintf(f, "--- C Stack Trace ---\n");
    write_stack_trace_posix(f);
    fprintf(f, "\n--- End of Crash Report ---\n");

    if (f != stderr) {
        fclose(f);
        fprintf(stderr, "\n%s CRASH: %s\n", info.log_prefix, crash_report_signal_name(sig));
        fprintf(stderr, "%s Crash report: %s\n", info.log_prefix, log_path.c_str());
    }
}
#endif
