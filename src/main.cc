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
#include "millennium/backend_mgr.h"
#include "millennium/crash_handler.h"
#include "millennium/env.h"
#include "millennium/init.h"
#include "millennium/logger.h"
#include "millennium/millennium_updater.h"

#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <signal.h>

void RemoveDeprecatedFile(const std::filesystem::path& filePath)
{
    if (std::filesystem::exists(filePath)) {
        try {
            if (std::filesystem::is_directory(filePath)) {
                std::filesystem::remove_all(filePath);
                Logger.Log("Removed deprecated directory: {}", filePath.string());
            } else {
                std::filesystem::remove(filePath);
                Logger.Log("Removed deprecated file: {}", filePath.string());
            }
        } catch (const std::filesystem::filesystem_error& e) {
            LOG_ERROR("Failed to remove deprecated file or directory {}: {}", filePath.string(), e.what());
            MessageBoxA(NULL, fmt::format("Failed to remove deprecated file or directory: {}\nPlease remove it manually and restart Steam.", filePath.string()).c_str(),
                        "Startup Error", MB_ICONERROR | MB_OK);
        }
    }
}

/**
 * @brief Verify the environment to ensure that the CEF remote debugging is enabled.
 * .cef-enable-remote-debugging is a special file name that Steam uses to signal CEF to enable remote debugging.
 */
const static void VerifyEnvironment()
{
#ifdef _WIN32
    /** Check if the user has set a Steam.cfg file to block updates, this is incompatible with Millennium as Millennium relies on the latest version of Steam. */
    const auto steamUpdateBlock = SystemIO::GetSteamPath() / "Steam.cfg";

    const std::string errorMessage = fmt::format(
        "Millennium is incompatible with your {} config. This is a file you likely created to block Steam updates. In order for Millennium to properly function, remove it.",
        steamUpdateBlock.string());

    if (std::filesystem::exists(steamUpdateBlock)) {
        try {
            std::string steamConfig = SystemIO::ReadFileSync(steamUpdateBlock.string());

            std::vector<std::string> blackListedKeys = {
                "BootStrapperInhibitAll",
                "BootStrapperForceSelfUpdate",
                "BootStrapperInhibitClientChecksum",
                "BootStrapperInhibitBootstrapperChecksum",
                "BootStrapperInhibitUpdateOnLaunch",
            };

            for (const auto& key : blackListedKeys) {
                if (steamConfig.find(key) != std::string::npos) {
                    throw SystemIO::FileException("Steam.cfg contains blacklisted keys");
                }
            }
        } catch (const SystemIO::FileException& e) {
            MessageBoxA(NULL, errorMessage.c_str(), "Startup Error", MB_ICONERROR | MB_OK);
            LOG_ERROR(errorMessage);
        }
    }

    /** Remove deprecated Millennium shim file */
    RemoveDeprecatedFile(SystemIO::GetSteamPath() / "user32.dll");
    /** Remove deprecated CEF remote debugging file in place of virtually enabling it in memory */
    RemoveDeprecatedFile(SystemIO::GetSteamPath() / ".cef-enable-remote-debugging");
    /** Remove old shims folder (they've been added into process memory) */
    RemoveDeprecatedFile(SystemIO::GetInstallPath() / "ext" / "data" / "shims");
#endif
}

/**
 * @brief Millennium's main method, called on startup on both Windows and Linux.
 */
void EntryMain()
{
    std::thread updateThread(MillenniumUpdater::CheckForUpdates);

    const auto startTime = std::chrono::system_clock::now();
    VerifyEnvironment();

    BackendManager& manager = BackendManager::GetInstance();
    g_pluginLoader = std::make_shared<PluginLoader>(startTime);

    /** Start the injection process into the Steam web helper */
    g_pluginLoader->StartBackEnds(manager);
    g_pluginLoader->StartFrontEnds(); /** IO blocking, returns once Steam dies */

    /** Shutdown backend service once frontend disconnects*/
    (&manager)->Shutdown();

    /** Wait for the update thread to finish */
    updateThread.join();
}
