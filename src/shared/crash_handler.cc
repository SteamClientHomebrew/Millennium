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

#include "shared/crash_handler.h"
#include "shared/crash_handler_core.h"
#include "shared/crash_report.h"
#ifdef _WIN32
#include "millennium/filesystem.h"
#endif

#include <csignal>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>

static std::string s_crash_dump_base;

static std::string compute_crash_dump_base()
{
#ifdef _WIN32
    try {
        return (platform::get_millennium_path() / "crashes").string();
    } catch (...) {
        return "crash_dumps";
    }
#else
    const char* xdg_state = std::getenv("XDG_STATE_HOME");
    if (xdg_state && xdg_state[0]) return std::string(xdg_state) + "/millennium/crash_dumps";

    const char* home = std::getenv("HOME");
    return std::string(home ? home : "/tmp") + "/.local/state/millennium/crash_dumps";
#endif
}

static std::string make_crash_dir()
{
    char timestamp[64];
    std::time_t now = std::time(nullptr);
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", std::localtime(&now));

    std::string dir = s_crash_dump_base + "/millennium-" + timestamp;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

#ifdef _WIN32

#include "shared/crash_ipc.h"
#include <windows.h>

static crash_handler_ctx s_ctx{};

static HANDLE s_shm_mapping = nullptr;
static HANDLE s_shutdown_event = nullptr;
static HANDLE s_watchdog_process = nullptr;

static const crash_report_info s_info = {
    "Millennium Crash Report", "millennium.dll", nullptr, nullptr, "[millennium]",
};

static bool millennium_watchdog_available()
{
    if (!s_watchdog_process) return false;
    return WaitForSingleObject(s_watchdog_process, 0) == WAIT_TIMEOUT;
}

static void millennium_fallback_write(EXCEPTION_POINTERS* ep)
{
    std::string dir = make_crash_dir();
    crash_report_write(s_info, dir, ep);
}

static void spawn_watchdog()
{
    uint32_t steam_pid = GetCurrentProcessId();

    char shm_name[128];
    crash_ipc_shm_name(shm_name, sizeof(shm_name), steam_pid);

    s_shm_mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(crash_ipc_region), shm_name);
    if (!s_shm_mapping) return;

    s_ctx.shm = static_cast<crash_ipc_region*>(MapViewOfFile(s_shm_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(crash_ipc_region)));
    if (!s_ctx.shm) {
        CloseHandle(s_shm_mapping);
        s_shm_mapping = nullptr;
        return;
    }

    memset(s_ctx.shm, 0, sizeof(crash_ipc_region));
    s_ctx.shm->magic = CRASH_IPC_MAGIC;
    s_ctx.shm->version = CRASH_IPC_VERSION;
    s_ctx.shm->steam_pid = steam_pid;
    strncpy(s_ctx.shm->crash_dump_base, s_crash_dump_base.c_str(), sizeof(s_ctx.shm->crash_dump_base) - 1);
    strncpy(s_ctx.shm->component_name, "millennium.dll", sizeof(s_ctx.shm->component_name) - 1);
    strncpy(s_ctx.shm->log_prefix, "[millennium]", sizeof(s_ctx.shm->log_prefix) - 1);
    strncpy(s_ctx.shm->report_title, "Millennium Crash Report", sizeof(s_ctx.shm->report_title) - 1);

    char req_name[128], done_name[128], stop_name[128];
    crash_ipc_request_event_name(req_name, sizeof(req_name), steam_pid);
    crash_ipc_done_event_name(done_name, sizeof(done_name), steam_pid);
    crash_ipc_shutdown_event_name(stop_name, sizeof(stop_name), steam_pid);

    s_ctx.request_event = CreateEventA(nullptr, FALSE, FALSE, req_name); /* auto-reset */
    s_ctx.done_event = CreateEventA(nullptr, FALSE, FALSE, done_name);   /* auto-reset */
    s_shutdown_event = CreateEventA(nullptr, TRUE, FALSE, stop_name);    /* manual-reset */

    if (!s_ctx.request_event || !s_ctx.done_event || !s_shutdown_event) return;

    HANDLE self_pseudo = GetCurrentProcess();
    HANDLE inheritable_handle = nullptr;
    if (!DuplicateHandle(self_pseudo, self_pseudo, self_pseudo, &inheritable_handle, PROCESS_ALL_ACCESS, TRUE, 0)) {
        return;
    }

    const std::string handler_exe = (platform::get_millennium_bin_path() / "millennium.crashhandler64.exe").string();
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\" --steam-pid=%u --steam-handle=%llX", handler_exe.c_str(), steam_pid, (unsigned long long)(uintptr_t)inheritable_handle);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(nullptr, cmd, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(inheritable_handle);
        return;
    }

    s_watchdog_process = pi.hProcess;
    CloseHandle(pi.hThread);
    CloseHandle(inheritable_handle);
}

void install_millennium_crash_handler()
{
    s_crash_dump_base = compute_crash_dump_base();
    spawn_watchdog();

    s_ctx.is_watchdog_available = millennium_watchdog_available;
    s_ctx.write_shm_fields = nullptr;
    s_ctx.fallback_write = millennium_fallback_write;
    s_ctx.terminate_after_crash = false;

    crash_handler_core_install(&s_ctx);
}

#else /* !_WIN32 */

static const crash_report_info s_info = {
    "Millennium Crash Report", "libmillennium", nullptr, nullptr, "[millennium]",
};

static crash_handler_signal_ctx s_signal_ctx{};

void install_millennium_crash_handler()
{
    s_crash_dump_base = compute_crash_dump_base();

    s_signal_ctx.info = &s_info;
    s_signal_ctx.make_crash_dir = make_crash_dir;
    s_signal_ctx.chain_to_previous = true;

    static const int signals[] = { SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL };
    crash_handler_core_install_signals(&s_signal_ctx, signals, 5);
}

#endif
