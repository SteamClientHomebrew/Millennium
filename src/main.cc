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

#define UNICODE
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#define _WINSOCKAPI_
#endif
#include "co_spawn.h"
#include "crash_handler.h"
#include "executor.h"
#include "loader.h"
#include "terminal_pipe.h"
#include <env.h>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <internal_logger.h>
#include <signal.h>

#ifdef __linux__
extern "C" int IsSamePath(const char* path1, const char* path2);
#endif

/**
 * @brief Verify the environment to ensure that the CEF remote debugging is enabled.
 * .cef-enable-remote-debugging is a special file name that Steam uses to signal CEF to enable remote debugging.
 */
const static void VerifyEnvironment()
{
    // Check if the user has set a Steam.cfg file to block updates, this is incompatible with Millennium as Millennium relies on the latest version of Steam.
    const auto steamUpdateBlock = SystemIO::GetSteamPath() / "Steam.cfg";

    const std::string errorMessage = fmt::format(
        "Millennium is incompatible with your {} config. This is a file you likely created to block Steam updates. In order for Millennium to properly function, remove it.",
        steamUpdateBlock.string());

    if (std::filesystem::exists(steamUpdateBlock))
    {
        try
        {
            std::string steamConfig = SystemIO::ReadFileSync(steamUpdateBlock.string());

            std::vector<std::string> blackListedKeys = {
                "BootStrapperInhibitAll",
                "BootStrapperForceSelfUpdate",
                "BootStrapperInhibitClientChecksum",
                "BootStrapperInhibitBootstrapperChecksum",
                "BootStrapperInhibitUpdateOnLaunch",
            };

            for (const auto& key : blackListedKeys)
            {
                if (steamConfig.find(key) != std::string::npos)
                {
                    throw SystemIO::FileException("Steam.cfg contains blacklisted keys");
                }
            }
        }
        catch (const SystemIO::FileException& e)
        {
#ifdef _WIN32
            MessageBoxA(NULL, errorMessage.c_str(), "Startup Error", MB_ICONERROR | MB_OK);
#endif
            LOG_ERROR(errorMessage);
        }
    }
}

/**
 * @brief Millennium's main method, called on startup on both Windows and Linux.
 */
const static void EntryMain()
{
    /** Handle signal interrupts (^C) */
    signal(SIGINT, [](int signalCode) { std::exit(128 + SIGINT); });

#if defined(_WIN32)
    SetConsoleTitleA(std::string("Millennium@" + std::string(MILLENNIUM_VERSION)).c_str());
    SetupEnvironmentVariables();

    std::set_terminate(UnhandledExceptionHandler); // Set custom terminate handler for easier debugging
    WinUtils::Win32_UpdatePreloader();
#endif

    const auto startTime = std::chrono::system_clock::now();
    VerifyEnvironment();

    PythonManager& manager = PythonManager::GetInstance();
    g_pluginLoader = std::make_shared<PluginLoader>(startTime);

    /** Start the injection process into the Steam web helper */
    g_pluginLoader->StartBackEnds(manager);
    g_pluginLoader->StartFrontEnds(); /** IO blocking, returns once Steam dies */

    /** Shutdown backend service once frontend disconnects*/
    (&manager)->Shutdown();
}

__attribute__((constructor)) void __init_millennium()
{
#if defined(__linux__)
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1)
    {
        path[len] = '\0';
        const char* pathPtr = path;

        // Check if the path is the same as the Steam executable
        if (!IsSamePath(pathPtr, fmt::format("{}/.steam/steam/ubuntu12_32/steam", std::getenv("HOME")).c_str()))
        {
            return;
        }
    }
    else
    {
        perror("readlink");
        return;
    }
#endif

    try
    {
        SetupEnvironmentVariables();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to set up environment variables: {}", e.what());
        std::exit(EXIT_FAILURE);
    }
}

#ifdef _WIN32
std::thread g_millenniumThread;

void Win32_AttachMillennium(void)
{
    /** Starts the CEF arg hook, it doesn't wait for the hook to be installed, it waits for the hook to be setup */
    const bool hooked = HookCefArgs();
    if (!hooked)
    {
        /** Error already handled. */
        return;
    }

    EntryMain();

    /** Shutdown cron threads that manage Steam HTTP hooks */
    HttpHookManager& hookManager = HttpHookManager::get();
    (&hookManager)->Shutdown();

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    /** Deallocate the developer console */
    if (CommandLineArguments::HasArgument("-dev"))
    {
        FreeConsole();
    }
}

/**
 * Cleans up resources and gracefully shuts down Millennium on Windows.
 *
 * @warning This cleanup function assumes EntryMain has returned with success, otherwise this detach function will deadlock.
 * It depends on Python being uninitialized with all threads terminated, and all frontend hooks detached.
 */
void Win32_DetachMillennium(void)
{
    Logger.PrintMessage(" MAIN ", "Shutting Millennium down...", COL_MAGENTA);

    g_shouldTerminateMillennium->flag.store(true);
    Logger.Log("Set terminate flag");

    /** There should be 0% chance the socket is still somehow open, but we free it just in case to prevent deadlocks */
    Sockets::Shutdown();

    Logger.Log("Waiting for Millennium thread to exit...");

    if (!g_millenniumThread.joinable())
    {
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
extern "C" __attribute__((dllexport)) int __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
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

#elif defined(__linux__) || defined(__APPLE__)
#include "helpers.h"
#include <chrono>
#include <ctime>
#include <dlfcn.h>
#include <fstream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

extern "C"
{
    /* Trampoline for the real main() */
    static int (*fnMainOriginal)(int, char**, char**);
    std::unique_ptr<std::thread> g_millenniumThread;

    /**
     * Since this isn't an executable, and is "preloaded", the kernel doesn't implicitly load dependencies, so we need to manually.
     */
    static constexpr const char* __LIBPYTHON_RUNTIME_PATH = LIBPYTHON_RUNTIME_PATH;

    /** New interop funcs that receive calls from hooked libXtst */
    namespace HookInterop
    {
    int StartMillennium()
    {
        Logger.Log("Hooked main() with PID: {}", getpid());
        Logger.Log("Loading python libraries from {}", __LIBPYTHON_RUNTIME_PATH);

        if (!dlopen(__LIBPYTHON_RUNTIME_PATH, RTLD_LAZY | RTLD_GLOBAL))
        {
            LOG_ERROR("Failed to load python libraries: {},\n\nThis is likely because it was not found on disk, try reinstalling Millennium.", dlerror());
        }

        g_millenniumThread = std::make_unique<std::thread>(EntryMain);
        Logger.Log("Millennium started successfully.");
        return 0;
    }

    int StopMillennium()
    {
        Logger.Log("Unloading Millennium...");
        g_threadTerminateFlag->flag.store(true);

        Sockets::Shutdown();
        if (g_millenniumThread && g_millenniumThread->joinable())
        {
            g_millenniumThread->join();
        }

        Logger.Log("Millennium unloaded successfully.");
        return 0;
    }
    } // namespace HookInterop

    /* Our fake main() that gets called by __libc_start_main() */
    int MainHooked(int argc, char** argv, char** envp)
    {
        Logger.Log("Hooked main() with PID: {}", getpid());
        Logger.Log("Loading python libraries from {}", __LIBPYTHON_RUNTIME_PATH);

        if (!dlopen(__LIBPYTHON_RUNTIME_PATH, RTLD_LAZY | RTLD_GLOBAL))
        {
            LOG_ERROR("Failed to load python libraries: {},\n\nThis is likely because it was not found on disk, try reinstalling Millennium.", dlerror());
        }

#ifdef __APPLE__
        {
            EntryMain();
            Logger.Log("Shutting down Millennium...");

            return 0;
        }
#else
        {
#ifdef MILLENNIUM_SHARED
            {
                /** Start Millennium on a new thread to prevent I/O blocking */
                g_millenniumThread = std::make_unique<std::thread>(EntryMain);
                int steam_main = fnMainOriginal(argc, argv, envp);
                Logger.Log("Hooked Steam entry returned {}", steam_main);

                g_threadTerminateFlag->flag.store(true);
                Sockets::Shutdown();
                g_millenniumThread->join();

                Logger.Log("Shutting down Millennium...");
                return steam_main;
            }
#else
            {
                g_threadTerminateFlag->flag.store(true);
                g_millenniumThread = std::make_unique<std::thread>(EntryMain);
                g_millenniumThread->join();
                return 0;
            }
#endif
        }
#endif
    }

    void RemoveFromLdPreload()
    {
        const char* ldPreload = std::getenv("LD_PRELOAD");
        if (!ldPreload)
        {
            LOG_ERROR("LD_PRELOAD environment variable is not set, this shouldn't be possible?");
            return;
        }

        std::string ldPreloadStr(ldPreload);
        std::string millenniumPath = GetEnv("MILLENNIUM_RUNTIME_PATH");

        Logger.Log("Removing Millennium from LD_PRELOAD: {}", millenniumPath);

        // Tokenize the LD_PRELOAD string
        std::vector<std::string> tokens;
        std::stringstream ss(ldPreloadStr);
        std::string token;

        while (ss >> token)
        {
            if (token != millenniumPath)
            {
                tokens.push_back(token);
            }
        }

        std::string updatedLdPreload;
        for (size_t i = 0; i < tokens.size(); ++i)
        {
            if (i > 0)
                updatedLdPreload += " ";
            updatedLdPreload += tokens[i];
        }

        std::cout << "Updating LD_PRELOAD from [" << ldPreloadStr << "] to [" << updatedLdPreload << "]\n";

        if (setenv("LD_PRELOAD", updatedLdPreload.c_str(), 1) != 0)
        {
            std::perror("setenv");
        }
    }
#ifdef MILLENNIUM_SHARED

    int IsSamePath(const char* path1, const char* path2)
    {
        char realpath1[PATH_MAX], realpath2[PATH_MAX];
        struct stat stat1, stat2;

        // Get the real paths for both paths (resolves symlinks)
        if (realpath(path1, realpath1) == NULL)
        {
            perror("realpath failed for path1");
            return 0; // Error in resolving path
        }
        if (realpath(path2, realpath2) == NULL)
        {
            perror("realpath failed for path2");
            return 0; // Error in resolving path
        }

        // Compare resolved paths
        if (strcmp(realpath1, realpath2) != 0)
        {
            return 0; // Paths are different
        }

        // Check if both paths are symlinks and compare symlink targets
        if (lstat(path1, &stat1) == 0 && lstat(path2, &stat2) == 0)
        {
            if (S_ISLNK(stat1.st_mode) && S_ISLNK(stat2.st_mode))
            {
                // Both are symlinks, compare the target paths
                char target1[PATH_MAX], target2[PATH_MAX];
                ssize_t len1 = readlink(path1, target1, sizeof(target1) - 1);
                ssize_t len2 = readlink(path2, target2, sizeof(target2) - 1);

                if (len1 == -1 || len2 == -1)
                {
                    perror("readlink failed");
                    return 0;
                }

                target1[len1] = '\0';
                target2[len2] = '\0';

                // Compare the symlink targets
                if (strcmp(target1, target2) != 0)
                {
                    return 0; // Symlinks point to different targets
                }
            }
        }

        return 1; // Paths are the same, including symlinks to each other
    }

    /**
     * As of 1/7/2025 Steam offloads update checker to a child process. We don't want to hook that process.
     */
    bool IsChildUpdaterProc(int argc, char** argv)
    {
        for (int i = 0; i < argc; ++i)
        {
            if (strcmp(argv[i], "-child-update-ui") == 0 || strcmp(argv[i], "-child-update-ui-socket") == 0)
            {
                return true;
            }
        }
        return false;
    }

    /*
     * Trampoline for __libc_start_main() that replaces the real main
     * function with our hooked version.
     */
    int __libc_start_main(int (*main)(int, char**, char**), int argc, char** argv, int (*init)(int, char**, char**), void (*fini)(void), void (*rtld_fini)(void), void* stack_end)
    {
        /* Save the real main function address */
        fnMainOriginal = main;

        /* Get the address of the real __libc_start_main() */
        decltype(&__libc_start_main) orig = (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

        /** not loaded in a invalid child process */
        if (!IsSamePath(argv[0], GetEnv("MILLENNIUM__STEAM_EXE_PATH").c_str()) || IsChildUpdaterProc(argc, argv))
        {
            return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
        }

        Logger.Log("Hooked __libc_start_main() {} pid: {}", argv[0], getpid());

        /* Remove the Millennium library from LD_PRELOAD */
        RemoveFromLdPreload();
/* Log that we've loaded Millennium */
#ifdef __linux__
        Logger.Log("Loaded Millennium on {}, system architecture {}", GetLinuxDistro(), GetSystemArchitecture());
#endif
        /* ... and call it with our custom main function */
        return orig(MainHooked, argc, argv, init, fini, rtld_fini, stack_end);
    }
#endif
}

int main(int argc, char** argv, char** envp)
{
    signal(SIGTERM,
           [](int signalCode)
           {
               Logger.Warn("Received terminate signal...");
               g_threadTerminateFlag->flag.store(true);
           });

    int result = MainHooked(argc, argv, envp);
    Logger.Log("Millennium main returned: {}", result);

    return result;
}
#endif
