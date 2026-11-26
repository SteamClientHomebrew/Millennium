#ifdef __APPLE__
#include <crt_externs.h>
#include <dlfcn.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Every process steam_osx spawns loads this through DYLD_INSERT_LIBRARIES. Only
// the CEF browser needs hhx64: it is the "Steam Helper" with no --type= switch.
static bool is_cef_browser()
{
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) return false;
    const char* name = strrchr(path, '/');
    if (!name || strcmp(name + 1, "Steam Helper") != 0) return false;

    char** argv = *_NSGetArgv();
    for (int index = 1; argv[index]; ++index) {
        if (strncmp(argv[index], "--type=", 7) == 0) return false;
    }
    return true;
}

__attribute__((constructor)) static void child_hook_init()
{
    if (!is_cef_browser()) return;

    // The browser's own children (renderers, GPU, utilities) need nothing from us.
    unsetenv("DYLD_INSERT_LIBRARIES");

    const char* hhx64 = getenv("MILLENNIUM_HOOK_HELPER_PATH");
    if (!hhx64 || !dlopen(hhx64, RTLD_NOW | RTLD_GLOBAL)) {
        fprintf(stderr, "[Millennium] Failed to load hhx64: %s\n", hhx64 ? dlerror() : "path not set");
    }
}
#endif
