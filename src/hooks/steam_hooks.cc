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

#include <condition_variable>

std::mutex mtx_hasAllPythonPluginsShutdown, mtx_hasSteamUnloaded, mtx_hasSteamUIStartedLoading;
std::condition_variable cv_hasSteamUnloaded, cv_hasAllPythonPluginsShutdown, cv_hasSteamUIStartedLoading;

#ifdef _WIN32
#include <winsock2.h>
#include "MinHook.h"

#include "millennium/argp_win32.h"
#include "millennium/logger.h"
#include "millennium/backend_mgr.h"

#define ARG_REMOTE_ALLOW_ORIGINS "--remote-allow-origins"
#define ARG_REMOTE_DEBUGGING_ADDRESS "--remote-debugging-address"
#define ARG_REMOTE_DEBUGGING_PORT "--remote-debugging-port"

#define LDR_DLL_NOTIFICATION_REASON_LOADED 1
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2
#define DEFAULT_DEVTOOLS_PORT "8080"

LdrRegisterDllNotification_t LdrRegisterDllNotification = nullptr;
LdrUnregisterDllNotification_t LdrUnregisterDllNotification = nullptr;
PVOID g_NotificationCookie = nullptr;

typedef const char*(__cdecl* Plat_CommandLineParamValue_t)(const char* param);
typedef CHAR(__cdecl* Plat_CommandLineParamExists_t)(const char* param);
typedef INT(__cdecl* CreateSimpleProcess_t)(const char* a1, char a2, const char* lpMultiByteStr);
typedef BOOL(WINAPI* ReadDirectoryChangesW_t)(HANDLE, LPVOID, DWORD, BOOL, DWORD, LPDWORD, LPOVERLAPPED, LPOVERLAPPED_COMPLETION_ROUTINE);

Plat_CommandLineParamValue_t fpOriginalPlat_CommandLineParamValue = nullptr;
Plat_CommandLineParamExists_t fpPlat_CommandLineParamExists = nullptr;
CreateSimpleProcess_t fpCreateSimpleProcess = nullptr;
ReadDirectoryChangesW_t orig_ReadDirectoryChangesW = NULL;

HMODULE steamTier0Module;
std::string STEAM_DEVELOPER_TOOLS_PORT;

std::atomic<bool> ab_shouldDisconnectFrontend{ false };

struct HookInfo
{
    const char* name;
    LPVOID detour;
    LPVOID* original;
};

unsigned short GetRandomOpenPort()
{
    asio::io_context io_context;
    asio::ip::tcp::acceptor acceptor(io_context);
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), 0);

    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    acceptor.listen();
    return acceptor.local_endpoint().port();
}

const bool IsDeveloperMode(void)
{
    return CommandLineArguments::HasArgument("-dev");
}

const char* GetAppropriateDevToolsPort()
{
    const bool isDevMode = IsDeveloperMode();
    const char* port = isDevMode ? DEFAULT_DEVTOOLS_PORT : STEAM_DEVELOPER_TOOLS_PORT.c_str();
    return port;
}

void AppendParameter(std::string& input, const std::string& parameter)
{
    if (!input.empty() && input.back() != ' ') {
        input += ' ';
    }
    input += '"' + parameter + '"';
}

std::string ExtractExecutablePath(const std::string& input)
{
    std::string exePath;
    if (!input.empty() && input[0] == '"') {
        size_t end = input.find('"', 1);
        if (end != std::string::npos) {
            exePath = input.substr(1, end - 1);
        }
    } else {
        size_t space = input.find(' ');
        exePath = (space != std::string::npos) ? input.substr(0, space) : input;
    }

    std::transform(exePath.cbegin(), exePath.cend(), exePath.begin(), ::tolower);
    return exePath;
}

/**
 * Prevent the developer tools from being accessed on the browser programmatically.
 */
void BlockDeveloperToolsAccess(std::string& input)
{
    const std::string target = ARG_REMOTE_ALLOW_ORIGINS "=*";
    const std::string replacement = ARG_REMOTE_ALLOW_ORIGINS;

    size_t pos = input.find(target);
    if (pos != std::string::npos) {
        input.replace(pos, target.size(), replacement);
    }
}

/**
 * Force the remote debugging address to localhost, so its not accessible externally.
 */
void EnsureRemoteDebuggingAddress(std::string& input)
{
    const std::string remoteDebugAddr = ARG_REMOTE_DEBUGGING_ADDRESS "=127.0.0.1";

    if (input.find(remoteDebugAddr) == std::string::npos) {
        AppendParameter(input, remoteDebugAddr);
    }
}

/**
 * Virtually enable the remote debugger without having to touch the disk like previously done.
 * Previous to 8/27/2025 the remote debugger was enabled via .cef-enable-remote-debugging file.
 */
void EnsureRemoteDebuggingPort(std::string& input)
{
    const std::string remoteDebugPortPrefix = ARG_REMOTE_DEBUGGING_PORT "=";

    if (input.find(remoteDebugPortPrefix) == std::string::npos) {
        std::string portParam = remoteDebugPortPrefix + GetAppropriateDevToolsPort();
        AppendParameter(input, portParam);
    } else {
        /** replace with the appropriate port */
        std::string newPort = remoteDebugPortPrefix + GetAppropriateDevToolsPort();
        size_t pos = input.find(remoteDebugPortPrefix);

        if (pos != std::string::npos) {
            size_t end = input.find('"', pos);
            input.replace(pos, (end == std::string::npos ? input.length() : end) - pos, newPort);
        }
    }
}

const char* SanitizeCommandLine(const char* cmd)
{
    if (!cmd) {
        return nullptr;
    }

    std::string input(cmd);
    std::string exePath = ExtractExecutablePath(input);

    /** If the call isn't spawning Steam's webhelper, just ignore it */
    if (exePath.find("steamwebhelper.exe") == std::string::npos) {
        return _strdup(cmd);
    }

    Logger.Log("Is developer mode: {}", IsDeveloperMode());

    /** If developer mode is enabled, disable restrictions */
    if (!IsDeveloperMode()) {
        BlockDeveloperToolsAccess(input);
    }

    EnsureRemoteDebuggingAddress(input);
    EnsureRemoteDebuggingPort(input);

    return _strdup(input.c_str());
}

INT Hooked_CreateSimpleProcess(const char* a1, char a2, const char* lpMultiByteStr)
{
    return fpCreateSimpleProcess(SanitizeCommandLine(a1), a2, lpMultiByteStr);
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

    HookInfo hooks[] = {
        /**
         * Used in the creation of the Steam web helper -- this is the actual function that spawns the child process. We can hook it directly and
         * edit the commandline before its passed to the system.
         */
        { "CreateSimpleProcess", reinterpret_cast<LPVOID>(&Hooked_CreateSimpleProcess), reinterpret_cast<LPVOID*>(&fpCreateSimpleProcess) }
    };

    for (const auto& hook : hooks) {
        FARPROC proc = GetProcAddress(steamTier0Module, hook.name);
        if (proc != nullptr) {
            if (MH_CreateHook(reinterpret_cast<LPVOID>(proc), hook.detour, hook.original) != MH_OK) {
                MessageBoxA(NULL, "Failed to create hook for CreateSimpleProcess", "Error", MB_ICONERROR | MB_OK);
                return;
            }
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
    auto& backendManager = BackendManager::GetInstance();

    /** No need to wait if all backends aren't python */
    if (!backendManager.HasAnyPythonBackends() && !backendManager.HasAnyLuaBackends()) {
        Logger.Warn("No backends detected, skipping shutdown wait...");
        return;
    }

    std::unique_lock<std::mutex> lk(mtx_hasSteamUnloaded);
    Logger.Warn("Waiting for Millennium to be unloaded...");

    /** wait for Millennium to be unloaded */
    cv_hasSteamUnloaded.wait(lk, [&backendManager]()
    {
        /** wait for all backends to stop so we can safely free the loader lock */
        if (backendManager.HasAllPythonBackendsStopped() && backendManager.HasAllLuaBackendsStopped()) {
            Logger.Warn("All backends have stopped, proceeding with termination...");

            std::unique_lock<std::mutex> lk2(mtx_hasAllPythonPluginsShutdown);
            cv_hasAllPythonPluginsShutdown.notify_all();
            return true;
        }
        return false;
    });

    Logger.Warn("Terminate condition variable signaled, exiting...");
}

VOID HandleSteamLoad()
{
    Logger.Log("[DllNotificationCallback] Notified that Steam UI has loaded, notifying main thread...");
    cv_hasSteamUIStartedLoading.notify_all();
}

VOID CALLBACK DllNotificationCallback(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context)
{
    std::wstring_view baseDllName(NotificationData->BaseDllName->Buffer, NotificationData->BaseDllName->Length / sizeof(WCHAR));

    /** hook Steam load */
    if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED && baseDllName == L"steamui.dll") {
        HandleSteamLoad();
        return;
    }

    /** hook Steam unload */
    if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_UNLOADED && baseDllName == L"steamclient.dll") {
        HandleSteamUnload();
        return;
    }

    /** hook steam cross platform api (used to hook create proc) */
    if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED && baseDllName == L"tier0_s.dll") {
        HandleTier0Dll(NotificationData->DllBase);
        return;
    }
}

/**
 * Hook and disable ReadDirectoryChangesW to prevent the Steam client from monitoring changes
 * to the steamui dir, which is incredibly fucking annoying during development.
 */
BOOL WINAPI Hooked_ReadDirectoryChangesW(void*, void*, void*, void*, void*, LPDWORD bytesRet, void*, void*)
{
    Logger.Log("[Steam] Blocked attempt to ReadDirectoryChangesW...");

    if (bytesRet)
        *bytesRet = 0; // no changes
    return TRUE;       // indicate success
}

BOOL InitializeSteamHooks()
{
    const auto startTime = std::chrono::system_clock::now();

    if (MH_Initialize() != MH_OK) {
        MessageBoxA(NULL, "Failed to initialize MinHook", "Error", MB_ICONERROR | MB_OK);
        return false;
    }

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

    LdrRegisterDllNotification = (LdrRegisterDllNotification_t)GetProcAddress(ntdllModule, "LdrRegisterDllNotification");
    LdrUnregisterDllNotification = (LdrUnregisterDllNotification_t)GetProcAddress(ntdllModule, "LdrUnregisterDllNotification");

    if (!LdrRegisterDllNotification || !LdrUnregisterDllNotification) {
        MessageBoxA(NULL, "Failed to get address for LdrRegisterDllNotification or LdrUnregisterDllNotification", "Error", MB_ICONERROR | MB_OK);
        return false;
    }

    auto dllRegStatus = NT_SUCCESS(LdrRegisterDllNotification(0, DllNotificationCallback, nullptr, &g_NotificationCookie));

    Logger.Log("[SH_Hook] Waiting for Steam UI to load...");

    /** wait for steamui.dll to load (which signifies Steam is actually starting and not updating/verifying files) */
    std::unique_lock<std::mutex> lk(mtx_hasSteamUIStartedLoading);
    cv_hasSteamUIStartedLoading.wait(lk);

    const auto endTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime).count();
    Logger.Log("[SH_Hook] Steam UI loaded in {} ms, continuing Millennium startup...", endTime);

    /** only hook if developer mode is enabled */
    if (IsDeveloperMode()) {
        MH_CreateHook((LPVOID)&ReadDirectoryChangesW, (LPVOID)&Hooked_ReadDirectoryChangesW, (LPVOID*)&orig_ReadDirectoryChangesW);
        MH_EnableHook((LPVOID)&ReadDirectoryChangesW);
    }

    return dllRegStatus;
}
#endif
