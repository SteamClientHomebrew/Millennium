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

#ifdef __linux__
#include "shared.h"

static void* h_hhx64 = NULL;

/**
 * shared objects don't have an explicit entry point, so we tell the compiler to
 * jump to our attach function whenever we are given code execution control.
 *
 * we do have to be careful calling certain functions as we are loaded before main() is called.
 */
static void libmillennium_hhx64_init(void) __attribute__((constructor));
static void libmillennium_hhx64_cleanup(void) __attribute__((destructor));

static const char* get_hhx64_path(void)
{
#ifdef MILLENNIUM_HHX64_PATH
    return MILLENNIUM_HHX64_PATH;
#else
    static char path[PATH_MAX];
    /* resolve our own path via /proc, then swap the filename */
    char maps_line[PATH_MAX + 64];
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return NULL;

    Dl_info info;
    if (!dladdr((void*)get_hhx64_path, &info) || !info.dli_fname) {
        fclose(f);
        return NULL;
    }
    fclose(f);

    char resolved[PATH_MAX];
    if (!realpath(info.dli_fname, resolved)) return NULL;

    /* replace our filename with the hhx64 filename */
    char* slash = strrchr(resolved, '/');
    if (!slash) return NULL;
    snprintf(path, sizeof(path), "%.*s/" MILLENNIUM_HHX64_NAME, (int)(slash - resolved), resolved);
    return path;
#endif
}

static void libmillennium_hhx64_init(void)
{
    char* p = get_process_path();
    if (!p || !strstr(p, "steamwebhelper")) {
        LOG_INFO("Skipping hhx64 setup for non-steamwebhelper process: %s", p ? p : "(unknown)");
        return;
    }

    /*
     * steamwebhelper links libXtst.so.6 directly, so it's already in the
     * process link map. Use the soname only (no path) so the dynamic linker
     * finds whatever copy is mapped regardless of the container's prefix.
     */
    h_xtst = dlopen("libXtst.so.6", RTLD_NOW | RTLD_GLOBAL | RTLD_NOLOAD);
    if (!h_xtst) LOG_WARN("libXtst.so.6 not found in process link map; XTest forwarding disabled");

    const char* hhx64_path = get_hhx64_path();
    if (!hhx64_path) {
        LOG_ERROR("Failed to resolve hhx64 path");
        return;
    }

    h_hhx64 = dlopen_or_log(hhx64_path, RTLD_NOW | RTLD_GLOBAL);
    if (!h_hhx64) return;

    LOG_INFO("Loaded hhx64 from %s", hhx64_path);
}

static void libmillennium_hhx64_cleanup(void)
{
    LOG_INFO("Unloading Millennium library...");

    int status;
    const void** libs[] = { h_xtst, h_hhx64 };

    for (int i = 0; i < 2; i++) {
        if (!libs[i]) continue;

        status = dlclose(libs[i]);
        if (status) LOG_ERROR("Failed to unload module %d: %s", i, dlerror());
    }
}
#endif
