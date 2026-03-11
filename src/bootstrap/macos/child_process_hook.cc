#ifdef __APPLE__
#include <crt_externs.h>
#include <dlfcn.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace
{
constexpr const char* kHookHelperPathEnv = "MILLENNIUM_HOOK_HELPER_PATH";
constexpr const char* kSteamExecutableEnv = "MILLENNIUM_STEAM_EXECUTABLE";
constexpr const char* kCefRemoteDebuggingMarker = ".cef-enable-remote-debugging";
} // namespace

static bool get_current_executable(char* output, size_t output_size)
{
    uint32_t size = static_cast<uint32_t>(output_size);
    if (_NSGetExecutablePath(output, &size) != 0) {
        return false;
    }

    char resolved[PATH_MAX];
    if (!realpath(output, resolved)) {
        return false;
    }

    strncpy(output, resolved, output_size - 1);
    output[output_size - 1] = '\0';
    return true;
}

static bool is_steam_helper_process()
{
    char executable_path[PATH_MAX];
    if (!get_current_executable(executable_path, sizeof(executable_path))) {
        return false;
    }

    const char* filename = strrchr(executable_path, '/');
    filename = filename ? filename + 1 : executable_path;

    constexpr const char* kSteamHelperPrefix = "Steam Helper";
    constexpr size_t kSteamHelperPrefixLength = sizeof("Steam Helper") - 1;
    if (strncmp(filename, kSteamHelperPrefix, kSteamHelperPrefixLength) != 0) {
        return false;
    }

    const char suffix = filename[kSteamHelperPrefixLength];
    return suffix == '\0' || suffix == ' ' || suffix == '(';
}

static const char* get_process_type_argument(char* const argv[])
{
    if (!argv) {
        return nullptr;
    }

    for (size_t index = 1; argv[index] != nullptr; ++index) {
        if (strncmp(argv[index], "--type=", 7) == 0) {
            return argv[index] + 7;
        }
    }

    return nullptr;
}

static bool is_crashpad_helper(char* const argv[])
{
    const char* process_type = get_process_type_argument(argv);
    return process_type && strcmp(process_type, "crashpad-handler") == 0;
}

static bool is_top_level_helper(char* const argv[])
{
    return get_process_type_argument(argv) == nullptr;
}

static void remove_cef_remote_debugging_marker()
{
    const char* steam_executable = getenv(kSteamExecutableEnv);
    if (!steam_executable || steam_executable[0] == '\0') {
        return;
    }

    const char* separator = strrchr(steam_executable, '/');
    if (!separator) {
        return;
    }

    char marker_path[PATH_MAX];
    const size_t directory_length = static_cast<size_t>(separator - steam_executable);
    if (directory_length + 1 + strlen(kCefRemoteDebuggingMarker) + 1 > sizeof(marker_path)) {
        return;
    }

    memcpy(marker_path, steam_executable, directory_length);
    marker_path[directory_length] = '/';
    strcpy(marker_path + directory_length + 1, kCefRemoteDebuggingMarker);

    unlink(marker_path);
}

static bool resolve_hook_helper_path(char* output, size_t output_size)
{
    const char* configured = getenv(kHookHelperPathEnv);
    if (!configured || configured[0] == '\0') {
        return false;
    }

    if (access(configured, R_OK) != 0) {
        return false;
    }

    char resolved[PATH_MAX];
    if (!realpath(configured, resolved)) {
        return false;
    }

    strncpy(output, resolved, output_size - 1);
    output[output_size - 1] = '\0';
    return true;
}

__attribute__((constructor)) static void child_hook_init()
{
    if (!is_steam_helper_process()) {
        return;
    }

    char** current_argv = _NSGetArgv() ? *_NSGetArgv() : nullptr;
    if (is_crashpad_helper(current_argv)) {
        return;
    }

    if (is_top_level_helper(current_argv)) {
        remove_cef_remote_debugging_marker();
    }

    char hook_helper_path[PATH_MAX];
    if (!resolve_hook_helper_path(hook_helper_path, sizeof(hook_helper_path))) {
        fprintf(stderr, "[Millennium] Failed to resolve helper hook path for Steam Helper.\n");
        return;
    }

    if (!dlopen(hook_helper_path, RTLD_NOW | RTLD_GLOBAL)) {
        fprintf(stderr, "[Millennium] Failed to load helper hook library: %s\n", dlerror());
    }
}
#endif
