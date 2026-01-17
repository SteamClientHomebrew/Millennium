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

#include "head/entry_point.h"
#include "hhx64/smem.h"
#include "head/default_cfg.h"

#include "millennium/crash_handler.h"
#include "millennium/env.h"
#include "millennium/plugin_loader.h"
#include "millennium/logger.h"
#include "millennium/millennium_updater.h"
#include "millennium/millennium.h"

#include <filesystem>
#include <fmt/core.h>

std::unique_ptr<millennium> g_millennium;

millennium::millennium()
{
    m_settings_store = std::make_shared<SettingsStore>();
    m_millennium_updater = std::make_shared<millennium_updater>();
    m_plugin_loader = std::make_shared<plugin_loader>(m_settings_store, m_millennium_updater);

    m_millennium_updater->win32_update_legacy_shims();
    this->check_for_updates();
    m_millennium_updater->cleanup();
}

std::shared_ptr<plugin_loader> millennium::get_plugin_loader()
{
    return this->m_plugin_loader;
}

std::shared_ptr<SettingsStore> millennium::get_settings_store()
{
    return this->m_settings_store;
}

void millennium::check_health()
{
    const auto cefRemoteDebugging = SystemIO::GetSteamPath() / ".cef-enable-remote-debugging";
#ifdef __linux__
    if (std::filesystem::exists(cefRemoteDebugging) && !std::filesystem::remove(cefRemoteDebugging)) {
        LOG_ERROR("Failed to remove '{}', likely non-fatal but manual intervention recommended.", cefRemoteDebugging.string());
    }
#elif _WIN32
    const auto steam_cfg = SystemIO::GetSteamPath() / "Steam.cfg";
    const auto bootstrap_error = fmt::format("Millennium is incompatible with your {} config. Remove this file to allow Steam updates.", steam_cfg.string());
    const auto cef_error = fmt::format("Failed to remove deprecated file: {}\nRemove manually and restart Steam.", cefRemoteDebugging.string());

    if (std::filesystem::exists(steam_cfg)) {
        try {
            const std::string steamConfig = SystemIO::ReadFileSync(steam_cfg.string());
            static const std::vector<std::string> blackListedKeys = {
                "BootStrapperInhibitAll",
                "BootStrapperForceSelfUpdate",
                "BootStrapperInhibitClientChecksum",
                "BootStrapperInhibitBootstrapperChecksum",
                "BootStrapperInhibitUpdateOnLaunch",
            };

            for (const auto& key : blackListedKeys) {
                if (steamConfig.find(key) != std::string::npos) {
                    Plat_ShowMessageBox("Startup Error", bootstrap_error.c_str(), MESSAGEBOX_ERROR);
                    LOG_ERROR(bootstrap_error);
                    break;
                }
            }
        } catch (const SystemIO::FileException&) {
            Plat_ShowMessageBox("Startup Error", bootstrap_error.c_str(), MESSAGEBOX_ERROR);
            LOG_ERROR(bootstrap_error);
        }
    }

    if (std::filesystem::exists(cefRemoteDebugging)) {
        try {
            std::filesystem::remove(cefRemoteDebugging);
        } catch (const std::filesystem::filesystem_error& e) {
            LOG_ERROR("Failed to remove deprecated file {}: {}", cefRemoteDebugging.string(), e.what());
            Plat_ShowMessageBox("Startup Error", cef_error.c_str(), MESSAGEBOX_ERROR);
        }
    }
#endif
}

void millennium::check_for_updates()
{
    try {
        m_millennium_updater->check_for_updates();

        const auto update = m_millennium_updater->has_any_updates();
        const bool should_auto_install = CONFIG.GetNested("general.onMillenniumUpdate", OnMillenniumUpdate::AUTO_INSTALL) == OnMillenniumUpdate::AUTO_INSTALL;

        if (!update["hasUpdate"]) {
            Logger.Log("No Millennium updates available.");
            return;
        }

        const std::string new_version = update.value("newVersion", nlohmann::json::object()).value("tag_name", std::string("unknown"));

        if (!should_auto_install) {
            Logger.Log("Millennium update available to version {}. Auto-install is disabled, please update manually.", new_version);
            return;
        }

        const std::string download_url = update["platformRelease"]["browser_download_url"];
        const size_t download_size = update["platformRelease"]["size"].get<size_t>();

        Logger.Log("Auto-updating Millennium to version {}...", new_version);
        m_millennium_updater->update(download_url, download_size, false); // TODO: Removed should forward flag, check if it works.

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to check for Millennium updates: {}", e.what());
    } catch (...) {
        LOG_ERROR("Failed to check for Millennium updates: unknown error");
    }
}

/**
 * Millennium main entry point.
 * This entry point is called on posix and windows.
 */
void millennium::entry()
{
    shm_init_simple();
    this->check_health();

    m_plugin_loader->start_plugin_backends();
    m_plugin_loader->start_plugin_frontends();
}
