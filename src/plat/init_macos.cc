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

#ifdef __APPLE__
#include "hhx64/smem.h"
#include "millennium/backend_mgr.h"
#include "millennium/crash_handler.h"
#include "millennium/env.h"
#include "millennium/init.h"
#include "millennium/logger.h"
#include "millennium/steam_hooks.h"

#include <limits.h>
#include <mach-o/dyld.h>

#include <ctime>
#include <chrono>
#include <dlfcn.h>
#include <filesystem>
#include <fmt/core.h>
#include <optional>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

std::unique_ptr<std::thread> g_millenniumThread;

void VerifyEnvironment();

/** forward declare main function */
void EntryMain();
void Plat_CheckForUpdates();
extern "C" void Plat_InstallMacOSMenuItems();

extern std::mutex mtx_hasAllPythonPluginsShutdown, mtx_hasSteamUnloaded;
extern std::condition_variable cv_hasSteamUnloaded, cv_hasAllPythonPluginsShutdown;

static std::optional<std::filesystem::path> GetCurrentExecutablePath()
{
    char path_buffer[PATH_MAX];
    uint32_t size = sizeof(path_buffer);

    if (_NSGetExecutablePath(path_buffer, &size) != 0) {
        return std::nullopt;
    }

    char resolved[PATH_MAX];
    if (!realpath(path_buffer, resolved)) {
        return std::nullopt;
    }

    return std::filesystem::path(resolved);
}

static bool IsSteamMainExecutable(const std::filesystem::path& executable_path)
{
    const auto filename = executable_path.filename().string();
    /* The wrapper preserves the user-facing name, but the exec'd target is steam_osx.real. */
    return filename == "steam_osx" || filename == "steam_osx.real";
}

CONSTRUCTOR void MacOS_InitializeEnvironment()
{
    const auto executable_path = GetCurrentExecutablePath();
    if (!executable_path) {
        Logger.Warn("[Millennium] Failed to resolve executable path while setting up macOS environment.");
        return;
    }

    if (!IsSteamMainExecutable(*executable_path)) {
        Logger.Warn("[Millennium] Process is not running under Steam: {}", executable_path->string());
        return;
    }

    SetupEnvironmentVariables();
    Logger.Log("[Millennium] Setting up environment for Steam process: {}", executable_path->string());
}

DESTRUCTOR void MacOS_UnInitializeEnvironment()
{
    if (g_lb_patch_arena) {
        shm_arena_close(g_lb_patch_arena, SHM_IPC_SIZE);
        shm_arena_unlink(SHM_IPC_NAME);
        g_lb_patch_arena = NULL;
    }
}

static void MacOS_AttachMillennium()
{
    signal(SIGINT, [](int) { std::exit(128 + SIGINT); });
    Plat_InstallMacOSMenuItems();
    if (!Plat_InitializeSteamHooks()) {
        Logger.Warn("macOS Steam helper injection is currently inactive because the wrapper installation could not be verified.");
    }
    std::thread([]()
    {
        std::this_thread::sleep_for(std::chrono::seconds(8));
        Plat_CheckForUpdates();
    }).detach();
    EntryMain();

    BackendManager& manager = BackendManager::GetInstance();
    std::unique_lock<std::mutex> lk(mtx_hasSteamUnloaded);

    cv_hasSteamUnloaded.wait(lk, [&manager]()
    {
        if (manager.HasAllPythonBackendsStopped() && manager.HasAllLuaBackendsStopped()) {
            Logger.Warn("All backends have stopped, proceeding with termination...");

            std::unique_lock<std::mutex> lk2(mtx_hasAllPythonPluginsShutdown);
            cv_hasAllPythonPluginsShutdown.notify_all();
            return true;
        }
        return false;
    });
}

extern "C" __attribute__((visibility("default"))) int StartMillennium()
{
    Logger.Log("Hooked main() with PID: {}", getpid());
    Logger.Log("Loading python libraries from {}", LIBPYTHON_RUNTIME_PATH);

    if (!dlopen(LIBPYTHON_RUNTIME_PATH, RTLD_LAZY | RTLD_GLOBAL)) {
        LOG_ERROR("Failed to load python libraries: {}", dlerror());
        return -1;
    }

    g_millenniumThread = std::make_unique<std::thread>(MacOS_AttachMillennium);
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
