/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|_|___|_|_|_|
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

#pragma once
#ifdef _WIN32

#include <cstdint>
#include <cstdio>

#define CRASH_IPC_MAGIC 0x4D4C4E43u /* "MLNC" */
#define CRASH_IPC_VERSION 2

/* written by VEH handler (signal-safe), read by watchdog */
struct crash_ipc_region
{
    uint32_t magic;
    uint32_t version;
    uint32_t steam_pid;

    volatile long crash_lock;
    uint32_t crashing_pid;
    uint32_t faulting_tid;
    uint64_t exception_ptrs;
    uint32_t exc_code;
    char crash_dump_base[260];
    char component_name[128];
    char backend_file[260];
    char log_prefix[32];
    char report_title[128];
};

inline void crash_ipc_shm_name(char* buf, size_t len, uint32_t pid)
{
    snprintf(buf, len, "Local\\MillenniumCrashShm_%u", pid);
}

inline void crash_ipc_request_event_name(char* buf, size_t len, uint32_t pid)
{
    snprintf(buf, len, "Local\\MillenniumCrashReq_%u", pid);
}

inline void crash_ipc_done_event_name(char* buf, size_t len, uint32_t pid)
{
    snprintf(buf, len, "Local\\MillenniumCrashDone_%u", pid);
}

inline void crash_ipc_shutdown_event_name(char* buf, size_t len, uint32_t pid)
{
    snprintf(buf, len, "Local\\MillenniumCrashStop_%u", pid);
}
#endif /* _WIN32 */
