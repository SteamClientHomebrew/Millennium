#ifdef __APPLE__
#include <dlfcn.h>
#include <libgen.h>
#include <libproc.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

typedef int (*millennium_entry_t)(void);

static void* g_millennium_handle = nullptr;

static bool is_steam_main_process()
{
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) return false;
    const char* name = strrchr(path, '/');
    name = name ? name + 1 : path;
    return strcmp(name, "steam_osx") == 0;
}

// Path of a library staged next to this bootstrap, or "" when it is missing.
static std::string sibling(const char* name)
{
    Dl_info info;
    char self[PATH_MAX], resolved[PATH_MAX];
    if (!dladdr((void*)sibling, &info) || !realpath(info.dli_fname, self)) return "";
    const std::string candidate = std::string(dirname(self)) + "/" + name;
    return realpath(candidate.c_str(), resolved) ? resolved : "";
}

// `millennium load` attaches after the CEF browser is already up without hhx64.
// Kill it once: steam_osx respawns it, and the new browser inherits
// DYLD_INSERT_LIBRARIES, so it loads the child hook. The browser is the
// "Steam Helper" that is steam_osx's direct child.
static void relaunch_cef_browser()
{
    pid_t pids[8192];
    const int count = proc_listallpids(pids, sizeof(pids)) / (int)sizeof(pid_t);
    char path[PROC_PIDPATHINFO_MAXSIZE];
    for (int index = 0; index < count; ++index) {
        struct proc_bsdinfo info;
        if (proc_pidinfo(pids[index], PROC_PIDTBSDINFO, 0, &info, sizeof(info)) != sizeof(info)) continue;
        if ((pid_t)info.pbi_ppid != getpid()) continue;
        if (proc_pidpath(pids[index], path, sizeof(path)) <= 0) continue;
        if (strstr(path, "Steam Helper.app/Contents/MacOS/Steam Helper")) {
            kill(pids[index], SIGKILL);
            return;
        }
    }
}

__attribute__((constructor)) static void bootstrap_init()
{
    if (!is_steam_main_process()) return;

    const std::string runtime = sibling(__MILLENNIUM_OUTPUT_NAME__);
    const std::string child_hook = sibling(__MILLENNIUM_CHILD_HOOK_OUTPUT_NAME__);
    const std::string helper_hook = sibling(__MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__);
    if (runtime.empty() || child_hook.empty() || helper_hook.empty()) {
        fprintf(stderr, "[Millennium] The staged runtime is incomplete; run `millennium setup`.\n");
        return;
    }

    // Every process steam_osx spawns inherits this environment. The CEF browser
    // loads hhx64 through the child hook, from the same release as this bootstrap.
    setenv("DYLD_INSERT_LIBRARIES", child_hook.c_str(), 1);
    setenv("MILLENNIUM_RUNTIME_PATH", runtime.c_str(), 1);
    setenv("MILLENNIUM_HOOK_HELPER_PATH", helper_hook.c_str(), 1);

    // CDP and loopback run over unix sockets the runtime listens on and hhx64 connects to.
    if (const char* home = getenv("HOME"); home && home[0]) {
        const std::string dir = std::string(home) + "/Library/Application Support/Millennium/";
        setenv("MILLENNIUM_CDP_SOCKET", (dir + "cdp.sock").c_str(), 1);
        setenv("MILLENNIUM_LOOPBACK_SOCKET", (dir + "loopback.sock").c_str(), 1);
    }

    g_millennium_handle = dlopen(runtime.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!g_millennium_handle) {
        fprintf(stderr, "[Millennium] Failed to load runtime library: %s\n", dlerror());
        return;
    }

    auto start = (millennium_entry_t)dlsym(g_millennium_handle, "StartMillennium");
    if (!start || start() < 0) {
        fprintf(stderr, "[Millennium] Failed to start runtime.\n");
        dlclose(g_millennium_handle);
        g_millennium_handle = nullptr;
        return;
    }

    relaunch_cef_browser();
}

__attribute__((destructor)) static void bootstrap_cleanup()
{
    if (!g_millennium_handle) return;
    if (auto stop = (millennium_entry_t)dlsym(g_millennium_handle, "StopMillennium")) stop();
    dlclose(g_millennium_handle);
    g_millennium_handle = nullptr;
}
#endif
