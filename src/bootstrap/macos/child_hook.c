#ifdef __APPLE__
// Inserted with DYLD_INSERT_LIBRARIES into every process steam_osx spawns. In the
// CEF browser it loads hhx64 and hands it cef_browser_host_create_browser, which
// Steam Helper resolves through dlsym; elsewhere it only forwards dlsym.
#include <crt_externs.h>
#include <dlfcn.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <os/log.h>
#include <stdlib.h>
#include <string.h>

#define DYLD_INTERPOSE(replacement, replacee)                                                                                                                                      \
    __attribute__((used)) static struct                                                                                                                                            \
    {                                                                                                                                                                              \
        const void* replacement;                                                                                                                                                   \
        const void* replacee;                                                                                                                                                      \
    } interpose_##replacee __attribute__((section("__DATA,__interpose"))) = { (const void*)&replacement, (const void*)&replacee }

static const char kCreateBrowser[] = "cef_browser_host_create_browser";

// hhx64's millennium_hook_cef_export: takes the real export, returns its replacement.
static void* (*g_hook_export)(const char* symbol, void* real);

// dyld does not interpose this image's own calls, so `dlsym` here is the real one.
// Every other lookup is a tail call, which keeps the caller's return address and
// with it RTLD_NEXT / RTLD_SELF semantics.
static void* millennium_dlsym(void* handle, const char* symbol)
{
    if (g_hook_export && symbol && strcmp(symbol, kCreateBrowser) == 0) {
        void* real = dlsym(handle, symbol);
        return real ? g_hook_export(symbol, real) : NULL;
    }
    __attribute__((musttail)) return dlsym(handle, symbol);
}
DYLD_INTERPOSE(millennium_dlsym, dlsym);

// The CEF browser is the "Steam Helper" with no --type= switch.
static int is_cef_browser(void)
{
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) return 0;
    const char* name = strrchr(path, '/');
    if (!name || strcmp(name + 1, "Steam Helper") != 0) return 0;

    char** argv = *_NSGetArgv();
    for (int index = 1; argv[index]; ++index) {
        if (strncmp(argv[index], "--type=", 7) == 0) return 0;
    }
    return 1;
}

__attribute__((constructor)) static void child_hook_init(void)
{
    if (!is_cef_browser()) return;

    // The browser's own children (renderers, GPU, utilities) need nothing from us.
    unsetenv("DYLD_INSERT_LIBRARIES");

    os_log_t log = os_log_create("app.steambrew.millennium", "child-hook");
    const char* path = getenv("MILLENNIUM_HOOK_HELPER_PATH");
    void* hhx64 = path ? dlopen(path, RTLD_NOW | RTLD_GLOBAL) : NULL;
    if (!hhx64) {
        os_log_error(log, "could not load hhx64 from %{public}s: %{public}s", path ? path : "(unset)", path ? dlerror() : "");
        return;
    }
    g_hook_export = dlsym(hhx64, "millennium_hook_cef_export");
    if (!g_hook_export) os_log_error(log, "hhx64 has no millennium_hook_cef_export");
}
#endif
