/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2023 - 2026. Project Millennium
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

#ifdef __linux__
#include "millennium/env.h"
#include "millennium/init.h"
#include "millennium/logger.h"
#include "millennium/posix_util.h"

#include <dlfcn.h>
#include <unistd.h>

#include <regex>
#include <thread>

/* Trampoline for the real main() */
static int (*fnMainOriginal)(int, char**, char**);

/** fwd decl of main thread: from new init_posix.cc */
extern std::unique_ptr<std::thread> g_millenniumThread;

/** fwd decl of attach function */
void Posix_AttachMillennium();

/** fwd decl of path comparison function */
int IsSamePath(const char* path1, const char* path2);

/**
 * After Millennium has been loaded,
 * we need to remove it from LD_PRELOAD to prevent issues with child processes.
 */
void RemoveFromLdPreload()
{
    const char* ldPreload = std::getenv("LD_PRELOAD");
    if (!ldPreload) {
        LOG_ERROR("LD_PRELOAD environment variable is not set, this shouldn't be possible?");
        return;
    }

    std::string ldPreloadStr(ldPreload);
    std::string millenniumPath = GetEnv("MILLENNIUM_RUNTIME_PATH");
    Logger.Log("Removing Millennium from LD_PRELOAD: {}", millenniumPath);

    size_t pos = 0;
    while ((pos = ldPreloadStr.find(millenniumPath, pos)) != std::string::npos) {
        ldPreloadStr.erase(pos, millenniumPath.length());
    }

    std::regex spaceRegex("\\s+");
    std::string updatedLdPreload = std::regex_replace(ldPreloadStr, spaceRegex, " ");

    updatedLdPreload.erase(0, updatedLdPreload.find_first_not_of(' '));
    updatedLdPreload.erase(updatedLdPreload.find_last_not_of(' ') + 1);

    std::cout << "Updating LD_PRELOAD from [" << ldPreload << "] to [" << updatedLdPreload << "]\n";

    if (setenv("LD_PRELOAD", updatedLdPreload.c_str(), 1) != 0) {
        std::perror("setenv");
    }
}

/**
 * Called from the legacy __libc_start_main hook,
 * this starts Millennium on a new thread to prevent blocking Steam's main thread.
 *
 * @deprecated This function is deprecated and is being replaced with module shimming.
 */
int Deprecated_HookedMain(int argc, char** argv, char** envp)
{
    RemoveFromLdPreload();
    Logger.Log("Hooked main() with PID: {}", getpid());
    Logger.Log("Loading python libraries from {}", LIBPYTHON_RUNTIME_PATH);

    if (!dlopen(LIBPYTHON_RUNTIME_PATH, RTLD_LAZY | RTLD_GLOBAL)) {
        LOG_ERROR("Failed to load python libraries: {},\n\nThis is likely because it was not found on disk, try reinstalling Millennium.", dlerror());
    }

    /** Start Millennium on a new thread to prevent I/O blocking */
    g_millenniumThread = std::make_unique<std::thread>(Posix_AttachMillennium);
    int steam_main = fnMainOriginal(argc, argv, envp);
    Logger.Log("Hooked Steam entry returned {}", steam_main);

    g_shouldTerminateMillennium->flag.store(true);
    Sockets::Shutdown();
    g_millenniumThread->join();

    Logger.Log("Shutting down Millennium...");
    return steam_main;
}

/**
 * As of 1/7/2025 Steam offloads update checker to a child process. We don't want to hook that process.
 */
bool IsChildUpdaterProc(int argc, char** argv)
{
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "-child-update-ui") == 0 || strcmp(argv[i], "-child-update-ui-socket") == 0) {
            return true;
        }
    }
    return false;
}

/*
 * Trampoline for __libc_start_main() that replaces the real main
 * function with our hooked version.
 *
 * @deprecated This method of hooking is simply here for compatibility with NixOS.
 */
extern "C" int __libc_start_main(int (*main)(int, char**, char**), int argc, char** argv, int (*init)(int, char**, char**), void (*fini)(void), void (*rtld_fini)(void),
                                 void* stack_end)
{
    fnMainOriginal = main;
    decltype(&__libc_start_main) orig = (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

    if (!IsSamePath(argv[0], GetEnv("MILLENNIUM__STEAM_EXE_PATH").c_str()) || IsChildUpdaterProc(argc, argv)) {
        return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
    }

    Logger.Log("Hooked __libc_start_main() {} pid: {}", argv[0], getpid());

#ifdef __linux__
    Logger.Log("Loaded Millennium on {}, system architecture {}", GetLinuxDistro(), GetSystemArchitecture());
#endif
    return orig(Deprecated_HookedMain, argc, argv, init, fini, rtld_fini, stack_end);
}

#endif
