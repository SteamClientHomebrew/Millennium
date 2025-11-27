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

#include "millennium/argp_win32.h"
#include "millennium/plat_msg.h"
#include "millennium/env.h"
#include "millennium/http_hooks.h"
#include "millennium/init.h"
#include "millennium/ffi.h"
#include "millennium/crash_handler.h"
#include "millennium/millennium_updater.h"
#include "millennium/plat_msg.h"

#include <MinHook.h>
#include <thread>

std::thread g_millenniumThread;
void EntryMain(); /** forward declare main function */

/**
 * Setup environment variables used throughout Millennium.
 * These are vital to be setup, if they aren't Millennium and its loaded plugins will likely fail to start/run
 */
CONSTRUCTOR VOID Win32_InitializeEnvironment(VOID)
{
    try {
        SetupEnvironmentVariables();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to set up environment variables: {}", e.what());
        std::exit(EXIT_FAILURE);
    }
}

extern std::mutex mtx_hasSteamUIStartedLoading;
extern std::condition_variable cv_hasSteamUIStartedLoading;

BOOL AreFilesIdentical(LPCWSTR path1, LPCWSTR path2)
{
    HANDLE h1 = CreateFileW(path1, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h1 == INVALID_HANDLE_VALUE) return FALSE;

    HANDLE h2 = CreateFileW(path2, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h2 == INVALID_HANDLE_VALUE) {
        CloseHandle(h1);
        return FALSE;
    }

    BY_HANDLE_FILE_INFORMATION info1, info2;
    BOOL same = FALSE;

    if (GetFileInformationByHandle(h1, &info1) && GetFileInformationByHandle(h2, &info2)) {
        same = (info1.dwVolumeSerialNumber == info2.dwVolumeSerialNumber) && (info1.nFileIndexHigh == info2.nFileIndexHigh) && (info1.nFileIndexLow == info2.nFileIndexLow);
    }

    CloseHandle(h1);
    CloseHandle(h2);
    return same;
}

/**
 * Initialize Millennium webhelper hook by hardlinking it into the cef bin directory.
 */
VOID Win32_AttachWebHelperHook(VOID)
{
    const auto hookPath = SystemIO::GetSteamPath() / "millennium.hhx64.dll";
    const auto targetPath = SystemIO::GetSteamPath() / "bin" / "cef" / "cef.win7x64" / "version.dll";

    if (!std::filesystem::exists(hookPath)) {
        Plat_ShowMessageBox("Millennium Error", "Millennium webhelper hook is missing. Please reinstall Millennium.", MESSAGEBOX_ERROR);
        return;
    }

    if (!AreFilesIdentical(hookPath.wstring().c_str(), targetPath.wstring().c_str())) {
        // Remove existing target if it exists
        DeleteFileW(targetPath.wstring().c_str());
    }

    if (std::filesystem::exists(targetPath)) {
        /** target file exist, and is identical to the hook, so we don't need to hardlink */
        return;
    }

    BOOL result = CreateHardLinkA(targetPath.string().c_str(), hookPath.string().c_str(), NULL);
    if (!result) {
        Plat_ShowMessageBox(
            "Millennium Error",
            fmt::format("Failed to create hardlink for Millennium webhelper hook.\nError Code: {}\nMake sure Steam is not running and try again.", GetLastError()).c_str(),
            MESSAGEBOX_ERROR);
    }
}

VOID Win32_AttachMillennium(VOID)
{
#ifdef MILLENNIUM_64BIT
    /** Starts the CEF arg hook, it doesn't wait for the hook to be installed, it waits for the hook to be setup */
    if (!Plat_InitializeSteamHooks()) {
        return;
    }
#endif

    Win32_AttachWebHelperHook();

    // SetConsoleTitleA(std::string("Millennium@" + std::string(MILLENNIUM_VERSION)).c_str());
    SetupEnvironmentVariables();

    // Set custom terminate handler for easier debugging
    std::set_terminate(UnhandledExceptionHandler);
    SetUnhandledExceptionFilter(Win32_CrashHandler);

    /** cleanup temp files created by the updater from the previous update */
    MillenniumUpdater::CleanupMillenniumUpdaterTempFiles();

    EntryMain();
    Logger.Log("[Win32_AttachMillennium] Millennium main function has returned, proceeding with shutdown...");

    /** Shutdown the shared JS message emitter */
    CefSocketDispatcher& emitter = CefSocketDispatcher::get();
    (&emitter)->Shutdown();
    Logger.Log("CefSocketDispatcher has been shut down.");

    /** Shutdown cron threads that manage Steam HTTP hooks */
    HttpHookManager& hookManager = HttpHookManager::get();
    (&hookManager)->Shutdown();
    Logger.Log("HttpHookManager has been shut down.");

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    Logger.Log("MinHook has been uninitialized.");

    /** Deallocate the developer console */
    if (CommandLineArguments::HasArgument("-dev")) {
        FreeConsole();
    }
}

/**
 * Cleans up resources and gracefully shuts down Millennium on Windows.
 *
 * @warning This cleanup function assumes EntryMain has returned with success, otherwise this detach function will deadlock.
 * It depends on Python being uninitialized with all threads terminated, and all frontend hooks detached.
 */
VOID Win32_DetachMillennium(VOID)
{
    Logger.PrintMessage(" MAIN ", "Shutting Millennium down...", COL_MAGENTA);

    g_shouldTerminateMillennium->flag.store(true);
    Logger.Log("Set terminate flag");

    /** There should be 0% chance the socket is still somehow open, but we free it just in case to prevent deadlocks */
    Sockets::Shutdown();

    Logger.Log("Waiting for Millennium thread to exit...");

    if (!g_millenniumThread.joinable()) {
        Plat_ShowMessageBox("Warning", "Millennium thread is not joinable, skipping join. This is likely because Millennium failed to start properly.", MESSAGEBOX_WARNING);
        return;
    }

    g_millenniumThread.join();
    Logger.Log("Millennium thread has exited.");
}

/**
 * @brief Entry point for Millennium on Windows.
 * @param fdwReason The reason for calling the DLL.
 * @return True if the DLL was successfully loaded, false otherwise.
 */
DLL_EXPORT INT WINAPI DllMain([[maybe_unused]] HINSTANCE hinstDLL, DWORD fdwReason, [[maybe_unused]] LPVOID lpvReserved)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
        {
#if defined(MILLENNIUM_32BIT)
            const char* plat = "32-bit";
#elif defined(MILLENNIUM_64BIT)
            const char* plat = "64-bit";
#else
#error "Unsupported Platform"
#endif
            Logger.Log("Millennium-{}@{} attached...", plat, MILLENNIUM_VERSION);
            g_millenniumThread = std::thread(Win32_AttachMillennium);
            break;
        }
        case DLL_PROCESS_DETACH:
        {
#if defined(MILLENNIUM_64BIT)
            Win32_DetachMillennium();
#elif defined(MILLENNIUM_32BIT)
            if (g_millenniumThread.joinable()) {
                g_millenniumThread.detach();
            }
            exit(0);
#else
#error "Unsupported Platform"
#endif
            break;
        }
    }

    return true;
}
#endif
