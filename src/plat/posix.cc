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

#if defined(__linux__) || defined(__APPLE__)
#include "co_spawn.h"
#include "crash_handler.h"
#include "executor.h"
#include "helpers.h"
#include "loader.h"
#include "terminal_pipe.h"
#include <chrono>
#include <ctime>
#include <dlfcn.h>
#include <env.h>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <internal_logger.h>
#include <signal.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

const static void VerifyEnvironment();

__attribute__((constructor)) void Posix_InitializeEnvironment()
{
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
}

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
        EntryMain();
        Logger.Log("Shutting down Millennium...");

        return 0;
#else
        /** Start Millennium on a new thread to prevent I/O blocking */
        g_millenniumThread = std::make_unique<std::thread>(EntryMain);
        int steam_main = fnMainOriginal(argc, argv, envp);
        Logger.Log("Hooked Steam entry returned {}", steam_main);

        g_threadTerminateFlag->flag.store(true);
        Sockets::Shutdown();
        g_millenniumThread->join();

        Logger.Log("Shutting down Millennium...");
        return steam_main;
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
        fnMainOriginal = main;
        decltype(&__libc_start_main) orig = (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

        if (!IsSamePath(argv[0], GetEnv("MILLENNIUM__STEAM_EXE_PATH").c_str()) || IsChildUpdaterProc(argc, argv))
        {
            return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
        }

        Logger.Log("Hooked __libc_start_main() {} pid: {}", argv[0], getpid());
        RemoveFromLdPreload();

#ifdef __linux__
        Logger.Log("Loaded Millennium on {}, system architecture {}", GetLinuxDistro(), GetSystemArchitecture());
#endif
        return orig(MainHooked, argc, argv, init, fini, rtld_fini, stack_end);
    }
}

int main(int argc, char** argv, char** envp)
{
    signal(SIGTERM, [](int signalCode)
    {
        Logger.Warn("Received terminate signal...");
        g_threadTerminateFlag->flag.store(true);
    });

    int result = MainHooked(argc, argv, envp);
    Logger.Log("Millennium main returned: {}", result);

    return result;
}
#endif