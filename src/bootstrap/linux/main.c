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

#ifdef __linux__
#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <string.h>
#include <dlfcn.h>
#include <libgen.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define COLOR_RESET "\033[0m"
#define COLOR_INFO "\033[1;34m"
#define COLOR_ERROR "\033[1;31m"
#define COLOR_WARN "\033[1;33m"

void* h_xtst = NULL;
static void* h_millennium = NULL;
static int b_has_loaded_millennium = 0;

typedef int (*start_millennium_t)(void);
typedef int (*stop_millennium_t)(void);

#define HOOK_FUNC(name, ret_type, params, args)                                                                                                                                    \
    __attribute__((visibility("default"))) ret_type name params                                                                                                                    \
    {                                                                                                                                                                              \
        static ret_type(*orig) params = NULL;                                                                                                                                      \
        if (!orig) {                                                                                                                                                               \
            orig = (ret_type(*) params)dlsym(h_xtst, #name);                                                                                                                       \
            if (!orig) {                                                                                                                                                           \
                LOG_ERROR("Cannot find original %s", #name);                                                                                                                       \
                return False;                                                                                                                                                      \
            }                                                                                                                                                                      \
        }                                                                                                                                                                          \
        return orig args;                                                                                                                                                          \
    }

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

#ifdef MILLENNIUM_RUNTIME_PATH
static const char* k_millennium_path = MILLENNIUM_RUNTIME_PATH;
#else
static const char* get_millennium_library_path(void)
{
    static char path_buffer[PATH_MAX];
    static int initialized = 0;

    if (!initialized) {
        const char* envPath = getenv("MILLENNIUM_RUNTIME_PATH");
        if (envPath) {
            strncpy(path_buffer, envPath, PATH_MAX - 1);
            path_buffer[PATH_MAX - 1] = '\0';
        } else {
            strncpy(path_buffer, "/usr/lib/millennium/libmillennium_x86.so", PATH_MAX - 1);
            path_buffer[PATH_MAX - 1] = '\0';
        }
        initialized = 1;
    }
    return path_buffer;
}
#define k_millennium_path (get_millennium_library_path())
#endif

static char* get_process_path(void)
{
    static char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        return path;
    }
    return NULL;
}

static char* get_process_path_parent(void)
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

static int load_and_start_millennium(void)
{
    h_millennium = dlopen(k_millennium_path, RTLD_LAZY | RTLD_GLOBAL);
    if (!h_millennium) {
        LOG_ERROR("Failed to load Millennium library: %s", dlerror());
        return 0;
    }

    start_millennium_t fn_start_millennium = (start_millennium_t)dlsym(h_millennium, "StartMillennium");
    if (!fn_start_millennium) {
        LOG_ERROR("Failed to locate ordinal HookInterop::StartMillennium: %s", dlerror());
        dlclose(h_millennium);
        h_millennium = NULL;
        return 0;
    }

    int result = fn_start_millennium();
    if (result < 0) {
        LOG_ERROR("Failed to start Millennium: %d", result);
        dlclose(h_millennium);
        h_millennium = NULL;
        return 0;
    }

    return 1;
}

static void stop_and_unload_millennium(void)
{
    if (!h_millennium) {
        LOG_ERROR("Millennium library is not loaded.");
        return;
    }

    stop_millennium_t fn_stop_millennium = (stop_millennium_t)dlsym(h_millennium, "StopMillennium");
    if (fn_stop_millennium) {
        int result = fn_stop_millennium();
        if (result < 0) {
            LOG_ERROR("Failed to stop Millennium: %d", result);
        }
    } else {
        LOG_ERROR("Failed to locate ordinal HookInterop::StopMillennium: %s", dlerror());
    }

    dlclose(h_millennium);
    h_millennium = NULL;
}

static void setup_hooks(void)
{
    const char* p = get_process_path_parent();
    if (!p) {
        LOG_ERROR("Failed to retrieve current directory.");
        return;
    }

    char lbxtst_path[PATH_MAX];
    snprintf(lbxtst_path, PATH_MAX, "%s/steam-runtime/usr/lib/i386-linux-gnu/libXtst.so.6", p);

    if (access(lbxtst_path, F_OK) == -1) {
        LOG_ERROR("Pinned libXtst does not exist at: %s", lbxtst_path);
        return;
    }

    h_xtst = dlopen(lbxtst_path, RTLD_LAZY | RTLD_GLOBAL);
    if (!h_xtst) {
        fprintf(stderr, "Failed to load libXtst: %s\n", dlerror());
    }
}

static int is_steam_process(void)
{
    char* p = get_process_path();
    if (!p) return 0;

    char rp[PATH_MAX];
    if (!realpath(p, rp)) return 0;

    const char* home = getenv("HOME");
    if (!home) return 0;

    char steam_path[PATH_MAX];
    snprintf(steam_path, PATH_MAX, "%s/.steam/steam/ubuntu12_32/steam", home);

    char rsteam_path[PATH_MAX];
    if (!realpath(steam_path, rsteam_path)) return 0;

    return strcmp(rp, rsteam_path) == 0;
}

static void proxy_sentinel_init(void) __attribute__((constructor));
static void proxy_sentinel_cleanup(void) __attribute__((destructor));

static void proxy_at_exit_handler(void)
{
    LOG_INFO("at_exit: invoking stop_and_unload_millennium()");
    if (b_has_loaded_millennium) {
        stop_and_unload_millennium();
    }
}

static void proxy_sentinel_init(void)
{
    const int is_steam_proc = is_steam_process();
    if (!is_steam_proc) {
        LOG_INFO("Skipping Millennium setup for non-Steam process. Process path: %s", get_process_path());
        return;
    }

    LOG_INFO("Setting up proxy hooks...");
    setup_hooks();

    b_has_loaded_millennium = 1;

    LOG_INFO("Bootstrap library loaded successfully. Using Millennium library at: %s", k_millennium_path);
    if (!load_and_start_millennium()) {
        LOG_ERROR("Failed to load Millennium...");
        return;
    }

    LOG_INFO("Started Millennium...");

    if (atexit(proxy_at_exit_handler) != 0) {
        LOG_ERROR("Failed to register atexit handler for Millennium cleanup");
    }
}

static void proxy_sentinel_cleanup(void)
{
    // LOG_INFO("Unloading Millennium library...");

    // if (h_xtst) dlclose(h_xtst);
    // if (!b_has_loaded_millennium) return;

    // stop_and_unload_millennium();
}

/** Forward legitimate library calls to the original library */
HOOK_FUNC(XTestFakeButtonEvent, int, (Display * a, unsigned int b, Bool c, unsigned long d), (a, b, c, d))
HOOK_FUNC(XTestFakeKeyEvent, int, (Display * a, unsigned int b, Bool c, unsigned long d), (a, b, c, d))
HOOK_FUNC(XTestQueryExtension, int, (Display * a, int* b, int* c, int* d, int* e), (a, b, c, d, e))
HOOK_FUNC(XTestFakeRelativeMotionEvent, int, (Display * a, int b, int c, unsigned long d), (a, b, c, d))
HOOK_FUNC(XTestFakeMotionEvent, int, (Display * a, int b, int c, int d, unsigned long e), (a, b, c, d, e))
#endif
