#ifdef __APPLE__
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <vector>

namespace
{
constexpr const char* kDefaultDebugPort = "8080";
constexpr const char* kDebugPortEnv = "MILLENNIUM_DEBUG_PORT";
constexpr const char* kRuntimePathEnv = "MILLENNIUM_RUNTIME_PATH";
constexpr const char* kHookHelperPathEnv = "MILLENNIUM_HOOK_HELPER_PATH";
constexpr const char* kChildHookPathEnv = "MILLENNIUM_CHILD_HOOK_PATH";
constexpr const char* kSteamExecutableEnv = "MILLENNIUM_STEAM_EXECUTABLE";
constexpr const char* kCefRemoteDebuggingMarker = ".cef-enable-remote-debugging";
constexpr const char* kDefaultSteamExecutableSuffix = "/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/steam_osx";
} // namespace

static bool get_executable_path(char* output, size_t output_size)
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

static bool has_argument(int argc, char** argv, const char* target)
{
    for (int index = 1; index < argc; ++index) {
        if (argv[index] && strcmp(argv[index], target) == 0) {
            return true;
        }
    }

    return false;
}

static bool has_prefixed_argument(int argc, char** argv, const char* prefix)
{
    const size_t prefix_length = strlen(prefix);
    for (int index = 1; index < argc; ++index) {
        if (!argv[index]) {
            continue;
        }

        if (strncmp(argv[index], prefix, prefix_length) == 0) {
            return true;
        }
    }

    return false;
}

static const char* get_argument_value(int argc, char** argv, const char* option)
{
    const size_t option_length = strlen(option);

    for (int index = 1; index < argc; ++index) {
        if (!argv[index]) {
            continue;
        }

        if (strcmp(argv[index], option) == 0) {
            if (index + 1 < argc && argv[index + 1] && argv[index + 1][0] != '\0') {
                return argv[index + 1];
            }
            return nullptr;
        }

        if (strncmp(argv[index], option, option_length) == 0 && argv[index][option_length] == '=') {
            const char* value = argv[index] + option_length + 1;
            return value[0] == '\0' ? nullptr : value;
        }
    }

    return nullptr;
}

static bool resolve_existing_path(const char* candidate, char* output, size_t output_size, int access_mode)
{
    if (!candidate || candidate[0] == '\0') {
        return false;
    }

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

static bool ensure_adjacent_artifact_env(const char* env_name, const char* directory, const char* artifact_name)
{
    const char* existing_value = getenv(env_name);
    if (existing_value && existing_value[0] != '\0') {
        char resolved[PATH_MAX];
        if (!resolve_existing_path(existing_value, resolved, sizeof(resolved), R_OK)) {
            return false;
        }

        setenv(env_name, resolved, 1);
        return true;
    }

    char candidate[PATH_MAX];
    snprintf(candidate, sizeof(candidate), "%s/%s", directory, artifact_name);

    char resolved[PATH_MAX];
    if (!resolve_existing_path(candidate, resolved, sizeof(resolved), R_OK)) {
        return false;
    }

    setenv(env_name, resolved, 1);
    return true;
}

static bool resolve_steam_target(const char* wrapper_directory, char* output, size_t output_size)
{
    const char* configured_path = getenv(kSteamExecutableEnv);
    if (resolve_existing_path(configured_path, output, output_size, X_OK)) {
        return true;
    }

    char legacy_runtime_path[PATH_MAX];
    snprintf(legacy_runtime_path, sizeof(legacy_runtime_path), "%s/steam_osx.real", wrapper_directory);
    if (resolve_existing_path(legacy_runtime_path, output, output_size, X_OK)) {
        return true;
    }

    const char* home = getenv("HOME");
    if (!home || home[0] == '\0') {
        return false;
    }

    char default_runtime_path[PATH_MAX];
    snprintf(default_runtime_path, sizeof(default_runtime_path), "%s%s", home, kDefaultSteamExecutableSuffix);
    return resolve_existing_path(default_runtime_path, output, output_size, X_OK);
}

static void configure_debug_environment(int argc, char** argv)
{
    const bool is_dev_mode = has_argument(argc, argv, "-dev");
    const char* explicit_port = get_argument_value(argc, argv, "-devtools-port");
    if (explicit_port && explicit_port[0] != '\0') {
        setenv(kDebugPortEnv, explicit_port, 1);
        return;
    }

    const char* existing_port = getenv(kDebugPortEnv);
    if (existing_port && existing_port[0] != '\0') {
        return;
    }

    if (is_dev_mode) {
        setenv(kDebugPortEnv, kDefaultDebugPort, 1);
    }
}

static bool ensure_cef_remote_debugging_marker(const char* steam_executable_path)
{
    const char* separator = strrchr(steam_executable_path, '/');
    if (!separator) {
        return false;
    }

    const std::string steam_directory(steam_executable_path, static_cast<size_t>(separator - steam_executable_path));
    const std::string marker_path = steam_directory + "/" + kCefRemoteDebuggingMarker;

    FILE* marker = fopen(marker_path.c_str(), "a");
    if (!marker) {
        return false;
    }

    fclose(marker);
    return true;
}

static void remove_cef_remote_debugging_marker(const char* steam_executable_path)
{
    const char* separator = strrchr(steam_executable_path, '/');
    if (!separator) {
        return;
    }

    const std::string steam_directory(steam_executable_path, static_cast<size_t>(separator - steam_executable_path));
    const std::string marker_path = steam_directory + "/" + kCefRemoteDebuggingMarker;
    unlink(marker_path.c_str());
}

static std::vector<char*> build_launch_arguments(const char* target_steam_path, int argc, char** argv, std::vector<std::string>& owned_arguments)
{
    owned_arguments.clear();
    owned_arguments.reserve(static_cast<size_t>(argc) + 3);
    owned_arguments.emplace_back(target_steam_path);

    for (int index = 1; index < argc; ++index) {
        if (argv[index]) {
            owned_arguments.emplace_back(argv[index]);
        }
    }

    const bool has_devtools_flag =
        get_argument_value(argc, argv, "-devtools-port") != nullptr || has_argument(argc, argv, "-devtools-port") || has_prefixed_argument(argc, argv, "-devtools-port=");
    if (!has_devtools_flag) {
        const char* debug_port = getenv(kDebugPortEnv);
        if (debug_port && debug_port[0] != '\0') {
            owned_arguments.emplace_back("-devtools-port");
            owned_arguments.emplace_back(debug_port);
        }
    }

    std::vector<char*> launch_arguments;
    launch_arguments.reserve(owned_arguments.size() + 1);
    for (std::string& argument : owned_arguments) {
        launch_arguments.push_back(argument.data());
    }
    launch_arguments.push_back(nullptr);
    return launch_arguments;
}

static std::string merge_injected_libraries(const char* bootstrap_path)
{
    const char* existing_value = getenv("DYLD_INSERT_LIBRARIES");
    if (!existing_value || !*existing_value) {
        return std::string(bootstrap_path);
    }

    std::string existing(existing_value);
    size_t start = 0;
    while (start <= existing.size()) {
        const size_t end = existing.find(':', start);
        const std::string entry = existing.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const char* normalized_entry = entry.c_str();
        char resolved_entry[PATH_MAX];

        if (!entry.empty() && realpath(entry.c_str(), resolved_entry)) {
            normalized_entry = resolved_entry;
        }

        if (strcmp(normalized_entry, bootstrap_path) == 0) {
            return existing;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    return std::string(bootstrap_path) + ":" + existing;
}

int main(int argc, char** argv)
{
    char current_path[PATH_MAX];
    if (!get_executable_path(current_path, sizeof(current_path))) {
        fprintf(stderr, "[Millennium] Failed to resolve wrapper path.\n");
        return 1;
    }

    char directory_buffer[PATH_MAX];
    strncpy(directory_buffer, current_path, sizeof(directory_buffer) - 1);
    directory_buffer[sizeof(directory_buffer) - 1] = '\0';

    char* directory = dirname(directory_buffer);

    char bootstrap_path[PATH_MAX];
    snprintf(bootstrap_path, sizeof(bootstrap_path), "%s/%s", directory, __MILLENNIUM_BOOTLOADER_OUTPUT_NAME__);

    if (access(bootstrap_path, R_OK) != 0) {
        fprintf(stderr, "[Millennium] Missing bootstrap library at %s\n", bootstrap_path);
        return 1;
    }

    if (!ensure_adjacent_artifact_env(kRuntimePathEnv, directory, __MILLENNIUM_OUTPUT_NAME__)) {
        const char* configured_runtime = getenv(kRuntimePathEnv);
        if (!configured_runtime || configured_runtime[0] == '\0') {
            fprintf(stderr, "[Millennium] Missing Millennium runtime path. Set %s or place %s beside the launcher.\n", kRuntimePathEnv, __MILLENNIUM_OUTPUT_NAME__);
            return 1;
        }
    }

    if (!ensure_adjacent_artifact_env(kHookHelperPathEnv, directory, __MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__)) {
        const char* configured_hhx = getenv(kHookHelperPathEnv);
        if (!configured_hhx || configured_hhx[0] == '\0') {
            fprintf(stderr, "[Millennium] Missing helper hook path. Set %s or place %s beside the launcher.\n", kHookHelperPathEnv, __MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__);
            return 1;
        }
    }

    if (!ensure_adjacent_artifact_env(kChildHookPathEnv, directory, __MILLENNIUM_CHILD_HOOK_OUTPUT_NAME__)) {
        const char* configured_child_hook = getenv(kChildHookPathEnv);
        if (!configured_child_hook || configured_child_hook[0] == '\0') {
            fprintf(stderr, "[Millennium] Missing child hook path. Set %s or place %s beside the launcher.\n", kChildHookPathEnv, __MILLENNIUM_CHILD_HOOK_OUTPUT_NAME__);
            return 1;
        }
    }

    char target_steam_path[PATH_MAX];
    if (!resolve_steam_target(directory, target_steam_path, sizeof(target_steam_path))) {
        fprintf(stderr, "[Millennium] Failed to locate the real Steam runtime. Set %s or reinstall Steam.\n", kSteamExecutableEnv);
        return 1;
    }
    setenv(kSteamExecutableEnv, target_steam_path, 1);

    configure_debug_environment(argc, argv);

    if (getenv(kDebugPortEnv) && !ensure_cef_remote_debugging_marker(target_steam_path)) {
        fprintf(stderr, "[Millennium] Failed to create %s beside the Steam runtime.\n", kCefRemoteDebuggingMarker);
        return 1;
    }

    const std::string dyld_insert_libraries = merge_injected_libraries(bootstrap_path);
    setenv("DYLD_INSERT_LIBRARIES", dyld_insert_libraries.c_str(), 1);

    std::vector<std::string> owned_arguments;
    std::vector<char*> launch_arguments = build_launch_arguments(target_steam_path, argc, argv, owned_arguments);
    execv(target_steam_path, launch_arguments.data());

    remove_cef_remote_debugging_marker(target_steam_path);
    fprintf(stderr, "[Millennium] Failed to launch Steam: %s\n", strerror(errno));
    return 1;
}
#endif
