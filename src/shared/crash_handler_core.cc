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

#include "shared/crash_handler_core.h"

#ifdef _WIN32

#include <atomic>
#include <exception>

static crash_handler_ctx* s_ctx = nullptr;
static std::atomic<bool> s_crash_in_progress{ false };
static std::terminate_handler s_prev_terminate = nullptr;

/* signal-safe: no malloc, no stdio, no C++ runtime */
static void do_signal_watchdog(EXCEPTION_POINTERS* ep)
{
    DWORD code = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionCode : 0xE0000001;

    /* spin for the crash lock, 20s timeout */
    {
        const DWORD deadline = GetTickCount() + 20000;
        while (InterlockedCompareExchange(&s_ctx->shm->crash_lock, 1, 0) != 0) {
            if (GetTickCount() >= deadline) {
                s_ctx->fallback_write(ep);
                return;
            }
            Sleep(50);
        }
    }

    s_ctx->shm->crashing_pid = GetCurrentProcessId();
    s_ctx->shm->faulting_tid = GetCurrentThreadId();
    s_ctx->shm->exception_ptrs = reinterpret_cast<uint64_t>(ep);
    s_ctx->shm->exc_code = code;

    if (s_ctx->write_shm_fields) {
        s_ctx->write_shm_fields(s_ctx->shm);
    }

    SetEvent(s_ctx->request_event);
    WaitForSingleObject(s_ctx->done_event, 15000);
    InterlockedExchange(&s_ctx->shm->crash_lock, 0);
}

static LONG WINAPI crash_exception_handler(EXCEPTION_POINTERS* ep)
{
    if (!ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (!crash_report_is_fatal(code)) return EXCEPTION_CONTINUE_SEARCH;
    if (s_crash_in_progress.exchange(true)) return EXCEPTION_CONTINUE_SEARCH;

    if (s_ctx->is_watchdog_available()) {
        if (code == EXCEPTION_STACK_OVERFLOW) {
            HANDLE t = CreateThread(nullptr, 0, [](LPVOID param) -> DWORD
            {
                do_signal_watchdog(static_cast<EXCEPTION_POINTERS*>(param));
                return 0;
            }, ep, 0, nullptr);
            if (t) {
                WaitForSingleObject(t, 20000);
                CloseHandle(t);
            }
        } else {
            do_signal_watchdog(ep);
        }
    } else {
        crash_report_dispatch(ep, s_ctx->fallback_write);
    }

    if (s_ctx->terminate_after_crash) {
        TerminateProcess(GetCurrentProcess(), code);
    }

    s_crash_in_progress.store(false);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void on_terminate()
{
    if (!s_crash_in_progress.exchange(true)) {
        if (s_ctx->is_watchdog_available()) {
            do_signal_watchdog(nullptr);
        } else {
            s_ctx->fallback_write(nullptr);
        }
    }

    if (s_ctx->terminate_after_crash) {
        TerminateProcess(GetCurrentProcess(), 1);
    }

    if (s_prev_terminate) s_prev_terminate();
    std::abort();
}

void crash_handler_core_install(crash_handler_ctx* ctx)
{
    s_ctx = ctx;
    AddVectoredExceptionHandler(1, crash_exception_handler);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    s_prev_terminate = std::set_terminate(on_terminate);
}

#else /* !_WIN32 */

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static crash_handler_signal_ctx* s_signal_ctx = nullptr;
static struct sigaction s_prev_handlers[32];

static void crash_signal_handler(int sig)
{
    pid_t pid = fork();

    if (pid == 0) {
        std::string dir = s_signal_ctx->make_crash_dir();
        crash_report_write(*s_signal_ctx->info, dir, sig);
        _exit(0);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }

    if (s_signal_ctx->chain_to_previous) {
        const struct sigaction* prev = &s_prev_handlers[sig];
        if (prev->sa_handler == SIG_DFL) {
            signal(sig, SIG_DFL);
            raise(sig);
        } else if (prev->sa_handler != SIG_IGN) {
            prev->sa_handler(sig);
        }
    } else {
        signal(sig, SIG_DFL);
        raise(sig);
    }
}

void crash_handler_core_install_signals(crash_handler_signal_ctx* ctx, const int* signals, int num_signals)
{
    s_signal_ctx = ctx;

    struct sigaction sa{};
    sa.sa_handler = crash_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;

    for (int i = 0; i < num_signals; ++i) {
        sigaction(signals[i], &sa, ctx->chain_to_previous ? &s_prev_handlers[signals[i]] : nullptr);
    }
}

#endif
