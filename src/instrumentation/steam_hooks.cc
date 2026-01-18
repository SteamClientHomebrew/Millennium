/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
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
#include "millennium/steam_hooks.h"
#include "millennium/argp_win32.h"

std::mutex mtx_hasAllPythonPluginsShutdown, mtx_hasSteamUnloaded, mtx_hasSteamUIStartedLoading, mtx_hasBackendsLoaded;
std::condition_variable cv_hasSteamUnloaded, cv_hasAllPythonPluginsShutdown, cv_hasSteamUIStartedLoading, cv_hasBackendsLoaded;
std::atomic<bool> atm_hasBackendsAlreadyLoaded{ false };

std::string STEAM_DEVELOPER_TOOLS_PORT = "8080";

static std::atomic<bool> g_hasWaitedForBackends{ false };

uint_least16_t GetRandomOpenPort()
{
    asio::io_context io_context;
    asio::ip::tcp::acceptor acceptor(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
    return acceptor.local_endpoint().port();
}

const char* GetAppropriateDevToolsPort(const bool isDevMode)
{
    const char* port = isDevMode ? DEFAULT_DEVTOOLS_PORT : STEAM_DEVELOPER_TOOLS_PORT.c_str();
    return port;
}

#define MAX_PARAMS 128
#define MAX_PARAM_LEN 512
#define MAX_COMMAND_LEN 4096 /** I don't think any process call from steam would be longer than this */

typedef struct
{
    char* exec;
    char* params[MAX_PARAMS];
    int param_count;

    char full_command[MAX_COMMAND_LEN];
    int dirty;
} Command;

void Command_mark_dirty(Command* cmd)
{
    cmd->dirty = 1;
}

char* Command_get_executable(Command* cmd)
{
    char* last_slash = strrchr(cmd->exec, '/');
    char* last_backslash = strrchr(cmd->exec, '\\');
    char* last_separator = last_slash > last_backslash ? last_slash : last_backslash;
    return last_separator ? last_separator + 1 : cmd->exec;
}

void Command_init(Command* cmd, const char* full_cmd)
{
    cmd->param_count = 0;
    cmd->exec = NULL;
    cmd->dirty = 1;

#ifdef _WIN32
    char* copy = strdup(full_cmd);
    char* p = copy;
    char token[MAX_PARAM_LEN];
    int token_idx = 0;
    int in_quotes = 0;
    int token_count = 0;

    while (*p) {
        if (*p == '"') {
            in_quotes = !in_quotes;
            p++;
            continue;
        }
        if (*p == ' ' && !in_quotes) {
            if (token_idx > 0) {
                token[token_idx] = '\0';
                if (token_count == 0) {
                    cmd->exec = strdup(token);
                } else if (cmd->param_count < MAX_PARAMS) {
                    cmd->params[cmd->param_count++] = strdup(token);
                }
                token_count++;
                token_idx = 0;
            }
            p++;
            continue;
        }
        if (token_idx < MAX_PARAM_LEN - 1) {
            token[token_idx++] = *p;
        }
        p++;
    }
    if (token_idx > 0) {
        token[token_idx] = '\0';
        if (token_count == 0) {
            cmd->exec = strdup(token);
        } else if (cmd->param_count < MAX_PARAMS) {
            cmd->params[cmd->param_count++] = strdup(token);
        }
    }
    free(copy);
#else
    char* copy = strdup(full_cmd);
    char* token = strtok(copy, " ");

    if (token) {
        size_t len = strlen(token);
        if (len >= 2 && token[0] == '\'' && token[len - 1] == '\'') {
            token[len - 1] = '\0';
            token++;
        }
        cmd->exec = strdup(token);
        token = strtok(NULL, " ");
    }

    while (token && cmd->param_count < MAX_PARAMS) {
        size_t len = strlen(token);
        if (len >= 2 && token[0] == '\'' && token[len - 1] == '\'') {
            token[len - 1] = '\0';
            token++;
        }
        cmd->params[cmd->param_count++] = strdup(token);
        token = strtok(NULL, " ");
    }

    free(copy);
#endif
}

void Command_free(Command* cmd)
{
    if (cmd->exec) free(cmd->exec);
    for (int i = 0; i < cmd->param_count; i++)
        free(cmd->params[i]);
}

int Command_has_param(Command* cmd, const char* key)
{
    size_t key_len = strlen(key);
    for (int i = 0; i < cmd->param_count; i++) {
        if (strcmp(cmd->params[i], key) == 0) return 1;
        if (strncmp(cmd->params[i], key, key_len) == 0 && cmd->params[i][key_len] == '=') return 1;
    }
    return 0;
}

void Command_add_param(Command* cmd, const char* param)
{
    if (cmd->param_count >= MAX_PARAMS) return;
    if (!Command_has_param(cmd, param)) {
        for (int i = cmd->param_count; i > 0; i--)
            cmd->params[i] = cmd->params[i - 1];
        cmd->params[0] = strdup(param); // Prepend
        cmd->param_count++;
        Command_mark_dirty(cmd);
    }
}

void Command_remove_param(Command* cmd, const char* key)
{
    size_t key_len = strlen(key);
    for (int i = 0; i < cmd->param_count; i++) {
        if (strcmp(cmd->params[i], key) == 0 || (strncmp(cmd->params[i], key, key_len) == 0 && cmd->params[i][key_len] == '=')) {
            free(cmd->params[i]);
            for (int j = i; j < cmd->param_count - 1; j++)
                cmd->params[j] = cmd->params[j + 1];
            cmd->param_count--;
            i--;
            Command_mark_dirty(cmd);
        }
    }
}

void Command_update_param(Command* cmd, const char* key, const char* value)
{
    size_t key_len = strlen(key);
    char new_param[MAX_PARAM_LEN];
    if (value)
        snprintf(new_param, sizeof(new_param), "%s=%s", key, value);
    else
        snprintf(new_param, sizeof(new_param), "%s", key);

    for (int i = 0; i < cmd->param_count; i++) {
        if (strcmp(cmd->params[i], key) == 0 || (strncmp(cmd->params[i], key, key_len) == 0 && cmd->params[i][key_len] == '=')) {
            free(cmd->params[i]);
            for (int j = i; j < cmd->param_count - 1; j++)
                cmd->params[j] = cmd->params[j + 1];
            cmd->param_count--;
            for (int j = cmd->param_count; j > 0; j--)
                cmd->params[j] = cmd->params[j - 1];
            cmd->params[0] = strdup(new_param);
            cmd->param_count++;
            Command_mark_dirty(cmd);
            return;
        }
    }
    if (cmd->param_count < MAX_PARAMS) {
        for (int i = cmd->param_count; i > 0; i--)
            cmd->params[i] = cmd->params[i - 1];
        cmd->params[0] = strdup(new_param);
        cmd->param_count++;
        Command_mark_dirty(cmd);
    }
}

static void Command_ensure_parameter(Command* c, const char* cmd, const char* value)
{
    if (Command_has_param(c, cmd)) {
        Command_update_param(c, cmd, value);
    } else {
        if (!value) {
            Command_add_param(c, cmd);
        } else {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "%s=%s", cmd, value);
            Command_add_param(c, buffer);
        }
    }
}

const char* Command_get(Command* cmd)
{
    if (!cmd->dirty) return cmd->full_command;

    size_t pos = 0;
#ifdef _WIN32
    if (strchr(cmd->exec, ' ')) {
        pos += snprintf(cmd->full_command + pos, MAX_COMMAND_LEN - pos, "\"%s\"", cmd->exec);
    } else {
        pos += snprintf(cmd->full_command + pos, MAX_COMMAND_LEN - pos, "%s", cmd->exec);
    }
#else
    pos += snprintf(cmd->full_command + pos, MAX_COMMAND_LEN - pos, "%s", cmd->exec);
#endif

    for (int i = 0; i < cmd->param_count; i++) {
#ifdef _WIN32
        if (strchr(cmd->params[i], '=') || strchr(cmd->params[i], ' ')) {
            pos += snprintf(cmd->full_command + pos, MAX_COMMAND_LEN - pos, " \"%s\"", cmd->params[i]);
        } else {
            pos += snprintf(cmd->full_command + pos, MAX_COMMAND_LEN - pos, " %s", cmd->params[i]);
        }
#else
        pos += snprintf(cmd->full_command + pos, MAX_COMMAND_LEN - pos, " %s", cmd->params[i]);
#endif
    }

    cmd->dirty = 0;
    return cmd->full_command;
}

const char* Plat_HookedCreateSimpleProcess(const char* cmd)
{
    /** only wait on the first call. */
    if (!g_hasWaitedForBackends.load()) {
        logger.log("[Plat_HookedCreateSimpleProcess] Waiting for backends to finish first load...");
        std::unique_lock<std::mutex> lock(mtx_hasBackendsLoaded);

        /** if backends have already loaded before this function was called, we don't need to wait. */
        if (!atm_hasBackendsAlreadyLoaded.load()) {
            cv_hasBackendsLoaded.wait(lock);
        }

        logger.log("[Plat_HookedCreateSimpleProcess] All backends have loaded, proceeding with CEF start...");
        g_hasWaitedForBackends.store(true);
    }

    Command c;
    Command_init(&c, cmd);

    const char* target_executable =
#ifdef _WIN32
        "steamwebhelper.exe";
#elif __linux__
        "steamwebhelper.sh";
#endif

    char* hooked_target = Command_get_executable(&c);

    if (strcmp(hooked_target, target_executable) != 0) {
        Command_free(&c);
        return cmd;
    }

    int is_developer_mode = CommandLineArguments::HasArgument("-dev");

    /** block any browser web requests when running in normal mode */
    Command_ensure_parameter(&c, "--remote-allow-origins", is_developer_mode ? "*" : "");
    /** always ensure the debugger is only local, and can't be accessed over lan */
    Command_ensure_parameter(&c, "--remote-debugging-address", "127.0.0.1");
    /** enable the CEF remote debugger */
    Command_ensure_parameter(&c, "--remote-debugging-port", GetAppropriateDevToolsPort(is_developer_mode));

    const char* result = Command_get(&c);
    char* owned_cmd = strdup(result);
    Command_free(&c);

    return owned_cmd;
}

#ifdef _WIN32
#include "MinHook.h"

#include "millennium/argp_win32.h"
#include "millennium/logger.h"
#include "millennium/backend_mgr.h"

#define LDR_DLL_NOTIFICATION_REASON_LOADED 1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2

LdrRegisterDllNotification_t LdrRegisterDllNotification = nullptr;
LdrUnregisterDllNotification_t LdrUnregisterDllNotification = nullptr;
PVOID g_NotificationCookie = nullptr;

typedef INT(__cdecl* CreateSimpleProcess_t)(const char* a1, char a2, const char* lpMultiByteStr);
typedef BOOL(WINAPI* ReadDirectoryChangesW_t)(HANDLE, LPVOID, DWORD, BOOL, DWORD, LPDWORD, LPOVERLAPPED, LPOVERLAPPED_COMPLETION_ROUTINE);

CreateSimpleProcess_t fpCreateSimpleProcess = nullptr;
ReadDirectoryChangesW_t orig_ReadDirectoryChangesW = NULL;

HMODULE steamTier0Module;
std::atomic<bool> ab_shouldDisconnectFrontend{ false };

INT Hooked_CreateSimpleProcess(const char* a1, char a2, const char* lpMultiByteStr)
{
    return fpCreateSimpleProcess(Plat_HookedCreateSimpleProcess(a1), a2, lpMultiByteStr);
}

/**
 * tier0_s.dll is a *cross platform* library bundled with Steam that helps it
 * manage low-level system interactions and provides various utility functions.
 *
 * It houses various functions, and we are interested in hooking its functions Steam
 * uses to spawn the Steam web helper.
 */
VOID HandleTier0Dll(PVOID moduleBaseAddress)
{
    steamTier0Module = static_cast<HMODULE>(moduleBaseAddress);
    Logger.Log("Setting up hooks for tier0_s.dll");

    FARPROC proc = GetProcAddress(steamTier0Module, "CreateSimpleProcess");
    if (proc != nullptr) {
        if (MH_CreateHook(reinterpret_cast<LPVOID>(proc), reinterpret_cast<LPVOID>(&Hooked_CreateSimpleProcess), reinterpret_cast<LPVOID*>(&fpCreateSimpleProcess)) != MH_OK) {
            MessageBoxA(NULL, "Failed to create hook for CreateSimpleProcess", "Error", MB_ICONERROR | MB_OK);
            return;
        }
    }

    MH_EnableHook(MH_ALL_HOOKS);
}

/**
 * Millennium uses DllNotificationCallback to hook when steamclient.dll unloaded
 * This signifies the Steam Client is unloading.
 * The reason it is done this way is to prevent windows loader lock from deadlocking or destroying parts of Millennium.
 * When we are running inside the callback, we are inside the loader lock (so windows can't continue loading/unloading dlls)
 */
VOID HandleSteamUnload()
{
    /**
     * notify the millennium frontend to disconnect
     * once the frontend disconnects, it will shutdown the rest of Millennium
     */
    ab_shouldDisconnectFrontend.store(true);
    // auto& backendManager = BackendManager::GetInstance();

    // /** No need to wait if all backends aren't python */
    // if (!backendManager.HasAnyPythonBackends() && !backendManager.HasAnyLuaBackends()) {
    //     Logger.Warn("No backends detected, skipping shutdown wait...");
    //     return;
    // }

    // std::unique_lock<std::mutex> lk(mtx_hasSteamUnloaded);
    // Logger.Warn("Waiting for Millennium to be unloaded...");

    // /** wait for Millennium to be unloaded */
    // cv_hasSteamUnloaded.wait(lk, [&backendManager]()
    // {
    //     /** wait for all backends to stop so we can safely free the loader lock */
    //     if (backendManager.HasAllPythonBackendsStopped() && backendManager.HasAllLuaBackendsStopped()) {
    //         Logger.Warn("All backends have stopped, proceeding with termination...");

    //         std::unique_lock<std::mutex> lk2(mtx_hasAllPythonPluginsShutdown);
    //         cv_hasAllPythonPluginsShutdown.notify_all();
    //         return true;
    //     }
    //     return false;
    // });

    logger.warn("Terminate condition variable signaled, exiting...");
}

VOID HandleSteamLoad()
{
    Logger.Log("[DllNotificationCallback] Notified that Steam UI has loaded, notifying main thread...");
    cv_hasSteamUIStartedLoading.notify_all();
}

VOID CALLBACK DllNotificationCallback(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, [[maybe_unused]] PVOID Context)
{
    std::wstring_view baseDllName(NotificationData->BaseDllName->Buffer, NotificationData->BaseDllName->Length / sizeof(WCHAR));

    /** hook Steam load */
    if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED && baseDllName == L"steamui.dll") {
        HandleSteamLoad();
        return;
    }

    /** hook Steam unload */
    if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_UNLOADED && baseDllName == L"steamclient64.dll") {
        Logger.Log("[DllNotificationCallback] Notified that steamclient64.dll has unloaded, handling Steam unload...");
        HandleSteamUnload();
        return;
    }

    /** hook steam cross platform api (used to hook create proc) */
    if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED && baseDllName == L"tier0_s64.dll") {
        HandleTier0Dll(NotificationData->DllBase);
        return;
    }
}

void HandleAlreadyLoaded(LPCWSTR dllName)
{
    HMODULE module = GetModuleHandleW(dllName);

    if (!module) {
        return;
    }

    LDR_DLL_NOTIFICATION_DATA notifData = {};
    UNICODE_STRING unicodeDllName = {};
    unicodeDllName.Buffer = const_cast<PWSTR>(dllName);
    unicodeDllName.Length = static_cast<USHORT>(wcslen(dllName) * sizeof(WCHAR));
    notifData.BaseDllName = &unicodeDllName;
    notifData.DllBase = module;
    DllNotificationCallback(LDR_DLL_NOTIFICATION_REASON_LOADED, &notifData, nullptr);
}

/**
 * Hook and disable ReadDirectoryChangesW to prevent the Steam client from monitoring changes
 * to the steamui dir, which is incredibly fucking annoying during development.
 */
BOOL WINAPI Hooked_ReadDirectoryChangesW(void*, void*, void*, void*, void*, LPDWORD bytesRet, void*, void*)
{
    Logger.Log("[Steam] Blocked attempt to ReadDirectoryChangesW...");

    if (bytesRet) *bytesRet = 0; // no changes
    return TRUE;                 // indicate success
}

bool InitializeSteamHooks()
{
    const auto startTime = std::chrono::system_clock::now();

    if (MH_Initialize() != MH_OK) {
        MessageBoxA(NULL, "Failed to initialize MinHook", "Error", MB_ICONERROR | MB_OK);
        return false;
    }

#ifdef MILLENNIUM_64BIT

    STEAM_DEVELOPER_TOOLS_PORT = std::to_string(GetRandomOpenPort());

    HMODULE hTier0 = GetModuleHandleW(L"tier0_s.dll");
    if (hTier0) {
        HandleTier0Dll(hTier0);
        return true;
    }

    HMODULE ntdllModule = GetModuleHandleA("ntdll.dll");
    if (!ntdllModule) {
        MessageBoxA(NULL, "Failed to get handle for ntdll.dll", "Error", MB_ICONERROR | MB_OK);
        return false;
    }

    // Check if modules are already loaded and handle them
    HandleAlreadyLoaded(L"steamui.dll");
    HandleAlreadyLoaded(L"steamclient64.dll");
    HandleAlreadyLoaded(L"tier0_s64.dll");

    LdrRegisterDllNotification = reinterpret_cast<LdrRegisterDllNotification_t>((void*)GetProcAddress(ntdllModule, "LdrRegisterDllNotification"));
    LdrUnregisterDllNotification = reinterpret_cast<LdrUnregisterDllNotification_t>((void*)GetProcAddress(ntdllModule, "LdrUnregisterDllNotification"));

    if (!LdrRegisterDllNotification || !LdrUnregisterDllNotification) {
        MessageBoxA(NULL, "Failed to get address for LdrRegisterDllNotification or LdrUnregisterDllNotification", "Error", MB_ICONERROR | MB_OK);
        return false;
    }

    auto dllRegStatus = NT_SUCCESS(LdrRegisterDllNotification(0, DllNotificationCallback, nullptr, &g_NotificationCookie));

    Logger.Log("[SH_Hook] Waiting for Steam UI to load...");

    /** wait for steamui.dll to load (which signifies Steam is actually starting and not updating/verifying files) */
    std::unique_lock<std::mutex> lk(mtx_hasSteamUIStartedLoading);
    cv_hasSteamUIStartedLoading.wait(lk);
#elif MILLENNIUM_32BIT
    bool dllRegStatus = true;
#endif
    const auto endTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime).count();
    Logger.Log("[SH_Hook] Steam UI loaded in {} ms, continuing Millennium startup...", endTime);

    /** only hook if developer mode is enabled */
    if (CommandLineArguments::HasArgument("-dev")) {
        MH_CreateHook((LPVOID)&ReadDirectoryChangesW, (LPVOID)&Hooked_ReadDirectoryChangesW, (LPVOID*)&orig_ReadDirectoryChangesW);
        MH_EnableHook((LPVOID)&ReadDirectoryChangesW);
    }

    return dllRegStatus;
}
#elif __linux__
#include "millennium/logger.h"

#include <dlfcn.h>
#include <string>
#include <fmt/format.h>
#include <subhook.h>

static SubHook create_hook;

/**
 * It seems a2, a3 might be a stack allocated struct pointer, but we don't really need them
 * Even if we mistyped them, it doesn't change the actual underlying data being sent in memory.
 * I assume it has something to with working directory &| flags
 */
extern "C" int Hooked_CreateSimpleProcess(const char* cmd, unsigned int a2, const char* a3)
{
    /** temporarily remove the hook to prevent recursive hook calls */
    SubHook::ScopedRemove remove(&create_hook);
    cmd = Plat_HookedCreateSimpleProcess(cmd);
    /** call the original */
    return reinterpret_cast<int (*)(const char* cmd, unsigned int flags, const char* cwd)>(create_hook.GetSrc())(cmd, a2, a3);
}

static void* GetModuleHandle(const char* libneedle, const char* symbol)
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

bool InitializeSteamHooks()
{
    const char* libneedle = "libtier0_s.so";
    const char* symbol = "CreateSimpleProcess";

    STEAM_DEVELOPER_TOOLS_PORT = std::to_string(GetRandomOpenPort());

    void* target = GetModuleHandle(libneedle, symbol);
    if (!target) {
        LOG_ERROR("Failed to locate symbol '{}'", symbol);
        return false;
    }

    logger.log("Located {} at address {}", symbol, target);
    const bool success = create_hook.Install(target, (void*)Hooked_CreateSimpleProcess);
    logger.log("Hook install success?: {}", success);
    return true;
}
#endif

void Plat_WaitForBackendLoad()
{
    logger.log("__backend_load was called!");
    cv_hasBackendsLoaded.notify_all();
    atm_hasBackendsAlreadyLoaded.store(true);
}

bool Plat_InitializeSteamHooks()
{
    return InitializeSteamHooks();
}
