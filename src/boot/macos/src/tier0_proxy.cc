#ifdef __APPLE__
#include <crt_externs.h>
#include <dlfcn.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <filesystem>
#include <mutex>
#include <string>

#ifndef __MILLENNIUM_OUTPUT_NAME__
#define __MILLENNIUM_OUTPUT_NAME__ "libmillennium.dylib"
#endif

#ifndef __MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__
#define __MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__ "libmillennium_hhx64.dylib"
#endif

#ifndef __MILLENNIUM_CHILD_HOOK_OUTPUT_NAME__
#define __MILLENNIUM_CHILD_HOOK_OUTPUT_NAME__ "libmillennium_child_hook.dylib"
#endif

typedef int (*start_millennium_t)(void);
typedef int (*stop_millennium_t)(void);

namespace {
constexpr const char* kRuntimePathEnv = "MILLENNIUM_RUNTIME_PATH";
constexpr const char* kHookHelperPathEnv = "MILLENNIUM_HOOK_HELPER_PATH";
constexpr const char* kChildHookPathEnv = "MILLENNIUM_CHILD_HOOK_PATH";
constexpr const char* kDebugPortEnv = "MILLENNIUM_DEBUG_PORT";
constexpr const char* kSteamExecutableEnv = "MILLENNIUM_STEAM_EXECUTABLE";
constexpr const char* kCefRemoteDebuggingMarker = ".cef-enable-remote-debugging";
constexpr const char* kDefaultDebugPort = "8080";
constexpr const char* kDevtoolsPortArgument = "-devtools-port";
}

static void* g_millennium_handle = nullptr;
static bool g_millennium_started = false;
static std::once_flag g_start_once;

static void log_proxy(const char* message)
{
    fprintf(stderr, "[Millennium][tier0-proxy] %s\n", message);
}

static void log_proxyf(const char* message, const char* detail)
{
    fprintf(stderr, "[Millennium][tier0-proxy] %s: %s\n", message, detail ? detail : "(null)");
}

static bool resolve_current_module_directory(std::filesystem::path& output)
{
    Dl_info info;
    if (!dladdr((void*)resolve_current_module_directory, &info) || !info.dli_fname) {
        return false;
    }

    char resolved[PATH_MAX];
    if (!realpath(info.dli_fname, resolved)) {
        return false;
    }

    output = std::filesystem::path(resolved).parent_path();
    return true;
}

static bool resolve_adjacent_readable_file(const char* filename, std::string& output)
{
    std::filesystem::path module_directory;
    if (!resolve_current_module_directory(module_directory)) {
        return false;
    }

    const std::filesystem::path candidate = module_directory / filename;
    if (access(candidate.c_str(), R_OK) != 0) {
        return false;
    }

    output = candidate.string();
    return true;
}

static bool resolve_current_executable_path(std::string& output)
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

    output = resolved;
    return true;
}

static bool is_steam_main_process()
{
    std::string executable_path;
    if (!resolve_current_executable_path(executable_path)) {
        return false;
    }

    const char* filename = strrchr(executable_path.c_str(), '/');
    filename = filename ? filename + 1 : executable_path.c_str();

    return strcmp(filename, "steam_osx") == 0 || strcmp(filename, "steam_osx.real") == 0;
}

static bool try_extract_devtools_port_from_args(std::string& output)
{
    char** argv = _NSGetArgv() ? *_NSGetArgv() : nullptr;
    if (!argv) {
        return false;
    }

    const size_t prefixed_length = strlen(kDevtoolsPortArgument);

    for (size_t index = 1; argv[index] != nullptr; ++index) {
        const char* argument = argv[index];
        if (!argument) {
            continue;
        }

        if (strcmp(argument, kDevtoolsPortArgument) == 0) {
            if (argv[index + 1] && argv[index + 1][0] != '\0') {
                output = argv[index + 1];
                return true;
            }
            continue;
        }

        if (strncmp(argument, kDevtoolsPortArgument, prefixed_length) == 0 && argument[prefixed_length] == '=') {
            const char* value = argument + prefixed_length + 1;
            if (value[0] != '\0') {
                output = value;
                return true;
            }
        }
    }

    return false;
}

static bool ensure_cef_remote_debugging_marker(const char* steam_executable_path)
{
    if (!steam_executable_path || steam_executable_path[0] == '\0') {
        return false;
    }

    const std::filesystem::path steam_path(steam_executable_path);
    if (!steam_path.has_parent_path()) {
        return false;
    }

    const std::filesystem::path marker_path = steam_path.parent_path() / kCefRemoteDebuggingMarker;
    FILE* marker_file = fopen(marker_path.c_str(), "a");
    if (!marker_file) {
        return false;
    }

    fclose(marker_file);
    return true;
}

static void maybe_set_env_if_unset(const char* env_name, const std::string& value)
{
    const char* existing = getenv(env_name);
    if (existing && existing[0] != '\0') {
        return;
    }

    setenv(env_name, value.c_str(), 1);
}

static void ensure_runtime_environment_defaults()
{
    std::string runtime_path;
    if (resolve_adjacent_readable_file(__MILLENNIUM_OUTPUT_NAME__, runtime_path)) {
        maybe_set_env_if_unset(kRuntimePathEnv, runtime_path);
    }

    std::string helper_hook_path;
    if (resolve_adjacent_readable_file(__MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__, helper_hook_path)) {
        maybe_set_env_if_unset(kHookHelperPathEnv, helper_hook_path);
    }

    std::string child_hook_path;
    if (resolve_adjacent_readable_file(__MILLENNIUM_CHILD_HOOK_OUTPUT_NAME__, child_hook_path)) {
        maybe_set_env_if_unset(kChildHookPathEnv, child_hook_path);
    }

    const char* configured_port = getenv(kDebugPortEnv);
    if (!configured_port || configured_port[0] == '\0') {
        std::string argument_port;
        if (try_extract_devtools_port_from_args(argument_port)) {
            setenv(kDebugPortEnv, argument_port.c_str(), 1);
        } else {
            setenv(kDebugPortEnv, kDefaultDebugPort, 1);
        }
    }
}

static void ensure_frontend_bridge_prerequisites()
{
    std::string steam_executable_path;
    if (!resolve_current_executable_path(steam_executable_path)) {
        log_proxy("Failed to resolve Steam executable path for frontend bridge setup.");
        return;
    }

    maybe_set_env_if_unset(kSteamExecutableEnv, steam_executable_path);
    if (!ensure_cef_remote_debugging_marker(steam_executable_path.c_str())) {
        log_proxy("Failed to create .cef-enable-remote-debugging marker beside Steam executable.");
    }
}

static void start_millennium()
{
    if (!is_steam_main_process()) {
        return;
    }

    ensure_runtime_environment_defaults();
    ensure_frontend_bridge_prerequisites();

    const char* runtime_path = getenv(kRuntimePathEnv);
    if (!runtime_path || runtime_path[0] == '\0') {
        log_proxy("MILLENNIUM_RUNTIME_PATH is unset and no adjacent runtime was found.");
        return;
    }

    g_millennium_handle = dlopen(runtime_path, RTLD_NOW | RTLD_GLOBAL);
    if (!g_millennium_handle) {
        log_proxyf("Failed to dlopen Millennium runtime", dlerror());
        return;
    }

    auto start_millennium_fn = (start_millennium_t)dlsym(g_millennium_handle, "StartMillennium");
    if (!start_millennium_fn) {
        log_proxyf("Failed to resolve StartMillennium", dlerror());
        dlclose(g_millennium_handle);
        g_millennium_handle = nullptr;
        return;
    }

    const int result = start_millennium_fn();
    if (result < 0) {
        log_proxy("StartMillennium returned failure.");
        dlclose(g_millennium_handle);
        g_millennium_handle = nullptr;
        return;
    }

    g_millennium_started = true;
}

static void stop_millennium()
{
    if (!g_millennium_handle) {
        return;
    }

    if (g_millennium_started) {
        auto stop_millennium_fn = (stop_millennium_t)dlsym(g_millennium_handle, "StopMillennium");
        if (stop_millennium_fn) {
            stop_millennium_fn();
        }
    }

    dlclose(g_millennium_handle);
    g_millennium_handle = nullptr;
    g_millennium_started = false;
}

extern "C" __attribute__((visibility("default"))) const char* millennium_tier0_proxy_marker()
{
    return "millennium-tier0-proxy-v1";
}

__attribute__((constructor)) static void tier0_proxy_init()
{
    std::call_once(g_start_once, start_millennium);
}

__attribute__((destructor)) static void tier0_proxy_cleanup()
{
    stop_millennium();
}
#endif
