#ifdef __APPLE__
// Inserted with DYLD_INSERT_LIBRARIES into the processes steam_osx spawns. In the
// CEF browser it loads hhx64 and hands it cef_browser_host_create_browser, which
// Steam Helper resolves through dlsym; elsewhere it only forwards dlsym. Every
// process drops it from the variable, so it never reaches a second generation.
#include <crt_externs.h>
#include <dlfcn.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <os/log.h>
#include <stdio.h>
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

// Remove this library from DYLD_INSERT_LIBRARIES, keeping any other entries.
static void drop_self_from_inserts(void)
{
    const char* inserts = getenv("DYLD_INSERT_LIBRARIES");
    Dl_info info;
    char self[PATH_MAX];
    if (!inserts || !dladdr((void*)drop_self_from_inserts, &info) || !realpath(info.dli_fname, self)) return;

    char kept[4096] = "";
    const char* entry = inserts;
    while (*entry) {
        const char* end = strchr(entry, ':');
        if (!end) end = entry + strlen(entry);
        char path[PATH_MAX], resolved[PATH_MAX];
        snprintf(path, sizeof(path), "%.*s", (int)(end - entry), entry);
        if (!realpath(path, resolved) || strcmp(resolved, self) != 0) {
            if (kept[0]) strlcat(kept, ":", sizeof(kept));
            strlcat(kept, path, sizeof(kept));
        }
        entry = *end ? end + 1 : end;
    }
    if (kept[0]) {
        setenv("DYLD_INSERT_LIBRARIES", kept, 1);
    } else {
        unsetenv("DYLD_INSERT_LIBRARIES");
    }
}

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
    drop_self_from_inserts();
    if (!is_cef_browser()) return;

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
