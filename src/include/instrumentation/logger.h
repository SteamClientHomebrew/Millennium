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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

static inline FILE* hhx64_log_file()
{
    static FILE* f = NULL;
    if (!f) {
#ifdef _WIN32
        char* logs_dir = NULL;
        size_t len = 0;
        if (_dupenv_s(&logs_dir, &len, "MILLENNIUM__LOGS_PATH") == 0 && logs_dir) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/hhx64.log", logs_dir);
            fopen_s(&f, path, "w");
            free(logs_dir);
        }
#else
        const char* logs_dir = getenv("MILLENNIUM__LOGS_PATH");
        if (logs_dir) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/hhx64.log", logs_dir);
            f = fopen(path, "w");
        }
#endif
        if (!f) f = stderr;
    }
    return f;
}

static void get_time_mmss(char* buf, size_t len)
{
#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buf, len, "%02d:%02d.%03d", st.wMinute, st.wSecond, st.wMilliseconds);
#else
    struct timeval tv;
    struct tm* tm;
    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);
    snprintf(buf, len, "%02d:%02d.%03d", tm->tm_min, tm->tm_sec, static_cast<int>(tv.tv_usec / 1000));
#endif
}

static inline void log_debug(const char* fmt, ...)
{
    char timebuf[16];
    get_time_mmss(timebuf, sizeof(timebuf));
    va_list args;
    va_start(args, fmt);
    FILE* f = hhx64_log_file();
    fprintf(f, "[%s] HHX64-DEBUG ", timebuf);
    vfprintf(f, fmt, args);
    fflush(f);
    va_end(args);
}

static inline void log_info(const char* fmt, ...)
{
    char timebuf[16];
    get_time_mmss(timebuf, sizeof(timebuf));
    va_list args;
    va_start(args, fmt);
    FILE* f = hhx64_log_file();
    fprintf(f, "[%s] HHX64-INFO ", timebuf);
    vfprintf(f, fmt, args);
    fflush(f);
    va_end(args);
}

static inline void log_warning(const char* fmt, ...)
{
    char timebuf[16];
    get_time_mmss(timebuf, sizeof(timebuf));
    va_list args;
    va_start(args, fmt);
    FILE* f = hhx64_log_file();
    fprintf(f, "[%s] HHX64-WARN ", timebuf);
    vfprintf(f, fmt, args);
    fflush(f);
    va_end(args);
}

static inline void log_error(const char* fmt, ...)
{
    char timebuf[16];
    get_time_mmss(timebuf, sizeof(timebuf));
    va_list args;
    va_start(args, fmt);
    FILE* f = hhx64_log_file();
    fprintf(f, "[%s] HHX64-ERROR ", timebuf);
    vfprintf(f, fmt, args);
    fflush(f);
    va_end(args);
}
