/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
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
#include <stdarg.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

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
    snprintf(buf, len, "%02d:%02d.%03ld", tm->tm_min, tm->tm_sec, tv.tv_usec / 1000);
#endif
}

static inline void log_debug(const char* fmt, ...)
{
    char timebuf[16];
    get_time_mmss(timebuf, sizeof(timebuf));
    va_list args;
    va_start(args, fmt);
#ifdef _WIN32
    char buf[1024];
    snprintf(buf, sizeof(buf), "[%s] \033[1m\033[36mMWHLBH-DEBUG\033[0m ", timebuf);
    OutputDebugStringA(buf);
    vsnprintf(buf, sizeof(buf), fmt, args);
    OutputDebugStringA(buf);
#else
    fprintf(stderr, "[%s] \033[1m\033[36mMWHLBH-DEBUG\033[0m ", timebuf);
    vfprintf(stderr, fmt, args);
#endif
    va_end(args);
}

static inline void log_info(const char* fmt, ...)
{
    char timebuf[16];
    get_time_mmss(timebuf, sizeof(timebuf));
    va_list args;
    va_start(args, fmt);
#ifdef _WIN32
    char buf[1024];
    snprintf(buf, sizeof(buf), "[%s] \033[1m\033[36mMWHLBH-INFO\033[0m ", timebuf);
    OutputDebugStringA(buf);
    vsnprintf(buf, sizeof(buf), fmt, args);
    OutputDebugStringA(buf);
#else
    fprintf(stderr, "[%s] \033[1m\033[36mMWHLBH-INFO\033[0m ", timebuf);
    vfprintf(stderr, fmt, args);
#endif
    va_end(args);
}

static inline void log_warning(const char* fmt, ...)
{
    char timebuf[16];
    get_time_mmss(timebuf, sizeof(timebuf));
    va_list args;
    va_start(args, fmt);
#ifdef _WIN32
    char buf[1024];
    snprintf(buf, sizeof(buf), "[%s] \033[1m\033[33mMWHLBH-WARN\033[0m ", timebuf);
    OutputDebugStringA(buf);
    vsnprintf(buf, sizeof(buf), fmt, args);
    OutputDebugStringA(buf);
#else
    fprintf(stderr, "[%s] \033[1m\033[33mMWHLBH-WARN\033[0m ", timebuf);
    vfprintf(stderr, fmt, args);
#endif
    va_end(args);
}

static inline void log_error(const char* fmt, ...)
{
    char timebuf[16];
    get_time_mmss(timebuf, sizeof(timebuf));
    va_list args;
    va_start(args, fmt);
#ifdef _WIN32
    char buf[1024];
    snprintf(buf, sizeof(buf), "[%s] \033[1m\033[31mMWHLBH-ERROR\033[0m  ", timebuf);
    OutputDebugStringA(buf);
    vsnprintf(buf, sizeof(buf), fmt, args);
    OutputDebugStringA(buf);
#else
    fprintf(stderr, "[%s] \033[1m\033[31mMWHLBH-ERROR\033[0m ", timebuf);
    vfprintf(stderr, fmt, args);
#endif
    va_end(args);
}