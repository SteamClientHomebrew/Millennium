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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <string_view>

#ifdef __linux__
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "millennium/filesystem.h" // IWYU pragma: keep

#include "millennium/logger.h"
#include "millennium/steam_hooks.h"
#include "millennium/cmdline_api.h"
#include "millennium/cmdline_parser.h"
#include "millennium/millennium_lifecycle.h"

std::mutex g_cdp_pipe_mutex;
std::condition_variable g_cdp_pipe_cv;
std::atomic<bool> g_cdp_pipes_ready{ false };
int g_cdp_pipe_generation = 0;

#ifdef _WIN32
HANDLE g_cdp_pipe_read = INVALID_HANDLE_VALUE;
HANDLE g_cdp_pipe_write = INVALID_HANDLE_VALUE;
#elif __linux__
int g_cdp_pipe_read_fd = -1;
int g_cdp_pipe_write_fd = -1;
int g_cdp_pipe_change_efd = -1;
#endif

#ifdef __linux__
static std::string g_pv_shim_dir;
static std::string g_cdp_cmd_fifo;
static std::string g_cdp_resp_fifo;

static std::string find_pvs64_binary()
{
#ifdef __PVS64_OUTPUT_ABSPATH__
    return __PVS64_OUTPUT_ABSPATH__;
#else
    return (platform::get_millennium_bin_path() / "libmillennium_pvs64").string();
#endif
}

static void cleanup_pv_shim()
{
    if (g_pv_shim_dir.empty()) {
        return;
    }

    unlink((g_pv_shim_dir + "/bin/pressure-vessel-unruntime").c_str());
    rmdir((g_pv_shim_dir + "/bin").c_str());

    /* fifo's live inside shim_dir; unlink in case pv-shim didn't. */
    if (!g_cdp_cmd_fifo.empty()) {
        unlink(g_cdp_cmd_fifo.c_str());
        g_cdp_cmd_fifo.clear();
    }

    if (!g_cdp_resp_fifo.empty()) {
        unlink(g_cdp_resp_fifo.c_str());
        g_cdp_resp_fifo.clear();
    }

    rmdir(g_pv_shim_dir.c_str());
    g_pv_shim_dir.clear();
}

static bool create_pv_shim()
{
    cleanup_pv_shim();

    int command_fd = -1;
    int response_fd = -1;

    char tmp[] = "/tmp/.millennium-pv-XXXXXX";
    const char* mkd_temp_directory = mkdtemp(tmp);

    if (!mkd_temp_directory) {
        LOG_ERROR("create_pv_shim: mkdtemp failed (errno {})", errno);
        return false;
    }

    std::string shim_directory = mkd_temp_directory;
    std::string binary_directory = shim_directory + "/bin";
    std::string command_fifo_directory = shim_directory + "/cmd.fifo";
    std::string response_fifo_directory = shim_directory + "/resp.fifo";

    if (mkdir(binary_directory.c_str(), 0755) < 0) {
        goto cleanup;
    }

    if (mkfifo(command_fifo_directory.c_str(), 0600) < 0 || mkfifo(response_fifo_directory.c_str(), 0600) < 0) {
        LOG_ERROR("create_pv_shim: mkfifo failed (errno {})", errno);
        goto cleanup;
    }

    command_fd = open(command_fifo_directory.c_str(), O_RDWR | O_CLOEXEC);
    response_fd = open(response_fifo_directory.c_str(), O_RDWR | O_CLOEXEC);

    if (command_fd < 0 || response_fd < 0) {
        LOG_ERROR("create_pv_shim: open FIFO failed (errno {})", errno);
        goto cleanup;
    }

    g_cdp_pipe_write_fd = command_fd;
    g_cdp_pipe_read_fd = response_fd;
    g_cdp_cmd_fifo = command_fifo_directory;
    g_cdp_resp_fifo = response_fifo_directory;
    {
        std::string pvs64 = find_pvs64_binary();
        if (pvs64.empty()) {
            LOG_ERROR("create_pv_shim: libmillennium_pvs64 binary not found");
            goto cleanup;
        }

        std::string shim_bin = binary_directory + "/pressure-vessel-unruntime";

        if (symlink(pvs64.c_str(), shim_bin.c_str()) < 0) {
            LOG_ERROR("create_pv_shim: symlink {} → {} failed (errno {})", pvs64, shim_bin, errno);
            goto cleanup;
        }
    }
    g_pv_shim_dir = shim_directory;
    return true;

cleanup:
    if (command_fd >= 0) close(command_fd);
    if (response_fd >= 0) close(response_fd);

    unlink(command_fifo_directory.c_str());
    unlink(response_fifo_directory.c_str());
    rmdir(binary_directory.c_str());
    rmdir(mkd_temp_directory);

    return false;
}
#endif /* __linux__ */

const char* Plat_HookedCreateSimpleProcess(const char* cmd)
{
    if (!cmd) {
        LOG_ERROR("Plat_HookedCreateSimpleProcess: received null cmd");
        return cmd;
    }

    logger.log("Plat_HookedCreateSimpleProcess: cmd = {}", cmd);
    command cmd_line(cmd);

    const char* target_executable =
#ifdef _WIN32
        "steamwebhelper.exe";
#elif __linux__
        "steamwebhelper.sh";
#elif __APPLE__
        "Steam Helper";
#endif

    if (cmd_line.executable() != target_executable) {
        logger.log("dispatching: {}", cmd_line.exec);
        return cmd;
    }

    if (!millennium_lifecycle::get().backends_loaded.flag.load()) {
        millennium_lifecycle::get().backends_loaded.wait();
    }

    bool is_developer_mode = CommandLineArguments::has_argument("-dev");

    cmd_line.ensure_param("--enable-unsafe-extension-debugging");
    cmd_line.ensure_param("--disable-blink-features", "AutomationControlled");

    if (is_developer_mode) {
        cmd_line.ensure_param("--remote-allow-origins", "*");
        cmd_line.ensure_param("--remote-debugging-address", "127.0.0.1");
        cmd_line.ensure_param("--remote-debugging-port", DEFAULT_DEVTOOLS_PORT);
    }

#ifdef _WIN32
    {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };

        HANDLE hChildRead = INVALID_HANDLE_VALUE, hParentWrite = INVALID_HANDLE_VALUE;
        HANDLE hParentRead = INVALID_HANDLE_VALUE, hChildWrite = INVALID_HANDLE_VALUE;

        bool pipes_ok = CreatePipe(&hChildRead, &hParentWrite, &sa, 0) && CreatePipe(&hParentRead, &hChildWrite, &sa, 0);

        if (pipes_ok) {
            /** keep only child-side handles inheritable */
            SetHandleInformation(hParentWrite, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(hParentRead, HANDLE_FLAG_INHERIT, 0);

            cmd_line.ensure_param("--remote-debugging-pipe");
            cmd_line.ensure_param("--remote-debugging-io-pipes", std::format("{},{}", reinterpret_cast<uintptr_t>(hChildRead), reinterpret_cast<uintptr_t>(hChildWrite)).c_str());

            g_cdp_pipe_read = hParentRead;
            g_cdp_pipe_write = hParentWrite;

            logger.log("CDP pipes created (child read={}, child write={}, parent read={}, parent write={})", reinterpret_cast<uintptr_t>(hChildRead),
                       reinterpret_cast<uintptr_t>(hChildWrite), reinterpret_cast<uintptr_t>(hParentRead), reinterpret_cast<uintptr_t>(hParentWrite));

            {
                std::lock_guard<std::mutex> lock(g_cdp_pipe_mutex);
                g_cdp_pipe_generation++;
                g_cdp_pipes_ready = true;
            }
            g_cdp_pipe_cv.notify_all();
        } else {
            LOG_ERROR("Failed to create CDP pipes (error {}), falling back to port-only debugging.", GetLastError());
            if (hChildRead != INVALID_HANDLE_VALUE) CloseHandle(hChildRead);
            if (hParentWrite != INVALID_HANDLE_VALUE) CloseHandle(hParentWrite);
        }
    }
#elif __linux__
    {
        cmd_line.ensure_param("--remote-debugging-pipe");

        if (create_pv_shim()) {
            cmd_line.params.insert(cmd_line.params.begin(), cmd_line.exec);
            cmd_line.params.insert(cmd_line.params.begin(), "PRESSURE_VESSEL_PREFIX=" + g_pv_shim_dir);
            cmd_line.exec = "/usr/bin/env";

            if (g_cdp_pipe_change_efd < 0) {
                g_cdp_pipe_change_efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
            } else {
                /** signal pipe_read_loop to exit, a new pipe has replaced the old one */
                const uint64_t val = 1;
                [[maybe_unused]] ssize_t _ = write(g_cdp_pipe_change_efd, &val, sizeof(val));
            }
        }
        {
            std::lock_guard<std::mutex> lock(g_cdp_pipe_mutex);
            g_cdp_pipe_generation++;
            g_cdp_pipes_ready = true;
        }
        g_cdp_pipe_cv.notify_all();
    }
#endif

    static thread_local std::string result;
    result = cmd_line.build();
    return result.c_str();
}

#ifdef _WIN32
#define SNARE_STATIC
#define SNARE_IMPLEMENTATION
#include <libsnare.h>

#include "millennium/argp_win32.h"
#include "millennium/logger.h"
#include "millennium/plat_msg.h"

#define LDR_DLL_NOTIFICATION_REASON_LOADED 1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2

LdrRegisterDllNotification_t LdrRegisterDllNotification = nullptr;
LdrUnregisterDllNotification_t LdrUnregisterDllNotification = nullptr;
PVOID g_notification_cookie = nullptr;

using CreateProcessInternalW_t = BOOL(WINAPI*)(HANDLE, LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW,
                                               LPPROCESS_INFORMATION, PHANDLE);

static snare_inline_t g_create_hook = nullptr;
static snare_inline_t g_rdcw_hook = nullptr;
static snare_inline_t g_create_process_internal_hook = nullptr;
static std::once_flag g_tier0_hook_once;

HMODULE steam_tier0_module;

INT hooked_create_simple_process(const char* a1, char a2, const char* lp_multi_byte_str)
{
    auto orig = reinterpret_cast<INT(__cdecl*)(const char*, char, const char*)>(snare_inline_get_trampoline(g_create_hook));
    return orig(Plat_HookedCreateSimpleProcess(a1), a2, lp_multi_byte_str);
}

BOOL WINAPI hooked_create_process_internal_w(HANDLE hUserToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                             LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
                                             LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation, PHANDLE hNewToken)
{
    auto orig = reinterpret_cast<CreateProcessInternalW_t>(snare_inline_get_trampoline(g_create_process_internal_hook));

    if (g_cdp_pipes_ready.load(std::memory_order_acquire) && lpCommandLine) {
        std::wstring cmd(lpCommandLine);
        if (cmd.find(L"steamwebhelper") != std::wstring::npos) {
            BOOL prev = bInheritHandles;
            bInheritHandles = TRUE;
            logger.log("CreateProcessInternalW hook fired for steamwebhelper (bInheritHandles: {} -> TRUE, dwCreationFlags: 0x{:X})", prev, dwCreationFlags);
        }
    }

    return orig(hUserToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory,
                lpStartupInfo, lpProcessInformation, hNewToken);
}

/**
 * tier0_s.dll is a *cross platform* library bundled with Steam that helps it
 * manage low-level system interactions and provides various utility functions.
 *
 * It houses various functions, and we are interested in hooking its functions Steam
 * uses to spawn the Steam web helper.
 *
 * Guarded by std::once_flag because the DLL notification callback and the
 * already-loaded check can both fire for the same module.  Without the guard,
 * a second call would leak the first hook object and double-hook the function.
 */
VOID handle_tier0_dll(PVOID module_base_address)
{
    std::call_once(g_tier0_hook_once, [&]()
    {
        steam_tier0_module = static_cast<HMODULE>(module_base_address);
        logger.log("Setting up hooks for tier0_s.dll");

        FARPROC proc = GetProcAddress(steam_tier0_module, "CreateSimpleProcess");
        if (proc != nullptr) {
            g_create_hook = snare_inline_new(reinterpret_cast<void*>(proc), reinterpret_cast<void*>(&hooked_create_simple_process));
            if (!g_create_hook || snare_inline_install(g_create_hook) < 0) {
                platform::messagebox::show("Millennium", "Failed to create hook for CreateSimpleProcess", platform::messagebox::error);
                return;
            }
        }

        {
            HMODULE kernelbase = GetModuleHandleW(L"kernelbase.dll");
            FARPROC cpi_proc = kernelbase ? GetProcAddress(kernelbase, "CreateProcessInternalW") : nullptr;
            if (!cpi_proc) {
                HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
                cpi_proc = kernel32 ? GetProcAddress(kernel32, "CreateProcessInternalW") : nullptr;
            }

            if (cpi_proc) {
                g_create_process_internal_hook = snare_inline_new(reinterpret_cast<void*>(cpi_proc), reinterpret_cast<void*>(&hooked_create_process_internal_w));
                if (g_create_process_internal_hook) {
                    int hook_result = snare_inline_install(g_create_process_internal_hook);
                    if (hook_result < 0) {
                        LOG_ERROR("CreateProcessInternalW hook install failed (snare returned {})", hook_result);
                    } else {
                        logger.log("CreateProcessInternalW hook installed successfully");
                    }
                } else {
                    LOG_ERROR("CreateProcessInternalW hook creation failed (snare_inline_new returned null)");
                }
            } else {
                LOG_ERROR("Failed to resolve CreateProcessInternalW from kernelbase.dll or kernel32.dll");
            }
        }
    });
}

/**
 * Millennium uses dll_notification_callback to hook when steamclient.dll unloaded
 * This signifies the Steam Client is unloading.
 * The reason it is done this way is to prevent windows loader lock from deadlocking or destroying parts of Millennium.
 * When we are running inside the callback, we are inside the loader lock (so windows can't continue loading/unloading dlls)
 */
VOID handle_steam_unload()
{
    millennium_lifecycle::get().disconnect_frontend.store(true);
    logger.warn("Terminate condition variable signaled, exiting...");
}

VOID handle_steam_load()
{
    logger.log("[dll_notification_callback] Notified that Steam UI has loaded, notifying main thread...");
    millennium_lifecycle::get().steam_ui_loaded.notify();
}

VOID CALLBACK dll_notification_callback(ULONG notification_reason, PLDR_DLL_NOTIFICATION_DATA notification_data, [[maybe_unused]] PVOID context)
{
    std::wstring_view base_dll_name(notification_data->BaseDllName->Buffer, notification_data->BaseDllName->Length / sizeof(WCHAR));

    /** hook Steam load */
    if (notification_reason == LDR_DLL_NOTIFICATION_REASON_LOADED && base_dll_name == L"steamui.dll") {
        handle_steam_load();
        return;
    }

    /** hook Steam unload */
    if (notification_reason == LDR_DLL_NOTIFICATION_REASON_UNLOADED && base_dll_name == L"steamclient64.dll") {
        logger.log("[dll_notification_callback] Notified that steamclient64.dll has unloaded, handling Steam unload...");
        handle_steam_unload();
        return;
    }

    /** hook steam cross platform api (used to hook create proc) */
    if (notification_reason == LDR_DLL_NOTIFICATION_REASON_LOADED && (base_dll_name == L"tier0_s64.dll" || base_dll_name == L"tier0_s.dll")) {
        handle_tier0_dll(notification_data->DllBase);
        return;
    }
}

void handle_already_loaded(LPCWSTR dll_name)
{
    HMODULE module = GetModuleHandleW(dll_name);

    if (!module) {
        return;
    }

    LDR_DLL_NOTIFICATION_DATA notif_data = {};
    UNICODE_STRING unicode_dll_name = {};
    unicode_dll_name.Buffer = const_cast<PWSTR>(dll_name);
    unicode_dll_name.Length = static_cast<USHORT>(wcslen(dll_name) * sizeof(WCHAR));
    notif_data.BaseDllName = &unicode_dll_name;
    notif_data.DllBase = module;
    dll_notification_callback(LDR_DLL_NOTIFICATION_REASON_LOADED, &notif_data, nullptr);
}

/**
 * Hook and disable ReadDirectoryChangesW to prevent the Steam client from monitoring changes
 * to the steamui dir, which is incredibly fucking annoying during development.
 *
 * Calls that originate from Millennium (e.g. file_watcher) are passed through to the real API.
 */
BOOL WINAPI hooked_read_directory_changes_w(HANDLE hDirectory, LPVOID lpBuffer, DWORD nBufferLength, BOOL bWatchSubtree, DWORD dwNotifyFilter, LPDWORD lpBytesReturned,
                                            LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    auto orig = reinterpret_cast<decltype(&ReadDirectoryChangesW)>(snare_inline_get_trampoline(g_rdcw_hook));
    const auto invoke_original = [&]()
    {
        return orig(hDirectory, lpBuffer, nBufferLength, bWatchSubtree, dwNotifyFilter, lpBytesReturned, lpOverlapped, lpCompletionRoutine);
    };

    wchar_t dir_path[MAX_PATH] = {};
    if (GetFinalPathNameByHandleW(hDirectory, dir_path, MAX_PATH, FILE_NAME_NORMALIZED)) {
        std::error_code ec;
        std::filesystem::path watched = std::filesystem::canonical(std::filesystem::path(dir_path), ec);
        if (ec) return invoke_original();
        std::filesystem::path steamui = std::filesystem::canonical(platform::get_steam_path() / "steamui", ec);
        if (ec) return invoke_original();

        /* block calls to listen to the steamui folder */
        if (watched == steamui) {
            if (lpBytesReturned) *lpBytesReturned = 0;
            return TRUE;
        }
    }

    return invoke_original();
}

void register_dll_notifications()
{
    HMODULE ntdll_module = GetModuleHandleA("ntdll.dll");
    if (!ntdll_module) {
        return;
    }

    LdrRegisterDllNotification = reinterpret_cast<LdrRegisterDllNotification_t>((void*)GetProcAddress(ntdll_module, "LdrRegisterDllNotification"));
    LdrUnregisterDllNotification = reinterpret_cast<LdrUnregisterDllNotification_t>((void*)GetProcAddress(ntdll_module, "LdrUnregisterDllNotification"));

    if (!LdrRegisterDllNotification || !LdrUnregisterDllNotification) {
        return;
    }

    LdrRegisterDllNotification(0, dll_notification_callback, nullptr, &g_notification_cookie);

    // Check modules that were already loaded before the notification was registered.
    // handle_tier0_dll is idempotent via std::once_flag, so double-fire is safe.
    handle_already_loaded(L"tier0_s.dll");
    handle_already_loaded(L"tier0_s64.dll");
    handle_already_loaded(L"steamui.dll");
}

/**
 * Called from the background thread. By the time this runs, register_dll_notifications()
 * has already been called from DllMain, so the CreateSimpleProcess hook is guaranteed
 * to be in place.
 */
bool initialize_steam_hooks()
{
    const auto start_time = std::chrono::system_clock::now();
    logger.log("Waiting for Steam UI to load...");

    /** wait for steamui.dll to load (which signifies Steam is actually starting and not updating/verifying files) */
    millennium_lifecycle::get().steam_ui_loaded.wait();

    const auto end_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_time).count();
    logger.log("Steam UI loaded in {} ms, continuing Millennium startup...", end_time);

    /** only hook if developer mode is enabled */
    if (CommandLineArguments::has_argument("-dev")) {
        g_rdcw_hook = snare_inline_new(reinterpret_cast<void*>(&ReadDirectoryChangesW), reinterpret_cast<void*>(&hooked_read_directory_changes_w));
        if (g_rdcw_hook) snare_inline_install(g_rdcw_hook);
    }

    return g_notification_cookie != nullptr;
}

void uninitialize_steam_hooks()
{
    if (g_rdcw_hook) {
        snare_inline_remove(g_rdcw_hook);
        snare_inline_free(g_rdcw_hook);
        g_rdcw_hook = nullptr;
    }
    if (g_create_hook) {
        snare_inline_remove(g_create_hook);
        snare_inline_free(g_create_hook);
        g_create_hook = nullptr;
    }
    if (g_create_process_internal_hook) {
        snare_inline_remove(g_create_process_internal_hook);
        snare_inline_free(g_create_process_internal_hook);
        g_create_process_internal_hook = nullptr;
    }
}
#elif __linux__
#include "millennium/logger.h"

#include <dlfcn.h>
#include <string>
#define SNARE_STATIC
#define SNARE_IMPLEMENTATION
#include <libsnare.h>
#include <stdlib.h>

static snare_inline create_hook;

/**
 * It seems a2, a3 might be a stack allocated struct pointer, but we don't really need them
 * Even if we mistyped them, it doesn't change the actual underlying data being sent in memory.
 * I assume it has something to with working directory &| flags
 */
extern "C" int hooked_create_simple_process(const char* cmd, unsigned int a2, const char* a3)
{
    cmd = Plat_HookedCreateSimpleProcess(cmd);
    auto orig = reinterpret_cast<int (*)(const char*, unsigned int, const char*)>(create_hook.get_trampoline());
    int result = orig(cmd, a2, a3);

    logger.log("[hooked_create_simple_process]: {}", cmd);
    return result;
}

static void* get_module_handle(const char* libneedle, const char* symbol)
{
    void* handle = dlopen(libneedle, RTLD_NOW | RTLD_NOLOAD);
    if (handle) {
        void* s = dlsym(handle, symbol);
        dlclose(handle);
        if (s) return s;
    }

    void* s2 = dlsym(RTLD_DEFAULT, symbol);
    if (s2) return s2;

    return nullptr;
}

bool initialize_steam_hooks()
{
    const char* libneedle = "libtier0_s.so";
    const char* symbol = "CreateSimpleProcess";

    void* target = get_module_handle(libneedle, symbol);
    if (!target) {
        LOG_ERROR("Failed to locate symbol '{}'", symbol);
        return false;
    }

    logger.log("Located {} at address {}", symbol, target);
    const bool success = create_hook.install(target, (void*)hooked_create_simple_process);
    logger.log("Hook install success?: {}", success);
    return true;
}
#elif defined(__APPLE__)
#include "millennium/logger.h"

#include <filesystem>
#include <unistd.h>

bool initialize_steam_hooks()
{
    const char* configured_port = std::getenv("MILLENNIUM_DEBUG_PORT");
    std::string debugger_port = (configured_port && configured_port[0] != '\0') ? configured_port : DEFAULT_DEVTOOLS_PORT;

    std::filesystem::path helper_hook_path;
    std::filesystem::path child_hook_path;

    const char* configured_hook_path = std::getenv("MILLENNIUM_HOOK_HELPER_PATH");
    if (configured_hook_path && configured_hook_path[0] != '\0') {
        helper_hook_path = configured_hook_path;
    } else {
        const char* runtime_path = std::getenv("MILLENNIUM_RUNTIME_PATH");
        if (runtime_path && runtime_path[0] != '\0') {
            const std::filesystem::path runtime_directory = std::filesystem::path(runtime_path).parent_path();
            helper_hook_path = runtime_directory / "libmillennium_hhx64.dylib";
            child_hook_path = runtime_directory / "libmillennium_child_hook.dylib";
        }
    }

    const char* configured_child_hook_path = std::getenv("MILLENNIUM_CHILD_HOOK_PATH");
    if (configured_child_hook_path && configured_child_hook_path[0] != '\0') {
        child_hook_path = configured_child_hook_path;
    }

    if (helper_hook_path.empty()) {
        const char* steam_path = std::getenv("MILLENNIUM__STEAM_PATH");
        if (steam_path && steam_path[0] != '\0') {
            const std::filesystem::path steam_directory = steam_path;
            helper_hook_path = steam_directory / "libmillennium_hhx64.dylib";
            if (child_hook_path.empty()) {
                child_hook_path = steam_directory / "libmillennium_child_hook.dylib";
            }
        }
    }

    if (helper_hook_path.empty()) {
        logger.warn("macOS bootstrap-based helper injection could not resolve libmillennium_hhx64.dylib.");
        return false;
    }

    if (access(helper_hook_path.c_str(), R_OK) != 0) {
        logger.warn("macOS bootstrap-based helper injection is not available because '{}' is missing.", helper_hook_path.string());
        return false;
    }

    if (child_hook_path.empty()) {
        logger.warn("macOS bootstrap-based helper injection could not resolve libmillennium_child_hook.dylib.");
        return false;
    }

    if (access(child_hook_path.c_str(), R_OK) != 0) {
        logger.warn("macOS bootstrap-based helper injection is not available because '{}' is missing.", child_hook_path.string());
        return false;
    }

    logger.log("Using bootstrap-based Steam Helper injection on macOS with debugger port {}", debugger_port);
    return true;
}
#endif

void platform::wait_for_backend_load()
{
    logger.log("__backend_load was called!");
    millennium_lifecycle::get().backends_loaded.notify();
}

bool platform::initialize_steam_hooks()
{
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    return ::initialize_steam_hooks();
#else
    return false;
#endif
}
