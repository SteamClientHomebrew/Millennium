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
#include "head/entry_point.h"
#include "head/plugin_mgr.h"
#include "head/theme_mgr.h"

#include "millennium/sysfs.h"
#include "millennium/core_ipc.h"
#include "millennium/http.h"
#include "millennium/logger.h"
#include <memory>

library_updater::library_updater(std::weak_ptr<millennium_backend> millennium_backend, std::shared_ptr<ipc_main> ipc_main)
    : m_millennium_backend(std::move(millennium_backend)), m_ipc_main(std::move(ipc_main)), m_has_checked_for_updates(false)
{
}

void library_updater::init(std::shared_ptr<SettingsStore> settings_store_ptr)
{
    theme_updater = std::make_shared<theme_installer>(settings_store_ptr, shared_from_this());
    plugin_updater = std::make_shared<plugin_installer>(m_millennium_backend, settings_store_ptr, shared_from_this());

    if (!CONFIG.GetNested("general.checkForPluginAndThemeUpdates").get<bool>()) {
        Logger.Warn("User has disabled update checking for plugins and themes.");
        return;
    }

    cached_updates = check_for_updates();
    m_has_checked_for_updates = true;
}

bool library_updater::download_plugin_update(const std::string& id, const std::string& name)
{
    return plugin_updater->update_plugin(id, name);
}

bool library_updater::download_theme_update(std::shared_ptr<ThemeConfig> themeConfig, const std::string& native)
{
    return theme_updater->update_theme(themeConfig, native);
}

std::optional<json> library_updater::get_cached_updates() const
{
    return cached_updates;
}

bool library_updater::has_checked_for_updates() const
{
    return m_has_checked_for_updates;
}

std::optional<json> library_updater::check_for_updates(bool force)
{
    try {
        if (!force && cached_updates.has_value()) {
            Logger.Log("Using cached updates.");
            return cached_updates;
        }

        auto plugins = plugin_updater->get_updater_request_body();
        auto themes = theme_updater->get_request_body();

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
            resp["themes"] = theme_updater->process_update(themes["update_query"], resp["themes"]);
        }

        cached_updates = resp;
        m_has_checked_for_updates = true;
        return resp;
    } catch (const std::exception& e) {
        Logger.Log(std::string("An error occurred while checking for updates: ") + e.what());
        return json{
            { "themes", { { "error", e.what() } } }
        };
    }
}

std::string library_updater::re_check_for_updates()
{
    Logger.Log("Resyncing updates...");
    cached_updates = check_for_updates(true);
    m_has_checked_for_updates = true;
    Logger.Log("Resync complete.");
    return cached_updates.has_value() ? cached_updates->dump() : "{}";
}

std::weak_ptr<theme_installer> library_updater::get_theme_updater()
{
    return theme_updater;
}

std::weak_ptr<plugin_installer> library_updater::get_plugin_updater()
{
    return plugin_updater;
}

void library_updater::dispatch_progress(const std::string& status, double progress, bool is_complete)
{
    std::vector<ipc_main::javascript_parameter> params = { status, progress, is_complete };
    m_ipc_main->evaluate_javascript_expression(m_ipc_main->compile_javascript_expression("core", "InstallerMessageEmitter", params));
}
