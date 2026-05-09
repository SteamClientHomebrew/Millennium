#ifdef __APPLE__
#include <algorithm>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <filesystem>
#include <fstream>
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace
{
constexpr const char* kDefaultDebugPort = "8080";
constexpr const char* kDebugPortEnv = "MILLENNIUM_DEBUG_PORT";
constexpr const char* kRuntimePathEnv = "MILLENNIUM_RUNTIME_PATH";
constexpr const char* kHookHelperPathEnv = "MILLENNIUM_HOOK_HELPER_PATH";
constexpr const char* kChildHookPathEnv = "MILLENNIUM_CHILD_HOOK_PATH";
constexpr const char* kAssetsRootEnv = "MILLENNIUM_ASSETS_ROOT";
constexpr const char* kBrowserContextPathEnv = "MILLENNIUM_BROWSER_CONTEXT_PATH";
constexpr const char* kSteamExecutableEnv = "MILLENNIUM_STEAM_EXECUTABLE";
constexpr const char* kCefRemoteDebuggingMarker = ".cef-enable-remote-debugging";
constexpr const char* kDefaultSteamExecutableSuffix = "/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/steam_osx";
constexpr const char* kBootstrapDirectoryName = "millennium";
constexpr const char* kBootstrapLoaderDirectoryName = "loader";
constexpr const char* kBootstrapChunksDirectoryName = "chunks";
constexpr const char* kBootstrapFrontendModuleName = "millennium-frontend.js";
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

static bool get_executable_path_raw(char* output, size_t output_size)
{
    uint32_t size = static_cast<uint32_t>(output_size);
    if (_NSGetExecutablePath(output, &size) != 0) {
        return false;
    }

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

static std::string reserve_debug_port()
{
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return {};
    }

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close(sock);
        return {};
    }

    socklen_t address_length = sizeof(address);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&address), &address_length) != 0) {
        close(sock);
        return {};
    }

    const auto port = ntohs(address.sin_port);
    close(sock);

    if (port == 0) {
        return {};
    }

    return std::to_string(port);
}

static std::string get_env_value(const char* name)
{
    const char* value = getenv(name);
    return value ? std::string(value) : std::string();
}

static std::string read_http_response_body(const std::string& response)
{
    const size_t headers_end = response.find("\r\n\r\n");
    if (headers_end == std::string::npos) {
        return {};
    }

    return response.substr(headers_end + 4);
}

static std::string extract_browser_context_url(const std::string& response_body)
{
    static constexpr const char* kKey = "\"webSocketDebuggerUrl\"";
    const size_t key_start = response_body.find(kKey);
    if (key_start == std::string::npos) {
        return {};
    }

    size_t cursor = key_start + strlen(kKey);
    while (cursor < response_body.size() && isspace(static_cast<unsigned char>(response_body[cursor]))) {
        ++cursor;
    }

    if (cursor >= response_body.size() || response_body[cursor] != ':') {
        return {};
    }

    ++cursor;
    while (cursor < response_body.size() && isspace(static_cast<unsigned char>(response_body[cursor]))) {
        ++cursor;
    }

    if (cursor >= response_body.size() || response_body[cursor] != '"') {
        return {};
    }

    ++cursor;
    std::string value;
    value.reserve(128);

    bool escaped = false;
    while (cursor < response_body.size()) {
        const char ch = response_body[cursor++];
        if (escaped) {
            value.push_back(ch);
            escaped = false;
            continue;
        }

        if (ch == '\\') {
            escaped = true;
            continue;
        }

        if (ch == '"') {
            return value;
        }

        value.push_back(ch);
    }

    return {};
}

static std::string ensure_browser_context_port(std::string browser_context_url, const char* debug_port)
{
    if (browser_context_url.empty() || !debug_port || debug_port[0] == '\0') {
        return browser_context_url;
    }

    const size_t scheme_end = browser_context_url.find("://");
    if (scheme_end == std::string::npos) {
        return browser_context_url;
    }

    const size_t authority_start = scheme_end + 3;
    const size_t path_start = browser_context_url.find('/', authority_start);
    const size_t port_separator = browser_context_url.find(':', authority_start);

    if (port_separator != std::string::npos && (path_start == std::string::npos || port_separator < path_start)) {
        return browser_context_url;
    }

    if (path_start == std::string::npos) {
        browser_context_url.append(":");
        browser_context_url.append(debug_port);
        return browser_context_url;
    }

    browser_context_url.insert(path_start, ":");
    browser_context_url.insert(path_start + 1, debug_port);
    return browser_context_url;
}

static std::string fetch_browser_context_once(const char* debug_port)
{
    if (!debug_port || debug_port[0] == '\0') {
        return {};
    }

    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return {};
    }

    timeval timeout = {};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(strtoul(debug_port, nullptr, 10)));
    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1) {
        close(fd);
        return {};
    }

    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close(fd);
        return {};
    }

    const std::string request = std::string("GET /json/version HTTP/1.1\r\nHost: 127.0.0.1:") + debug_port + "\r\nConnection: close\r\n\r\n";

    if (send(fd, request.c_str(), request.size(), 0) < 0) {
        close(fd);
        return {};
    }

    std::string response;
    char buffer[4096];
    while (true) {
        const ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0) {
            break;
        }

        response.append(buffer, static_cast<size_t>(bytes_read));
    }

    close(fd);
    return ensure_browser_context_port(extract_browser_context_url(read_http_response_body(response)), debug_port);
}

static void start_browser_context_probe()
{
    const std::string debug_port = get_env_value(kDebugPortEnv);
    if (debug_port.empty()) {
        return;
    }

    char temp_path[] = "/tmp/millennium-browser-context-XXXXXX";
    const int temp_fd = mkstemp(temp_path);
    if (temp_fd < 0) {
        return;
    }
    close(temp_fd);
    unlink(temp_path);

    setenv(kBrowserContextPathEnv, temp_path, 1);

    const pid_t child_pid = fork();
    if (child_pid != 0) {
        return;
    }

    unsetenv("DYLD_INSERT_LIBRARIES");

    for (size_t attempt = 0; attempt < 120; ++attempt) {
        const std::string browser_context_url = fetch_browser_context_once(debug_port.c_str());
        if (!browser_context_url.empty()) {
            const int output_fd = open(temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (output_fd >= 0) {
                const auto bytes_written = write(output_fd, browser_context_url.c_str(), browser_context_url.size());
                close(output_fd);
                if (bytes_written == static_cast<ssize_t>(browser_context_url.size())) {
                    _exit(0);
                }
            }

            unlink(temp_path);
            _exit(1);
        }

        usleep(250000);
    }

    unlink(temp_path);
    _exit(0);
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
        return;
    }

    const std::string reserved_port = reserve_debug_port();
    if (!reserved_port.empty()) {
        setenv(kDebugPortEnv, reserved_port.c_str(), 1);
        return;
    }

    setenv(kDebugPortEnv, kDefaultDebugPort, 1);
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

static bool ensure_directory_exists(const std::filesystem::path& path)
{
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
        return !error;
    }

    std::filesystem::create_directories(path, error);
    return !error;
}

static bool read_text_file(const std::filesystem::path& path, std::string* output)
{
    if (!output) {
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    output->assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return true;
}

static bool write_text_file(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(output);
}

static bool sync_text_file(const std::filesystem::path& source, const std::filesystem::path& target)
{
    std::string source_content;
    if (!read_text_file(source, &source_content)) {
        return false;
    }

    std::string existing_content;
    if (read_text_file(target, &existing_content) && existing_content == source_content) {
        return true;
    }

    return write_text_file(target, source_content);
}

static bool resolve_bootstrap_asset_sources(const char* wrapper_directory, std::filesystem::path* loader_source_dir, std::filesystem::path* frontend_source)
{
    if (!wrapper_directory || !loader_source_dir || !frontend_source) {
        return false;
    }

    auto has_valid_layout = [](const std::filesystem::path& candidate_root, std::filesystem::path* resolved_loader_dir, std::filesystem::path* resolved_frontend) -> bool
    {
        const std::filesystem::path loader_dir = candidate_root / "loader";
        const std::filesystem::path loader_entry = loader_dir / "millennium.js";
        const std::filesystem::path chunks_dir = loader_dir / "chunks";
        const std::filesystem::path frontend_module = candidate_root / kBootstrapFrontendModuleName;
        const std::filesystem::path frontend_binary = candidate_root / "frontend.bin";

        std::error_code error;
        const bool has_loader_entry = std::filesystem::is_regular_file(loader_entry, error) && !error;
        error.clear();
        const bool has_chunks_dir = std::filesystem::is_directory(chunks_dir, error) && !error;
        error.clear();
        const bool has_frontend_module = std::filesystem::is_regular_file(frontend_module, error) && !error;
        error.clear();
        const bool has_frontend_binary = std::filesystem::is_regular_file(frontend_binary, error) && !error;

        if (!has_loader_entry || !has_chunks_dir || (!has_frontend_module && !has_frontend_binary)) {
            return false;
        }

        *resolved_loader_dir = loader_dir;
        *resolved_frontend = has_frontend_module ? frontend_module : frontend_binary;
        return true;
    };

    std::vector<std::filesystem::path> candidates;
    const char* configured_assets_root = getenv(kAssetsRootEnv);
    if (configured_assets_root && configured_assets_root[0] != '\0') {
        candidates.emplace_back(configured_assets_root);
    }

    candidates.emplace_back(std::filesystem::path(wrapper_directory) / ".." / "Resources" / "millennium-assets");

    for (const std::filesystem::path& candidate : candidates) {
        std::error_code canonical_error;
        const std::filesystem::path resolved_candidate = std::filesystem::weakly_canonical(candidate, canonical_error);
        if (has_valid_layout(canonical_error ? candidate : resolved_candidate, loader_source_dir, frontend_source)) {
            return true;
        }
    }

#ifdef MILLENNIUM_ROOT
    const std::filesystem::path repo_root(MILLENNIUM_ROOT);
    const std::filesystem::path legacy_loader_dir = repo_root / "src" / "typescript" / "sdk" / "packages" / "loader" / "build";
    const std::filesystem::path legacy_frontend_candidates[] = {
        repo_root / "src" / "typescript" / ".frontend.bin",
        repo_root / "build" / "frontend.bin",
    };

    std::error_code legacy_error;
    if (std::filesystem::is_regular_file(legacy_loader_dir / "millennium.js", legacy_error) && !legacy_error &&
        std::filesystem::is_directory(legacy_loader_dir / "chunks", legacy_error) && !legacy_error) {
        for (const auto& candidate : legacy_frontend_candidates) {
            legacy_error.clear();
            if (std::filesystem::is_regular_file(candidate, legacy_error) && !legacy_error) {
                *loader_source_dir = legacy_loader_dir;
                *frontend_source = candidate;
                return true;
            }
        }
    }
#endif

    return false;
}

static bool prepare_macos_bootstrap_assets(const char* steam_executable_path)
{
    const char* separator = strrchr(steam_executable_path, '/');
    if (!separator) {
        return false;
    }

    const std::filesystem::path steam_root(std::string(steam_executable_path, static_cast<size_t>(separator - steam_executable_path)));
    const std::filesystem::path steamui_root = steam_root / "steamui";
    const std::filesystem::path bootstrap_root = steamui_root / kBootstrapDirectoryName;
    const std::filesystem::path loader_target_dir = bootstrap_root / kBootstrapLoaderDirectoryName;
    const std::filesystem::path chunks_target_dir = loader_target_dir / kBootstrapChunksDirectoryName;

    if (!ensure_directory_exists(chunks_target_dir)) {
        return false;
    }

    char executable_path[PATH_MAX];
    if (!get_executable_path_raw(executable_path, sizeof(executable_path))) {
        return false;
    }

    char wrapper_directory_buffer[PATH_MAX];
    strncpy(wrapper_directory_buffer, executable_path, sizeof(wrapper_directory_buffer) - 1);
    wrapper_directory_buffer[sizeof(wrapper_directory_buffer) - 1] = '\0';

    const char* wrapper_directory = dirname(wrapper_directory_buffer);

    std::filesystem::path loader_source_dir;
    std::filesystem::path frontend_source;
    if (!resolve_bootstrap_asset_sources(wrapper_directory, &loader_source_dir, &frontend_source)) {
        return false;
    }

    if (!sync_text_file(loader_source_dir / "millennium.js", loader_target_dir / "millennium.js")) {
        return false;
    }

    const std::filesystem::path loader_chunks_dir = loader_source_dir / "chunks";
    std::error_code directory_error;
    if (!std::filesystem::exists(loader_chunks_dir, directory_error) || directory_error) {
        return false;
    }

    std::vector<std::filesystem::path> chunk_files;
    for (std::filesystem::directory_iterator it(loader_chunks_dir, directory_error), end; !directory_error && it != end; it.increment(directory_error)) {
        const std::filesystem::directory_entry& entry = *it;
        if (!entry.is_regular_file(directory_error) || directory_error) {
            continue;
        }

        const std::filesystem::path chunk_path = entry.path();
        const std::string chunk_name = chunk_path.filename().string();
        if (chunk_name.rfind("chunk-", 0) != 0 || chunk_path.extension() != ".js") {
            continue;
        }

        chunk_files.push_back(chunk_path);
    }

    if (directory_error || chunk_files.empty()) {
        return false;
    }

    std::sort(chunk_files.begin(), chunk_files.end(), [](const std::filesystem::path& lhs, const std::filesystem::path& rhs)
    {
        return lhs.filename().string() < rhs.filename().string();
    });

    for (const std::filesystem::path& chunk_path : chunk_files) {
        if (!sync_text_file(chunk_path, chunks_target_dir / chunk_path.filename())) {
            return false;
        }
    }

    if (!sync_text_file(frontend_source, bootstrap_root / kBootstrapFrontendModuleName)) {
        return false;
    }

    return true;
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

    start_browser_context_probe();

    if (!ensure_cef_remote_debugging_marker(target_steam_path)) {
        fprintf(stderr, "[Millennium] Failed to create %s beside the Steam runtime.\n", kCefRemoteDebuggingMarker);
        return 1;
    }

    if (!prepare_macos_bootstrap_assets(target_steam_path)) {
        remove_cef_remote_debugging_marker(target_steam_path);
        fprintf(stderr, "[Millennium] Failed to prepare macOS bootstrap assets.\n");
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
