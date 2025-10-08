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
#include "millennium/env.h"
#include "millennium/http_hooks.h"
#include "millennium/init.h"
#include "millennium/ffi.h"
#include "millennium/crash_handler.h"
#include "millennium/millennium_updater.h"

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

VOID Win32_AttachMillennium(VOID)
{
    /** Starts the CEF arg hook, it doesn't wait for the hook to be installed, it waits for the hook to be setup */
    if (!HookCefArgs()) {
        return;
    }

    SetConsoleTitleA(std::string("Millennium@" + std::string(MILLENNIUM_VERSION)).c_str());
    SetupEnvironmentVariables();

    // Set custom terminate handler for easier debugging
    std::set_terminate(UnhandledExceptionHandler);
    SetUnhandledExceptionFilter(Win32_CrashHandler);

    /** cleanup temp files created by the updater from the previous update */
    MillenniumUpdater::CleanupMillenniumUpdaterTempFiles();

    EntryMain();
    Logger.Log("Millennium main function has returned, proceeding with shutdown...");

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
        MessageBoxA(NULL, "Millennium thread is not joinable, skipping join. This is likely because Millennium failed to start properly.", "Warning", MB_ICONWARNING);
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
DLL_EXPORT INT WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
        {
            Logger.Log("Millennium {} attached...", MILLENNIUM_VERSION);
            g_millenniumThread = std::thread(Win32_AttachMillennium);
            break;
        }
        case DLL_PROCESS_DETACH:
        {
            Win32_DetachMillennium();
            break;
        }
    }

    return true;
}
#endif