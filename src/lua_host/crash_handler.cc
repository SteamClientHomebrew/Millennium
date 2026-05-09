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
#include "crash_handler.h"
#include "shared/crash_handler_core.h"
#include "shared/crash_report.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

static std::string s_plugin_name;
static std::string s_backend_file;
static std::string s_crash_dump_dir;

static crash_report_info s_info;

static std::string make_crash_dir()
{
    std::error_code ec;
    std::filesystem::create_directories(s_crash_dump_dir, ec);
    return s_crash_dump_dir;
}

#ifndef _WIN32

#include <sys/resource.h>
#include <signal.h>

static crash_handler_signal_ctx s_signal_ctx{};

void install_crash_handler(const char* plugin_name, const char* backend_file, const char* steam_path, const char* crash_dump_dir, unsigned int steam_pid)
{
    (void)steam_pid;
    (void)steam_path;

    s_plugin_name = plugin_name;
    s_backend_file = backend_file;
    s_crash_dump_dir = crash_dump_dir;

    s_info.title = "Millennium Plugin Crash Report";
    s_info.component = nullptr;
    s_info.plugin_name = s_plugin_name.c_str();
    s_info.backend_file = s_backend_file.c_str();
    s_info.log_prefix = "[lua-host]";

    /* enable core dumps */
    struct rlimit rl;
    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &rl);

    s_signal_ctx.info = &s_info;
    s_signal_ctx.make_crash_dir = make_crash_dir;
    s_signal_ctx.chain_to_previous = false;

    static const int signals[] = { SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSYS };
    crash_handler_core_install_signals(&s_signal_ctx, signals, 6);
}

#else /* _WIN32 */

#include "shared/crash_ipc.h"
#include <windows.h>

static crash_handler_ctx s_ctx{};

static HANDLE s_shm_mapping = nullptr;

/* .bss buffers -- safe to read even if heap is trashed */
static char s_buf_crash_dump_dir[260];
static char s_buf_plugin_name[128];
static char s_buf_backend_file[260];

static bool plugin_watchdog_available()
{
    return s_ctx.shm != nullptr && s_ctx.request_event != nullptr && s_ctx.done_event != nullptr;
}

static void plugin_fallback_write(EXCEPTION_POINTERS* ep)
{
    std::string dir(s_buf_crash_dump_dir);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    crash_report_write(s_info, dir, ep);
}

static void plugin_write_shm_fields(crash_ipc_region* shm)
{
    memcpy(shm->crash_dump_base, s_buf_crash_dump_dir, sizeof(shm->crash_dump_base));
    memcpy(shm->component_name, s_buf_plugin_name, sizeof(shm->component_name));
    memcpy(shm->backend_file, s_buf_backend_file, sizeof(shm->backend_file));

    memset(shm->log_prefix, 0, sizeof(shm->log_prefix));
    memcpy(shm->log_prefix, "[lua-host]", 11);

    memset(shm->report_title, 0, sizeof(shm->report_title));
    memcpy(shm->report_title, "Millennium Plugin Crash Report", 31);
}

static BOOL WINAPI console_ctrl_handler(DWORD event)
{
    switch (event) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            return TRUE; /* parent manages our lifetime */
        default:
            return FALSE;
    }
}

static void connect_to_watchdog(uint32_t steam_pid)
{
    char shm_name[128], req_name[128], done_name[128];
    crash_ipc_shm_name(shm_name, sizeof(shm_name), steam_pid);
    crash_ipc_request_event_name(req_name, sizeof(req_name), steam_pid);
    crash_ipc_done_event_name(done_name, sizeof(done_name), steam_pid);

    s_shm_mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, shm_name);
    if (!s_shm_mapping) return;

    s_ctx.shm = static_cast<crash_ipc_region*>(MapViewOfFile(s_shm_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(crash_ipc_region)));
    if (!s_ctx.shm) {
        CloseHandle(s_shm_mapping);
        s_shm_mapping = nullptr;
        return;
    }

    if (s_ctx.shm->magic != CRASH_IPC_MAGIC || s_ctx.shm->version != CRASH_IPC_VERSION) {
        UnmapViewOfFile(s_ctx.shm);
        s_ctx.shm = nullptr;
        CloseHandle(s_shm_mapping);
        s_shm_mapping = nullptr;
        return;
    }

    s_ctx.request_event = OpenEventA(EVENT_MODIFY_STATE, FALSE, req_name);
    s_ctx.done_event = OpenEventA(SYNCHRONIZE, FALSE, done_name);

    if (!s_ctx.request_event || !s_ctx.done_event) {
        if (s_ctx.request_event) {
            CloseHandle(s_ctx.request_event);
            s_ctx.request_event = nullptr;
        }
        if (s_ctx.done_event) {
            CloseHandle(s_ctx.done_event);
            s_ctx.done_event = nullptr;
        }
        UnmapViewOfFile(s_ctx.shm);
        s_ctx.shm = nullptr;
        CloseHandle(s_shm_mapping);
        s_shm_mapping = nullptr;
    }
}

void install_crash_handler(const char* plugin_name, const char* backend_file, const char* steam_path, const char* crash_dump_dir, unsigned int steam_pid)
{
    (void)steam_path;

    s_plugin_name = plugin_name;
    s_backend_file = backend_file;
    s_crash_dump_dir = crash_dump_dir;

    memset(s_buf_crash_dump_dir, 0, sizeof(s_buf_crash_dump_dir));
    strncpy(s_buf_crash_dump_dir, crash_dump_dir, sizeof(s_buf_crash_dump_dir) - 1);

    memset(s_buf_plugin_name, 0, sizeof(s_buf_plugin_name));
    strncpy(s_buf_plugin_name, plugin_name, sizeof(s_buf_plugin_name) - 1);

    memset(s_buf_backend_file, 0, sizeof(s_buf_backend_file));
    strncpy(s_buf_backend_file, backend_file, sizeof(s_buf_backend_file) - 1);

    s_info.title = "Millennium Plugin Crash Report";
    s_info.component = nullptr;
    s_info.plugin_name = s_buf_plugin_name;
    s_info.backend_file = s_buf_backend_file;
    s_info.log_prefix = "[lua-host]";

    connect_to_watchdog(steam_pid);
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    s_ctx.is_watchdog_available = plugin_watchdog_available;
    s_ctx.write_shm_fields = plugin_write_shm_fields;
    s_ctx.fallback_write = plugin_fallback_write;
    s_ctx.terminate_after_crash = true;

    crash_handler_core_install(&s_ctx);
}

#endif
