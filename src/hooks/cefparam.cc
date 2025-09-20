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

#ifdef _WIN32
#include <winsock2.h>
#include "MinHook.h"

#include "millennium/argp_win32.h"
#include "millennium/logger.h"

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

Plat_CommandLineParamValue_t fpOriginalPlat_CommandLineParamValue = nullptr;
Plat_CommandLineParamExists_t fpPlat_CommandLineParamExists = nullptr;
CreateSimpleProcess_t fpCreateSimpleProcess = nullptr;

HMODULE steamTier0Module;
std::string STEAM_DEVELOPER_TOOLS_PORT;

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

struct HookInfo
{
    const char* name;
    LPVOID detour;
    LPVOID* original;
};

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

VOID CALLBACK DllNotificationCallback(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context)
{
    std::wstring baseDllName(NotificationData->BaseDllName->Buffer, NotificationData->BaseDllName->Length / sizeof(WCHAR));

    if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED && baseDllName == L"tier0_s.dll") {
        HandleTier0Dll(NotificationData->DllBase);
    }
}

BOOL HookCefArgs()
{
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

    // Register for DLL notifications
    return NT_SUCCESS(LdrRegisterDllNotification(0, DllNotificationCallback, nullptr, &g_NotificationCookie));
}
#endif