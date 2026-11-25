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
#include <sys/proc_info.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <string>
#include <vector>

typedef int (*start_millennium_t)(void);
typedef int (*stop_millennium_t)(void);

extern char** environ;

namespace
{
constexpr const char* kRuntimePathEnv = "MILLENNIUM_RUNTIME_PATH";
constexpr const char* kChildHookPathEnv = "MILLENNIUM_CHILD_HOOK_PATH";
constexpr const char* kHelperHookPathEnv = "MILLENNIUM_HOOK_HELPER_PATH";
constexpr const char* kCdpSocketEnv = "MILLENNIUM_CDP_SOCKET";
constexpr const char* kLoopbackSocketEnv = "MILLENNIUM_LOOPBACK_SOCKET";
constexpr const char* kSteamExecutableEnv = "MILLENNIUM_STEAM_EXECUTABLE";
constexpr const char* kBootstrapTraceEnv = "MILLENNIUM_BOOTSTRAP_TRACE_PATH";
} // namespace

#define DYLD_INTERPOSE(_replacement, _replacee)                                                                                                                                    \
    __attribute__((used)) static struct                                                                                                                                            \
    {                                                                                                                                                                              \
        const void* replacement;                                                                                                                                                   \
        const void* replacee;                                                                                                                                                      \
    } _interpose_##_replacee __attribute__((section("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee }

static void* g_millennium_handle = nullptr;
static bool g_bootstrap_enabled = false;

static bool is_steam_main_process_name(const char* filename)
{
    return strcmp(filename, "steam_osx") == 0 || strcmp(filename, "steam_osx.real") == 0;
}

static bool is_steam_main_process()
{
    char path_buffer[PATH_MAX];
    uint32_t size = sizeof(path_buffer);

    if (_NSGetExecutablePath(path_buffer, &size) != 0) {
        return false;
    }

    char resolved[PATH_MAX];
    if (!realpath(path_buffer, resolved)) {
        return false;
    }

    const char* filename = strrchr(resolved, '/');
    filename = filename ? filename + 1 : resolved;
    return is_steam_main_process_name(filename);
}

static bool get_bootstrap_path(char* output, size_t output_size)
{
    Dl_info info;
    if (!dladdr((void*)get_bootstrap_path, &info) || !info.dli_fname) {
        return false;
    }

    char resolved[PATH_MAX];
    if (!realpath(info.dli_fname, resolved)) {
        return false;
    }

    strncpy(output, resolved, output_size - 1);
    output[output_size - 1] = '\0';
    return true;
}

static bool get_adjacent_or_configured_path(const char* env_name, const char* artifact_name, int access_mode, char* output, size_t output_size)
{
    const char* configured = getenv(env_name);
    if (configured && configured[0] != '\0') {
        char resolved[PATH_MAX];
        if (access(configured, access_mode) == 0 && realpath(configured, resolved)) {
            strncpy(output, resolved, output_size - 1);
            output[output_size - 1] = '\0';
            return true;
        }
        return false;
    }

    char bootstrap_path[PATH_MAX];
    if (!get_bootstrap_path(bootstrap_path, sizeof(bootstrap_path))) {
        return false;
    }

    char bootstrap_dir[PATH_MAX];
    strncpy(bootstrap_dir, bootstrap_path, sizeof(bootstrap_dir) - 1);
    bootstrap_dir[sizeof(bootstrap_dir) - 1] = '\0';

    char* directory = dirname(bootstrap_dir);
    char candidate[PATH_MAX];
    snprintf(candidate, sizeof(candidate), "%s/%s", directory, artifact_name);

    if (access(candidate, access_mode) != 0) {
        return false;
    }

    char resolved[PATH_MAX];
    if (!realpath(candidate, resolved)) {
        return false;
    }

    strncpy(output, resolved, output_size - 1);
    output[output_size - 1] = '\0';
    return true;
}

static const char* get_runtime_path()
{
    static char runtime_path[PATH_MAX];
    static bool initialized = false;

    if (!initialized) {
        if (!get_adjacent_or_configured_path(kRuntimePathEnv, __MILLENNIUM_OUTPUT_NAME__, R_OK, runtime_path, sizeof(runtime_path))) {
            return nullptr;
        }
        initialized = true;
    }

    return runtime_path;
}

static const char* get_child_hook_path()
{
    static char child_hook_path[PATH_MAX];
    static bool initialized = false;

    if (!initialized) {
        if (!get_adjacent_or_configured_path(kChildHookPathEnv, __MILLENNIUM_CHILD_HOOK_OUTPUT_NAME__, R_OK, child_hook_path, sizeof(child_hook_path))) {
            return nullptr;
        }
        initialized = true;
    }

    return child_hook_path;
}

static const char* get_helper_hook_path()
{
    static char helper_hook_path[PATH_MAX];
    static bool initialized = false;

    if (!initialized) {
        if (!get_adjacent_or_configured_path(kHelperHookPathEnv, __MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__, R_OK, helper_hook_path, sizeof(helper_hook_path))) {
            return nullptr;
        }
        initialized = true;
    }

    return helper_hook_path;
}

static void append_trace(const char* stage)
{
    const char* trace_path = getenv(kBootstrapTraceEnv);
    if (!trace_path || trace_path[0] == '\0') {
        return;
    }

    FILE* file = fopen(trace_path, "a");
    if (!file) {
        return;
    }

    fprintf(file, "%s\n", stage);
    fclose(file);
}

static void append_exec_trace(const char* stage, const char* path, char* const argv[], const char* note)
{
    const char* trace_path = getenv(kBootstrapTraceEnv);
    if (!trace_path || trace_path[0] == '\0') {
        return;
    }

    const char* arg0 = (argv && argv[0]) ? argv[0] : "";
    FILE* file = fopen(trace_path, "a");
    if (!file) {
        return;
    }

    fprintf(file, "%s|path=%s|arg0=%s|note=%s\n", stage ? stage : "", path ? path : "", arg0, note ? note : "");
    fclose(file);
}

static std::string rewrite_dyld_insert_list(const std::vector<const char*>& library_paths, const char* removed_path, char* const envp[])
{
    std::vector<std::string> rewritten_entries;
    const char* existing_value = nullptr;
    const char* prefix = "DYLD_INSERT_LIBRARIES=";
    const size_t prefix_length = strlen(prefix);

    const auto append_unique_entry = [&](const char* candidate_path)
    {
        if (!candidate_path || candidate_path[0] == '\0') {
            return;
        }

        const char* normalized_entry = candidate_path;
        char resolved_entry[PATH_MAX];
        if (realpath(candidate_path, resolved_entry)) {
            normalized_entry = resolved_entry;
        }

        if (removed_path && strcmp(normalized_entry, removed_path) == 0) {
            return;
        }

        for (const std::string& existing_entry : rewritten_entries) {
            if (existing_entry == normalized_entry) {
                return;
            }
        }

        rewritten_entries.emplace_back(normalized_entry);
    };

    for (const char* library_path : library_paths) {
        append_unique_entry(library_path);
    }

    char* const* source_env = envp ? envp : environ;
    if (source_env) {
        for (size_t index = 0; source_env[index] != nullptr; ++index) {
            if (strncmp(source_env[index], prefix, prefix_length) == 0) {
                existing_value = source_env[index] + prefix_length;
                break;
            }
        }
    }

    if (existing_value && existing_value[0]) {
        std::string existing(existing_value);
        size_t start = 0;
        while (start <= existing.size()) {
            const size_t end = existing.find(':', start);
            const std::string entry = existing.substr(start, end == std::string::npos ? std::string::npos : end - start);
            append_unique_entry(entry.c_str());

            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }
    }

    std::string rewritten;
    for (size_t index = 0; index < rewritten_entries.size(); ++index) {
        if (index != 0) {
            rewritten.append(":");
        }
        rewritten.append(rewritten_entries[index]);
    }

    return rewritten;
}

static std::vector<std::string> build_child_environment(char* const envp[], const std::string& dyld_insert_libraries)
{
    std::vector<std::string> environment;
    char* const* source_env = envp ? envp : environ;

    if (source_env) {
        for (size_t index = 0; source_env[index] != nullptr; ++index) {
            if (strncmp(source_env[index], "DYLD_INSERT_LIBRARIES=", 22) == 0) {
                continue;
            }
            environment.emplace_back(source_env[index]);
        }
    }

    if (!dyld_insert_libraries.empty()) {
        environment.emplace_back("DYLD_INSERT_LIBRARIES=" + dyld_insert_libraries);
    }

    return environment;
}

struct DyldInsertPlan
{
    std::string dyldInsertLibraries;
    bool includesBootstrap = false;
    bool includesChildHook = false;
};

static bool is_steam_main_exec_target(const char* path)
{
    if (!path || path[0] == '\0') {
        return false;
    }

    const char* configured_target = getenv(kSteamExecutableEnv);
    if (!configured_target || configured_target[0] == '\0') {
        return false;
    }

    char resolved_target[PATH_MAX];
    const char* normalized_target = configured_target;
    if (realpath(configured_target, resolved_target)) {
        normalized_target = resolved_target;
    }

    char resolved_path[PATH_MAX];
    const char* normalized_path = path;
    if (realpath(path, resolved_path)) {
        normalized_path = resolved_path;
    }

    return strcmp(normalized_path, normalized_target) == 0;
}

static DyldInsertPlan build_dyld_insert_plan(const char* path, char* const envp[], const char* bootstrap_path_ptr)
{
    DyldInsertPlan plan;
    const char* child_hook_path = get_child_hook_path();

    plan.includesBootstrap = bootstrap_path_ptr && is_steam_main_exec_target(path);
    plan.includesChildHook = child_hook_path && access(child_hook_path, R_OK) == 0;

    std::vector<const char*> insert_libraries;
    if (plan.includesBootstrap) {
        insert_libraries.push_back(bootstrap_path_ptr);
    }
    if (plan.includesChildHook) {
        insert_libraries.push_back(child_hook_path);
    }

    plan.dyldInsertLibraries = rewrite_dyld_insert_list(insert_libraries, plan.includesBootstrap ? nullptr : bootstrap_path_ptr, envp);
    return plan;
}

static const char* describe_dyld_insert_plan(const DyldInsertPlan& plan)
{
    if (plan.includesBootstrap && plan.includesChildHook) {
        return "bootstrap+child-hook";
    }
    if (plan.includesBootstrap) {
        return "bootstrap";
    }
    if (plan.includesChildHook) {
        return "child-hook";
    }
    return "";
}

// Every helper steam_osx spawns must inherit child_hook through
// DYLD_INSERT_LIBRARIES. The CEF browser is started with posix_spawn, which the
// execve/execv interpose never sees, so the variable is set on steam_osx itself.
// On attach it starts empty: set it outright and drop the bootstrap's own entry,
// which only steam_osx needs.
static void update_current_process_dyld_inserts()
{
    char bootstrap_path[PATH_MAX];
    const char* removed_path = get_bootstrap_path(bootstrap_path, sizeof(bootstrap_path)) ? bootstrap_path : nullptr;

    const char* child_hook_path = get_child_hook_path();
    std::vector<const char*> insert_libraries;

    if (child_hook_path && access(child_hook_path, R_OK) == 0) {
        insert_libraries.push_back(child_hook_path);
    }

    const std::string rewritten = rewrite_dyld_insert_list(insert_libraries, removed_path, nullptr);
    if (rewritten.empty()) {
        unsetenv("DYLD_INSERT_LIBRARIES");
        return;
    }

    setenv("DYLD_INSERT_LIBRARIES", rewritten.c_str(), 1);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static int call_execve_syscall(const char* path, char* const argv[], char* const envp[])
{
    return static_cast<int>(syscall(SYS_execve, path, argv, envp ? envp : environ));
}
#pragma clang diagnostic pop

static int millennium_execve(const char* path, char* const argv[], char* const envp[])
{
    append_exec_trace("exec-enter", path, argv, "");

    if (!g_bootstrap_enabled) {
        append_exec_trace("exec-bootstrap-disabled", path, argv, "");
        return call_execve_syscall(path, argv, envp);
    }

    char bootstrap_path[PATH_MAX];
    const char* bootstrap_path_ptr = get_bootstrap_path(bootstrap_path, sizeof(bootstrap_path)) ? bootstrap_path : nullptr;
    const DyldInsertPlan plan = build_dyld_insert_plan(path, envp, bootstrap_path_ptr);
    std::vector<std::string> owned_environment = build_child_environment(envp, plan.dyldInsertLibraries);
    std::vector<char*> exec_environment;
    exec_environment.reserve(owned_environment.size() + 1);
    for (std::string& entry : owned_environment) {
        exec_environment.push_back(entry.data());
    }
    exec_environment.push_back(nullptr);

    const char* trace_stage = plan.includesBootstrap || plan.includesChildHook ? "exec-pass-through" : "exec-missing-insert";
    append_exec_trace(trace_stage, path, argv, describe_dyld_insert_plan(plan));
    return call_execve_syscall(path, argv, exec_environment.data());
}

DYLD_INTERPOSE(millennium_execve, execve);

static int millennium_execv(const char* path, char* const argv[])
{
    return millennium_execve(path, argv, environ);
}

DYLD_INTERPOSE(millennium_execv, execv);

// When `millennium load` attaches to a running steam_osx, the CEF browser is
// already up without hhx64. Kill it once: steam_osx respawns it, and the new
// browser inherits the DYLD_INSERT_LIBRARIES set above, so it loads the hook.

// The wrapper sets MILLENNIUM_STEAM_EXECUTABLE; `millennium load` sets nothing.
static bool loaded_by_attach()
{
    const char* value = getenv(kSteamExecutableEnv);
    return !value || value[0] == '\0';
}

// The browser is the "Steam Helper" that is steam_osx's direct child (no --type=).
static void relaunch_cef_browser()
{
    const pid_t self = getpid();
    pid_t pids[8192];
    const int count = proc_listallpids(pids, sizeof(pids)) / (int)sizeof(pid_t);
    char path[PROC_PIDPATHINFO_MAXSIZE];
    for (int index = 0; index < count; ++index) {
        const pid_t pid = pids[index];
        if (pid <= 0) continue;
        struct proc_bsdinfo info;
        if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) != sizeof(info)) continue;
        if ((pid_t)info.pbi_ppid != self) continue;
        if (proc_pidpath(pid, path, sizeof(path)) <= 0) continue;
        if (strstr(path, "Steam Helper.app/Contents/MacOS/Steam Helper")) {
            append_trace("attach-relaunch-browser");
            kill(pid, SIGKILL);
            return;
        }
    }
    append_trace("attach-no-browser");
}

__attribute__((constructor)) static void bootstrap_init()
{
    if (!is_steam_main_process()) {
        return;
    }

    g_bootstrap_enabled = true;
    append_trace("bootstrap-main-process");
    update_current_process_dyld_inserts();

    const char* runtime_path = get_runtime_path();
    if (!runtime_path) {
        fprintf(stderr, "[Millennium] Failed to resolve runtime path for macOS bootstrap.\n");
        return;
    }

    // Publish this release's paths before the runtime loads, so the runtime and
    // every process Steam spawns (the CEF browser, through the child hook) load
    // their libraries from the same release as the bootstrap.
    setenv(kRuntimePathEnv, runtime_path, 1);
    if (const char* child_hook_path = get_child_hook_path()) {
        setenv(kChildHookPathEnv, child_hook_path, 1);
    }
    if (const char* helper_hook_path = get_helper_hook_path()) {
        setenv(kHelperHookPathEnv, helper_hook_path, 1);
    }

    // CDP and loopback run over unix sockets the runtime listens on and hhx64 connects to.
    if (const char* home = getenv("HOME"); home && home[0]) {
        const std::string dir = std::string(home) + "/Library/Application Support/Millennium/";
        setenv(kCdpSocketEnv, (dir + "cdp.sock").c_str(), 1);
        setenv(kLoopbackSocketEnv, (dir + "loopback.sock").c_str(), 1);
    }

    g_millennium_handle = dlopen(runtime_path, RTLD_NOW | RTLD_GLOBAL);
    if (!g_millennium_handle) {
        fprintf(stderr, "[Millennium] Failed to load runtime library: %s\n", dlerror());
        return;
    }

    start_millennium_t start_millennium = (start_millennium_t)dlsym(g_millennium_handle, "StartMillennium");
    if (!start_millennium) {
        fprintf(stderr, "[Millennium] Failed to locate StartMillennium: %s\n", dlerror());
        dlclose(g_millennium_handle);
        g_millennium_handle = nullptr;
        return;
    }

    if (start_millennium() < 0) {
        fprintf(stderr, "[Millennium] Failed to start runtime.\n");
        dlclose(g_millennium_handle);
        g_millennium_handle = nullptr;
        return;
    }

    if (loaded_by_attach()) {
        relaunch_cef_browser();
    }
}

__attribute__((destructor)) static void bootstrap_cleanup()
{
    if (!g_millennium_handle) {
        return;
    }

    stop_millennium_t stop_millennium = (stop_millennium_t)dlsym(g_millennium_handle, "StopMillennium");
    if (stop_millennium) {
        stop_millennium();
    }

    dlclose(g_millennium_handle);
    g_millennium_handle = nullptr;

    g_bootstrap_enabled = false;
}
#endif
