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
#include "millennium/backend_mgr.h"
#include "millennium/crash_handler.h"
#include "millennium/env.h"
#include "millennium/init.h"
#include "millennium/logger.h"
#include "millennium/ffi.h"

#include <ctime>
#include <dlfcn.h>
#include <fmt/core.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

std::unique_ptr<std::thread> g_millenniumThread;

const static void VerifyEnvironment();
void EntryMain(); /** forward declare main function */

extern std::mutex mtx_hasAllPythonPluginsShutdown, mtx_hasSteamUnloaded, mtx_hasSteamUIStartedLoading;
extern std::condition_variable cv_hasSteamUnloaded, cv_hasAllPythonPluginsShutdown, cv_hasSteamUIStartedLoading;

static int IsSamePath(const char* path1, const char* path2)
{
    char realpath1[PATH_MAX], realpath2[PATH_MAX];

    if (realpath(path1, realpath1) == NULL || realpath(path2, realpath2) == NULL) {
        LOG_ERROR("Failed to resolve paths: {} or {}", path1, path2);
        return 0;
    }

    return strcmp(realpath1, realpath2) == 0;
}

CONSTRUCTOR void Posix_InitializeEnvironment()
{
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);

    if (len == -1) {
        perror("readlink");
        return;
    }

    path[len] = '\0';

    std::string steamPath = fmt::format("{}/.steam/steam/ubuntu12_32/steam", std::getenv("HOME"));
    if (!IsSamePath(path, steamPath.c_str())) {
        Logger.Warn("[Millennium] Process is not running under Steam: {}", path);
        return;
    }

    Logger.Log("[Millennium] Setting up environment for Steam process: {}", path);

    /** Setup environment variables if loaded into Steam process */
    SetupEnvironmentVariables();
}

void Posix_AttachMillennium()
{
    /** Handle signal interrupts (^C) */
    signal(SIGINT, [](int signalCode) { std::exit(128 + SIGINT); });

    EntryMain();

    /** Shutdown the shared JS message emitter */
    CefSocketDispatcher& emitter = CefSocketDispatcher::get();
    (&emitter)->Shutdown();

    /** Shutdown cron threads that manage Steam HTTP hooks */
    BackendManager& hookManager = BackendManager::GetInstance();
    (&hookManager)->Shutdown();

    std::unique_lock<std::mutex> lk(mtx_hasSteamUnloaded);
    cv_hasSteamUnloaded.wait(lk, [&hookManager]()
    {
        /** wait for all backends to stop so we can safely free the loader lock */
        if (hookManager.HasAllPythonBackendsStopped() && hookManager.HasAllLuaBackendsStopped()) {
            Logger.Warn("All backends have stopped, proceeding with termination...");

            std::unique_lock<std::mutex> lk2(mtx_hasAllPythonPluginsShutdown);
            cv_hasAllPythonPluginsShutdown.notify_all();
            return true;
        }
        return false;
    });
}

/** New interop funcs that receive calls from hooked libXtst */
extern "C" __attribute__((visibility("default"))) int StartMillennium()
{
    Logger.Log("Hooked main() with PID: {}", getpid());
    Logger.Log("Loading python libraries from {}", LIBPYTHON_RUNTIME_PATH);

    if (!dlopen(LIBPYTHON_RUNTIME_PATH, RTLD_LAZY | RTLD_GLOBAL)) {
        LOG_ERROR("Failed to load python libraries: {},\n\nThis is likely because it was not found on disk, try reinstalling Millennium.", dlerror());
    }

    g_millenniumThread = std::make_unique<std::thread>(Posix_AttachMillennium);
    Logger.Log("Millennium started successfully.");
    return 0;
}

extern "C" __attribute__((visibility("default"))) int StopMillennium()
{
    Logger.Log("Unloading Millennium...");
    g_shouldTerminateMillennium->flag.store(true);

    Sockets::Shutdown();
    if (g_millenniumThread && g_millenniumThread->joinable()) {
        g_millenniumThread->join();
    }

    Logger.Log("Millennium unloaded successfully.");
    return 0;
}
#endif
