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

#include "head/library_updater.h"
#include "millennium/sysfs.h"

#include "millennium/http.h"
#include "millennium/logger.h"

Updater::Updater() : api_url("https://steambrew.app/api/checkupdates"), has_checked_for_updates(false)
{
    if (!CONFIG.GetNested("general.checkForPluginAndThemeUpdates").get<bool>()) {
        Logger.Warn("User has disabled update checking for plugins and themes.");
        return;
    }

    cached_updates = CheckForUpdates();
    has_checked_for_updates = true;
}

bool Updater::DownloadPluginUpdate(const std::string& id, const std::string& name)
{
    return plugin_updater.DownloadPluginUpdate(id, name);
}

bool Updater::DownloadThemeUpdate(std::shared_ptr<ThemeConfig> themeConfig, const std::string& native)
{
    return theme_updater.UpdateTheme(themeConfig, native);
}

std::optional<json> Updater::GetCachedUpdates() const
{
    return cached_updates;
}

bool Updater::HasCheckedForUpdates() const
{
    return has_checked_for_updates;
}

std::optional<json> Updater::CheckForUpdates(bool force)
{
    #ifdef DISTRO_NIX
        Logger.Log("Skipping update check on Nix-based releases");
        return json{
            { "themes",  {} },
            { "plugins", {} }
        };
    #endif

    try {
        if (!force && cached_updates.has_value()) {
            Logger.Log("Using cached updates.");
            return cached_updates;
        }

        auto plugins = plugin_updater.GetRequestBody();
        auto themes = theme_updater.GetRequestBody();

        json request_body;

        if (!plugins.empty()) request_body["plugins"] = plugins;

        if (themes.contains("post_body") && themes["post_body"].is_array() && !themes["post_body"].empty()) request_body["themes"] = themes["post_body"];

        if (request_body.empty()) {
            Logger.Log("No themes or plugins to update!");
            return json{
                { "themes",  {} },
                { "plugins", {} }
            };
        }

        auto response_str = Http::Post(api_url.c_str(), request_body.dump());
        json resp = json::parse(response_str);

        if (resp.contains("themes") && !resp["themes"].empty() && !resp["themes"].contains("error")) {
            resp["themes"] = theme_updater.ProcessUpdates(themes["update_query"], resp["themes"]);
        }

        cached_updates = resp;
        has_checked_for_updates = true;
        return resp;
    } catch (const std::exception& e) {
        Logger.Log(std::string("An error occurred while checking for updates: ") + e.what());
        return json{
            { "themes", { { "error", e.what() } } }
        };
    }
}

std::string Updater::ResyncUpdates()
{
    Logger.Log("Resyncing updates...");
    cached_updates = CheckForUpdates(true);
    has_checked_for_updates = true;
    Logger.Log("Resync complete.");
    return cached_updates.has_value() ? cached_updates->dump() : "{}";
}

ThemeInstaller& Updater::GetThemeUpdater()
{
    return theme_updater;
}

PluginInstaller& Updater::GetPluginUpdater()
{
    return plugin_updater;
}
