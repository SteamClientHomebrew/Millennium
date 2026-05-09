#ifndef SHARED_H
#define SHARED_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>

#define COLOR_RESET "\033[0m"
#define COLOR_INFO "\033[1;34m"
#define COLOR_ERROR "\033[1;31m"
#define COLOR_WARN "\033[1;33m"

#define GET_TIMESTAMP()                                                                                                                                                            \
    ({                                                                                                                                                                             \
        struct timeval tv;                                                                                                                                                         \
        struct tm* tm_info;                                                                                                                                                        \
        gettimeofday(&tv, NULL);                                                                                                                                                   \
        tm_info = localtime(&tv.tv_sec);                                                                                                                                           \
        static char timestamp_buf[16];                                                                                                                                             \
        snprintf(timestamp_buf, sizeof(timestamp_buf), "[%02d:%02d.%03d]", tm_info->tm_min, tm_info->tm_sec, (int)(tv.tv_usec / 10000));                                           \
        timestamp_buf;                                                                                                                                                             \
    })

#define LOG_INFO(fmt, ...) fprintf(stdout, "%s " COLOR_INFO "BOOTSTRAP-INFO " COLOR_RESET fmt "\n", GET_TIMESTAMP(), ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "%s " COLOR_ERROR "BOOTSTRAP-ERROR " COLOR_RESET fmt "\n", GET_TIMESTAMP(), ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) fprintf(stderr, "%s " COLOR_WARN "BOOTSTRAP-WARN " COLOR_RESET fmt "\n", GET_TIMESTAMP(), ##__VA_ARGS__)

#define HOOK_FUNC(handle, name, ret_type, params, args)                                                                                                                            \
    __attribute__((visibility("default"))) ret_type name params                                                                                                                    \
    {                                                                                                                                                                              \
        static ret_type(*orig) params = NULL;                                                                                                                                      \
        if (!orig) {                                                                                                                                                               \
            if (!(handle)) return (ret_type)0;                                                                                                                                     \
            orig = (ret_type(*) params)dlsym((handle), #name);                                                                                                                     \
            if (!orig) {                                                                                                                                                           \
                LOG_ERROR("Cannot find original %s", #name);                                                                                                                       \
                return (ret_type)0;                                                                                                                                                \
            }                                                                                                                                                                      \
        }                                                                                                                                                                          \
        return orig args;                                                                                                                                                          \
    }

static inline void* dlopen_or_log(const char* path, int flags)
{
    void* handle = dlopen(path, flags);
    if (!handle) LOG_ERROR("Failed to dlopen %s: %s", path, dlerror());
    return handle;
}

static inline void* dlsym_or_log(void* handle, const char* symbol)
{
    void* fn = dlsym(handle, symbol);
    if (!fn) LOG_ERROR("Failed to locate symbol %s: %s", symbol, dlerror());
    return fn;
}

static inline void get_steam_lib_path(char* buf, const char* parent, const char* arch, const char* lib)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(buf, PATH_MAX, "%s/steam-runtime/usr/lib/%s/%s", parent, arch, lib);
#pragma GCC diagnostic pop
}

/** Returns $HOME/.steam/steam/ubuntu12_32 — the fixed base of the Steam installation. */
static inline const char* get_steam_ubuntu12_path(void)
{
    static char path[PATH_MAX];
    const char* home = getenv("HOME");
    if (!home) return NULL;
    snprintf(path, PATH_MAX, "%s/.steam/steam/ubuntu12_32", home);
    return path;
}

static inline char* get_process_path(void)
{
    static char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        return path;
    }
    return NULL;
}

static inline char* get_process_path_parent(void)
{
    static char parentPath[PATH_MAX];
    char* exePath = get_process_path();
    if (!exePath) return NULL;

    strncpy(parentPath, exePath, PATH_MAX - 1);
    parentPath[PATH_MAX - 1] = '\0';
    char* lastSlash = strrchr(parentPath, '/');

    if (lastSlash) *lastSlash = '\0';
    return parentPath;
}

static void* h_xtst = NULL;
HOOK_FUNC(h_xtst, XTestFakeButtonEvent, int, (Display * a, unsigned int b, Bool c, unsigned long d), (a, b, c, d))
HOOK_FUNC(h_xtst, XTestFakeKeyEvent, int, (Display * a, unsigned int b, Bool c, unsigned long d), (a, b, c, d))
HOOK_FUNC(h_xtst, XTestQueryExtension, int, (Display * a, int* b, int* c, int* d, int* e), (a, b, c, d, e))
HOOK_FUNC(h_xtst, XTestFakeRelativeMotionEvent, int, (Display * a, int b, int c, unsigned long d), (a, b, c, d))
HOOK_FUNC(h_xtst, XTestFakeMotionEvent, int, (Display * a, int b, int c, int d, unsigned long e), (a, b, c, d, e))

#endif // SHARED_H
