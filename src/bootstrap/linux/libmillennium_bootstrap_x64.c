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

/**
 * libmillennium_bootstrap_x64 — the SteamRT3 (64-bit Steam client) XTest proxy.
 *
 * Installed as <steam>/steamrt64/libXtst.so.6, so it is loaded into every 64-bit process
 * that links libXtst: the client (via steamui.so / steamclient.so), the webhelper,
 * streaming_client and vgui2_s.so. It forwards XTest to the runtime's real libXtst and
 * dispatches Millennium injection by process:
 *
 *   steamrt64/steam            -> libmillennium_x64.so (StartMillennium)
 *   steamwebhelper[.sh] chain  -> libmillennium_hhx64.so
 *   anything else              -> XTest forwarding only
 *
 * A SteamRT3 install runs the client inside a pressure-vessel container, where
 * /usr/lib/millennium does not exist. Paths are therefore resolved from this library's
 * own location first, then from the environment, so a store path or a host path both work.
 */
#ifdef __linux__
#include "shared.h"

#include <fcntl.h>
#include <libgen.h>

#define MILLENNIUM_X64_LIBRARY_NAME "libmillennium_x64.so"
#define MILLENNIUM_HHX64_LIBRARY_NAME "libmillennium_hhx64.so"
#define MILLENNIUM_DEFAULT_LIBRARY_DIR "/usr/lib/millennium"

static void* h_millennium = NULL;
static void* h_hhx64 = NULL;
static int b_has_loaded_millennium = 0;

typedef int (*start_millennium_t)(void);
typedef int (*stop_millennium_t)(void);

#ifdef MILLENNIUM_X64_PATH
static const char* get_millennium_library_path(void)
{
    return MILLENNIUM_X64_PATH;
}
#else
static const char* get_millennium_library_path(void)
{
    static char path_buffer[PATH_MAX];
    static int initialized = 0;

    if (!initialized) {
        const char* env_path = getenv("MILLENNIUM_RUNTIME_PATH_X64");
        if (env_path && env_path[0]) {
            snprintf(path_buffer, sizeof(path_buffer), "%s", env_path);
        } else {
            snprintf(path_buffer, sizeof(path_buffer), "%s/%s", MILLENNIUM_DEFAULT_LIBRARY_DIR, MILLENNIUM_X64_LIBRARY_NAME);
        }
        initialized = 1;
    }
    return path_buffer;
}
#endif

/** Directory this proxy was loaded from, resolved through dladdr so it works inside a container. */
static const char* get_own_directory(void)
{
    static char dir[PATH_MAX];
    static int initialized = 0;

    if (initialized) return dir[0] ? dir : NULL;

    initialized = 1;
    Dl_info info;
    if (!dladdr((void*)get_own_directory, &info) || !info.dli_fname) return NULL;

    char resolved[PATH_MAX];
    if (!realpath(info.dli_fname, resolved)) return NULL;

    snprintf(dir, sizeof(dir), "%s", resolved);
    char* slash = strrchr(dir, '/');
    if (!slash) {
        dir[0] = '\0';
        return NULL;
    }
    *slash = '\0';
    return dir;
}

static const char* get_hhx64_path(void)
{
#ifdef MILLENNIUM_HHX64_PATH
    return MILLENNIUM_HHX64_PATH;
#else
    static char path[PATH_MAX];
    static int initialized = 0;

    if (initialized) return path[0] ? path : NULL;
    initialized = 1;

    const char* dir = get_own_directory();
    if (!dir) return NULL;

    snprintf(path, sizeof(path), "%s/%s", dir, MILLENNIUM_HHX64_LIBRARY_NAME);
    return path;
#endif
}

static int cmdline_has_arg(const char* needle)
{
    char buffer[4096];
    int fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd < 0) return 0;

    ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (len <= 0) return 0;

    buffer[len] = '\0';
    for (ssize_t offset = 0; offset < len; offset += (ssize_t)strlen(buffer + offset) + 1) {
        if (strcmp(buffer + offset, needle) == 0) return 1;
    }
    return 0;
}

/** "steam" inside a steamrt64 directory, and not the -child-update-ui helper that reuses the same binary. */
static int is_steam_client_process(void)
{
    char exe[PATH_MAX];
    if (!get_process_path()) return 0;
    if (!realpath(get_process_path(), exe)) return 0;

    if (cmdline_has_arg("-child-update-ui")) return 0;

    char copy[PATH_MAX];
    snprintf(copy, sizeof(copy), "%s", exe);

    char* file = basename(copy);
    if (strcmp(file, "steam") != 0) return 0;

    char parent[PATH_MAX];
    snprintf(parent, sizeof(parent), "%s", exe);
    char* dir = dirname(parent);

    char dir_copy[PATH_MAX];
    snprintf(dir_copy, sizeof(dir_copy), "%s", dir);
    return strcmp(basename(dir_copy), "steamrt64") == 0;
}

static int is_webhelper_process(void)
{
    char* path = get_process_path();
    return path != NULL && strstr(path, "steamwebhelper") != NULL;
}

/**
 * The proxy satisfies the process's libXtst.so.6 dependency, so resolving by soname with
 * RTLD_NOLOAD can return this library and recurse. The runtime's copy is opened by
 * absolute path instead. All three candidates exist across steamrt3c and the SDK images.
 */
static void resolve_real_xtst(void)
{
    static const char* candidates[] = {
        "/usr/lib/x86_64-linux-gnu/libXtst.so.6",
        "/usr/lib64/libXtst.so.6",
        "/usr/lib/libXtst.so.6",
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (access(candidates[i], F_OK) != 0) continue;

        h_xtst = dlopen(candidates[i], RTLD_LAZY | RTLD_GLOBAL);
        if (h_xtst) return;
        LOG_WARN("Failed to dlopen real libXtst at %s: %s", candidates[i], dlerror());
    }

    LOG_WARN("No real libXtst found; XTest forwarding is disabled in %s", get_process_path());
}

static int load_and_start_millennium(void)
{
    const char* path = get_millennium_library_path();
    h_millennium = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);

    if (!h_millennium) {
        /**
         * On an FHS layout the configured path is /usr/lib/millennium, which does not exist inside the
         * SteamRT3 container. The host root is reachable under /run/host, so retry there once.
         */
        char host_path[PATH_MAX];
        if (path[0] == '/') {
            snprintf(host_path, sizeof(host_path), "/run/host%s", path);
            h_millennium = dlopen(host_path, RTLD_LAZY | RTLD_GLOBAL);
            if (h_millennium) path = host_path;
        }
    }

    if (!h_millennium) {
        LOG_ERROR("Failed to dlopen %s: %s", path, dlerror());
        return 0;
    }

    start_millennium_t fn_start = (start_millennium_t)dlsym_or_log(h_millennium, "StartMillennium");
    if (!fn_start) {
        dlclose(h_millennium);
        h_millennium = NULL;
        return 0;
    }

    int result = fn_start();
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
    if (!h_millennium) return;

    stop_millennium_t fn_stop = (stop_millennium_t)dlsym(h_millennium, "StopMillennium");
    if (fn_stop) {
        int result = fn_stop();
        if (result < 0) LOG_ERROR("Failed to stop Millennium: %d", result);
    }

    dlclose(h_millennium);
    h_millennium = NULL;
    b_has_loaded_millennium = 0;
}

static void load_webhelper_hook(void)
{
    const char* path = get_hhx64_path();
    if (!path) {
        LOG_ERROR("Failed to resolve hhx64 path");
        return;
    }

    h_hhx64 = dlopen_or_log(path, RTLD_NOW | RTLD_GLOBAL);
    if (!h_hhx64) return;

    LOG_INFO("Loaded hhx64 from %s", path);
}

static void libmillennium_bootstrap_x64_init(void) __attribute__((constructor));
static void libmillennium_bootstrap_x64_cleanup(void) __attribute__((destructor));

static void proxy_at_exit_handler(void)
{
    if (b_has_loaded_millennium) stop_and_unload_millennium();
}

static void libmillennium_bootstrap_x64_init(void)
{
    resolve_real_xtst();

    if (is_webhelper_process()) {
        LOG_INFO("SteamRT3 bootstrap: webhelper process, loading hhx64");
        load_webhelper_hook();
        return;
    }

    if (!is_steam_client_process()) {
        LOG_INFO("SteamRT3 bootstrap: forwarding XTest only for %s", get_process_path());
        return;
    }

    LOG_INFO("SteamRT3 bootstrap: Steam client process, starting Millennium from %s", get_millennium_library_path());

    if (!load_and_start_millennium()) return;

    b_has_loaded_millennium = 1;
    LOG_INFO("Started Millennium...");

    if (atexit(proxy_at_exit_handler) != 0) {
        LOG_ERROR("Failed to register atexit handler for Millennium cleanup");
    }
}

static void libmillennium_bootstrap_x64_cleanup(void)
{
    if (h_xtst) dlclose(h_xtst);
    if (h_hhx64) dlclose(h_hhx64);
    if (!b_has_loaded_millennium) return;

    stop_and_unload_millennium();
}
#endif
