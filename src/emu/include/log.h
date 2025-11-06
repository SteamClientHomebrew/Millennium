#pragma once
#include <fcntl.h>
#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#define BOLD "\033[1m"
#define COL_RED "\033[31m"
#define COL_GREEN "\033[32m"
#define COL_YELLOW "\033[33m"
#define COL_BLUE "\033[34m"
#define COL_MAGENTA "\033[35m"
#define COL_CYAN "\033[36m"
#define COL_WHITE "\033[37m"
#define COL_RESET "\033[0m"

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
    fprintf(stderr, "[%s] %s%sMWHLBH-DEBUG%s ", timebuf, BOLD, COL_CYAN, COL_RESET);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

static inline void log_info(const char* fmt, ...)
{
    char timebuf[16];
    get_time_mmss(timebuf, sizeof(timebuf));

    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[%s] %s%sMWHLBH-INFO%s ", timebuf, BOLD, COL_CYAN, COL_RESET);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

static inline void log_warning(const char* fmt, ...)
{
    char timebuf[16];
    get_time_mmss(timebuf, sizeof(timebuf));

    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[%s] %s%sMWHLBH-WARN%s ", timebuf, BOLD, COL_YELLOW, COL_RESET);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

static inline void log_error(const char* fmt, ...)
{
    char timebuf[16];
    get_time_mmss(timebuf, sizeof(timebuf));

    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[%s] %s%sMWHLBH-ERROR%s ", timebuf, BOLD, COL_RED, COL_RESET);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
