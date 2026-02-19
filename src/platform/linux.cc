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

#include "millennium/steam_hooks.h"
#include <memory>
#include <optional>
#if defined(__linux__) || defined(__APPLE__)
#include "instrumentation/smem.h"

#include "millennium/millennium.h"
#include "millennium/crash_handler.h"
#include "millennium/environment.h"
#include "millennium/plugin_loader.h"
#include "millennium/logger.h"

#include <ctime>
#include <dlfcn.h>
#include <fmt/core.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

std::unique_ptr<std::thread> g_millenniumThread;
extern std::mutex mtx_hasAllPythonPluginsShutdown, mtx_hasSteamUnloaded, mtx_hasSteamUIStartedLoading;
extern std::condition_variable cv_hasSteamUnloaded, cv_hasAllPythonPluginsShutdown, cv_hasSteamUIStartedLoading;

int IsSamePath(const char* path1, const char* path2)
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
        logger.warn("[Millennium] Process is not running under Steam: {}", path);
        return;
    }

    logger.log("[Millennium] Setting up environment for Steam process: {}", path);

    /** Setup environment variables if loaded into Steam process */
    platform::environment::setup();
}

DESTRUCTOR void Posix_UnInitializeEnvironment()
{
    /** destroy the shared memory pool between millennium and the web helper hook */
    if (g_lb_patch_arena) {
        platform::shared_memory::close(g_lb_patch_arena, SHM_IPC_SIZE);
        platform::shared_memory::unlink(SHM_IPC_NAME);
        g_lb_patch_arena = NULL;
    }
}

std::optional<std::filesystem::path> Posix_GetModuleBasePath()
{
    Dl_info info;
    if (dladdr((void*)Posix_GetModuleBasePath, &info)) {
        char resolved[PATH_MAX];
        if (realpath(info.dli_fname, resolved)) {
            std::filesystem::path p(resolved);
            return p.parent_path();
        }
    }
    return std::nullopt;
}

void Posix_AttachWebHelperHook()
{
    const char* existing = getenv("LD_PRELOAD");
    std::string hhx_path;

#ifdef MILLENNIUM_DEBUG
    hhx_path = __HOOK_HELPER_OUTPUT_ABSPATH__;
#else
    auto base_path = Posix_GetModuleBasePath();
    if (!base_path) {
        LOG_ERROR("[Posix_AttachWebHelperHook] Failed to get module base path");
        return;
    }
    hhx_path = (std::filesystem::path(*base_path) / __MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__);
#endif

    std::string new_value = existing ? std::string(existing) + ":" + hhx_path : hhx_path;

    setenv("LD_PRELOAD", new_value.c_str(), 1);
    logger.log("[Posix_AttachWebHelperHook] Setting LD_PRELOAD to {}", new_value);
}

void Posix_AttachMillennium()
{
    /** Handle signal interrupts (^C) */
    signal(SIGINT, [](int /** signalCode */) { std::exit(128 + SIGINT); });

    Plat_InitializeSteamHooks();
    Posix_AttachWebHelperHook();
    g_millennium = std::make_unique<millennium>();
    g_millennium->entry();
}

/** New interop funcs that receive calls from hooked libXtst */
extern "C" __attribute__((visibility("default"))) int StartMillennium()
{
    logger.log("Hooked main() with PID: {}", getpid());
    logger.log("Loading python libraries from {}", LIBPYTHON_RUNTIME_PATH);

    if (!dlopen(LIBPYTHON_RUNTIME_PATH, RTLD_LAZY | RTLD_GLOBAL)) {
        LOG_ERROR("Failed to load python libraries: {},\n\nThis is likely because it was not found on disk, try reinstalling Millennium.", dlerror());
    }

    g_millenniumThread = std::make_unique<std::thread>(Posix_AttachMillennium);
    logger.log("Millennium started successfully.");
    return 0;
}

extern "C" __attribute__((visibility("default"))) int StopMillennium()
{
    logger.log("Unloading Millennium...");
    g_shouldTerminateMillennium->flag.store(true);

    if (g_millenniumThread && g_millenniumThread->joinable()) {
        g_millenniumThread->join();
    }

    g_millennium.reset();

    logger.log("Millennium unloaded successfully.");
    return 0;
}
#endif
