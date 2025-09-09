#include <winsock2.h>

#include "millennium/argp_win32.h"
#include "millennium/env.h"
#include "millennium/http_hooks.h"
#include "millennium/init.h"
#include "millennium/shim_updater.h"
#include "millennium/ffi.h"

#include <MinHook.h>
#include <thread>

/** Maintain compatibility with different compilers */
#ifdef __GNUC__
#define CONSTRUCTOR __attribute__((constructor))
#define DESTRUCTOR __attribute__((destructor))
#define DLL_EXPORT extern "C" __attribute__((dllexport))
#else
#define CONSTRUCTOR
#define DESTRUCTOR
#define DLL_EXPORT extern "C" __declspec(dllexport)
#endif

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
    WinUtils::Win32_UpdatePreloader();

    EntryMain();

    /** Shutdown the shared JS message emitter */
    CefSocketDispatcher& emitter = CefSocketDispatcher::get();
    (&emitter)->Shutdown();

    /** Shutdown cron threads that manage Steam HTTP hooks */
    HttpHookManager& hookManager = HttpHookManager::get();
    (&hookManager)->Shutdown();

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

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