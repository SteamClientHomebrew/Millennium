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

#pragma once

#include "shared/crash_report.h"

#ifdef _WIN32

#include "shared/crash_ipc.h"
#include <windows.h>

struct crash_handler_ctx
{
    crash_ipc_region* shm;
    HANDLE request_event;
    HANDLE done_event;

    bool (*is_watchdog_available)();
    void (*write_shm_fields)(crash_ipc_region* shm); /* extra SHM fields, null = none */
    void (*fallback_write)(EXCEPTION_POINTERS* ep);  /* in-process fallback */
    bool terminate_after_crash;                      /* TerminateProcess vs EXCEPTION_CONTINUE_SEARCH */
};

/* ctx must outlive the process (static global) */
void crash_handler_core_install(crash_handler_ctx* ctx);

#else /* !_WIN32 */

#include <string>

struct crash_handler_signal_ctx
{
    const crash_report_info* info;
    std::string (*make_crash_dir)();
    bool chain_to_previous;
};

void crash_handler_core_install_signals(crash_handler_signal_ctx* ctx, const int* signals, int num_signals);

#endif
