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

#include "hhx64/smem.h"
#define UNICODE
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#define _WINSOCKAPI_
#endif
#include "millennium/backend_mgr.h"
#include "millennium/env.h"
#include "millennium/init.h"
#include "millennium/logger.h"
#include "head/default_cfg.h"
#include "millennium/millennium_updater.h"

#include <filesystem>
#include <fmt/format.h>

extern std::mutex mtx_hasSteamUnloaded;
extern std::condition_variable cv_hasSteamUnloaded;

static void VerifyEnvironment()
{
    const auto cefRemoteDebugging = SystemIO::GetSteamPath() / ".cef-enable-remote-debugging";

#ifdef __linux__
    if (exists(cefRemoteDebugging)) {
        /** Remove remote debugger flag if its enabled. We don't need it anymore (2025-10-22) */
        if (!std::filesystem::remove(cefRemoteDebugging)) {
            LOG_ERROR("Failed to remove '{}', this is likely non-fatal but manual intervention is recommended.", cefRemoteDebugging.string());
        }
    }
#elif _WIN32
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
            Plat_ShowMessageBox("Startup Error", errorMessage.c_str(), MESSAGEBOX_ERROR);
            LOG_ERROR(errorMessage);
        }
    }

#ifdef MILLENNIUM_64BIT
    const auto RemoveDeprecatedFile = [](const std::filesystem::path& filePath)
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
                Plat_ShowMessageBox("Startup Error",
                                    fmt::format("Failed to remove deprecated file or directory: {}\nPlease remove it manually and restart Steam.", filePath.string()).c_str(),
                                    MESSAGEBOX_ERROR);
            }
        }
    };

    /** Remove deprecated CEF remote debugging file in place of virtually enabling it in memory */
    RemoveDeprecatedFile(cefRemoteDebugging);
    /** Remove old shims folder (they've been added into process memory) */
    // RemoveDeprecatedFile(SystemIO::GetInstallPath() / "ext" / "data" / "shims");
#elif defined(MILLENNIUM_32BIT)
    if (!std::filesystem::exists(cefRemoteDebugging)) {
        std::ofstream file(cefRemoteDebugging);
        if (!file) {
            LOG_ERROR("Failed to create {}", cefRemoteDebugging.string());
            Plat_ShowMessageBox("Fatal Error", fmt::format("Failed to create remote debugger file! Manually create: {}", cefRemoteDebugging.string()).c_str(), MESSAGEBOX_ERROR);
            return;
        }
        file.close();
        Plat_ShowMessageBox("Restart Required!", "Please restart Steam to complete Millennium setup!", MESSAGEBOX_INFO);
    }
#endif
#endif
}

/**
 * Called on startup to check for Millennium updates
 */
void Plat_CheckForUpdates()
{
    try {
        MillenniumUpdater::CheckForUpdates();

        const auto update = MillenniumUpdater::HasAnyUpdates();
        const bool shouldAutoInstall = CONFIG.GetNested("general.onMillenniumUpdate", OnMillenniumUpdate::AUTO_INSTALL) == OnMillenniumUpdate::AUTO_INSTALL;

        if (!update["hasUpdate"]) {
            Logger.Log("No Millennium updates available.");
            return;
        }

        const std::string newVersion = update.value("newVersion", nlohmann::json::object()).value("tag_name", std::string("unknown"));

        if (!shouldAutoInstall) {
            Logger.Log("Millennium update available to version {}. Auto-install is disabled, please update manually.", newVersion);
            return;
        }

        const std::string downloadUrl = update["platformRelease"]["browser_download_url"];
        const size_t downloadSize = update["platformRelease"]["size"].get<size_t>();

        Logger.Log("Auto-updating Millennium to version {}...", newVersion);
        MillenniumUpdater::StartUpdate(downloadUrl, downloadSize, false, false);

    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("Failed to check for Millennium updates: {}", e.what());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to check for Millennium updates: {}", e.what());
    } catch (...) {
        LOG_ERROR("Failed to check for Millennium updates: unknown error");
    }
}

/**
 * @brief Millennium's main method, called on startup on both Windows and Linux.
 */
void EntryMain()
{
    shm_init_simple();

    const auto startTime = std::chrono::system_clock::now();
    VerifyEnvironment();

    BackendManager& manager = BackendManager::GetInstance();
    g_pluginLoader = std::make_shared<PluginLoader>(startTime);

    /** Start the injection process into the Steam web helper */
    g_pluginLoader->StartBackEnds(manager);
    g_pluginLoader->StartFrontEnds(); /** IO blocking, returns once Steam dies */

#ifdef _WIN32
    /** Shutdown backend service once frontend disconnects*/
    Logger.Log("Shutting down backend services...");
    (&manager)->Shutdown();
    Logger.Log("Backend services shut down successfully.");
#endif
    Logger.Log("Finished shutting down frontend and backend...");

    {
        /** new scope to prevent deadlocks */
        std::unique_lock<std::mutex> lk(mtx_hasSteamUnloaded);
        cv_hasSteamUnloaded.notify_all();
    }
}
