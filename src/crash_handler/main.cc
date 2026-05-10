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

#ifdef _WIN32

#include "shared/crash_ipc.h"
#include "shared/crash_report.h"

#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>

#pragma comment(lib, "dbghelp.lib")

static std::string make_crash_dir(const char* base)
{
    char timestamp[64];
    std::time_t now = std::time(nullptr);
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", std::localtime(&now));

    std::string dir = std::string(base) + "\\millennium-" + timestamp;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

static void write_minidump_external(HANDLE process, DWORD pid, DWORD tid, uint64_t ep_addr, const std::string& path)
{
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId = tid;
    mei.ExceptionPointers = reinterpret_cast<PEXCEPTION_POINTERS>(static_cast<uintptr_t>(ep_addr));
    mei.ClientPointers = TRUE;
    MiniDumpWriteDump(process, pid, hFile, MiniDumpWithDataSegs, ep_addr ? &mei : nullptr, nullptr, nullptr);
    CloseHandle(hFile);
}

static std::string make_or_use_crash_dir(const crash_ipc_region* shm)
{
    if (shm->crashing_pid && shm->crashing_pid != shm->steam_pid) {
        std::string dir(shm->crash_dump_base);
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
    }
    return make_crash_dir(shm->crash_dump_base);
}

static void handle_crash(HANDLE steam_process, const crash_ipc_region* shm)
{
    std::string dir = make_or_use_crash_dir(shm);
    std::string log_path = dir + "\\crash.log";
    std::string dmp_path = dir + "\\crash.dmp";

    uint32_t target_pid = shm->crashing_pid ? shm->crashing_pid : shm->steam_pid;
    HANDLE target_process = steam_process;
    bool owns_target_handle = false;

    if (target_pid != shm->steam_pid) {
        target_process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, target_pid);
        if (!target_process) {
            fprintf(stderr, "[crashhandler] failed to open child process %u (error %lu)\n", target_pid, GetLastError());
            target_process = steam_process;
            target_pid = shm->steam_pid;
        } else {
            owns_target_handle = true;
        }
    }

    write_minidump_external(target_process, target_pid, shm->faulting_tid, shm->exception_ptrs, dmp_path);

    crash_report_info info;
    info.title = shm->report_title;
    info.log_prefix = shm->log_prefix;

    if (shm->backend_file[0]) {
        info.plugin_name = shm->component_name;
        info.backend_file = shm->backend_file;
        info.component = nullptr;
    } else {
        info.component = shm->component_name;
        info.plugin_name = nullptr;
        info.backend_file = nullptr;
    }

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

    fprintf(f, "Exception          : %s (0x%08lX)\n", crash_report_exception_name(shm->exc_code), (unsigned long)shm->exc_code);
    fprintf(f, "Faulting Thread    : %lu\n", (unsigned long)shm->faulting_tid);
    fprintf(f, "PID                : %lu\n\n", (unsigned long)target_pid);
    fprintf(f, "\n--- End of Crash Report ---\n");

    if (f != stderr) {
        fclose(f);
        fprintf(stderr, "\n%s CRASH: %s (0x%08lX)\n", info.log_prefix, crash_report_exception_name(shm->exc_code), (unsigned long)shm->exc_code);
        fprintf(stderr, "%s Crash dump: %s\n", info.log_prefix, dir.c_str());
    }

    if (owns_target_handle) {
        CloseHandle(target_process);
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        MessageBoxA(nullptr,
                    "This is an internal Millennium runtime component.\n"
                    "It is not meant to be run directly.\n\n"
                    "Please launch Steam with Millennium instead.",
                    "millennium.crashhandler64.exe", MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    uint32_t steam_pid = 0;
    HANDLE steam_process = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--steam-pid=", 12) == 0) {
            steam_pid = static_cast<uint32_t>(strtoul(argv[i] + 12, nullptr, 10));
        } else if (strncmp(argv[i], "--steam-handle=", 15) == 0) {
            steam_process = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(strtoull(argv[i] + 15, nullptr, 16)));
        }
    }

    if (!steam_pid || !steam_process) {
        fprintf(stderr, "usage: millennium.crashhandler64.exe --steam-pid=<N> --steam-handle=<hex>\n");
        return 1;
    }

    char shm_name[128], req_name[128], done_name[128], stop_name[128];
    crash_ipc_shm_name(shm_name, sizeof(shm_name), steam_pid);
    crash_ipc_request_event_name(req_name, sizeof(req_name), steam_pid);
    crash_ipc_done_event_name(done_name, sizeof(done_name), steam_pid);
    crash_ipc_shutdown_event_name(stop_name, sizeof(stop_name), steam_pid);

    HANDLE hMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, shm_name);
    if (!hMapping) {
        fprintf(stderr, "[crashhandler] failed to open shared memory '%s' (error %lu)\n", shm_name, GetLastError());
        return 1;
    }

    const crash_ipc_region* shm = static_cast<const crash_ipc_region*>(MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, sizeof(crash_ipc_region)));
    if (!shm) {
        fprintf(stderr, "[crashhandler] failed to map shared memory (error %lu)\n", GetLastError());
        CloseHandle(hMapping);
        return 1;
    }

    if (shm->magic != CRASH_IPC_MAGIC || shm->version != CRASH_IPC_VERSION) {
        fprintf(stderr, "[crashhandler] shared memory magic/version mismatch\n");
        UnmapViewOfFile(shm);
        CloseHandle(hMapping);
        return 1;
    }

    HANDLE hRequest = OpenEventA(SYNCHRONIZE, FALSE, req_name);
    HANDLE hDone = OpenEventA(EVENT_MODIFY_STATE, FALSE, done_name);
    HANDLE hShutdown = OpenEventA(SYNCHRONIZE, FALSE, stop_name);

    if (!hRequest || !hDone || !hShutdown) {
        fprintf(stderr, "[crashhandler] failed to open IPC events (error %lu)\n", GetLastError());
        UnmapViewOfFile(shm);
        CloseHandle(hMapping);
        return 1;
    }

    fprintf(stderr, "[crashhandler] watchdog started for Steam PID %u\n", steam_pid);

    HANDLE wait_handles[] = { hRequest, steam_process, hShutdown };
    bool running = true;

    while (running) {
        DWORD result = WaitForMultipleObjects(3, wait_handles, FALSE, INFINITE);

        switch (result) {
            case WAIT_OBJECT_0:
                handle_crash(steam_process, shm);
                SetEvent(hDone);
                break;
            case WAIT_OBJECT_0 + 1:
                fprintf(stderr, "[crashhandler] Steam process exited, shutting down\n");
                running = false;
                break;
            case WAIT_OBJECT_0 + 2:
                fprintf(stderr, "[crashhandler] shutdown requested\n");
                running = false;
                break;
            default:
                running = false;
                break;
        }
    }

    UnmapViewOfFile(shm);
    CloseHandle(hMapping);
    CloseHandle(hRequest);
    CloseHandle(hDone);
    CloseHandle(hShutdown);
    CloseHandle(steam_process);
    return 0;
}
#else
int main()
{
    return 0;
}
#endif
