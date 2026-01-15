/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
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

#pragma once

#include "millennium/fwd_decl.h"
#include "head/plugin_mgr.h"
#include "head/theme_mgr.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

using json = nlohmann::json;

class Updater : public std::enable_shared_from_this<Updater>
{
  public:
    Updater(std::weak_ptr<millennium_backend> millennium_backend, std::shared_ptr<ipc_main> ipc_main);
    void init(std::shared_ptr<SettingsStore> settings_store_ptr);

    bool DownloadPluginUpdate(const std::string& id, const std::string& name);
    bool DownloadThemeUpdate(std::shared_ptr<ThemeConfig> themeConfig, const std::string& native);

    std::optional<json> GetCachedUpdates() const;
    bool HasCheckedForUpdates() const;

    std::optional<json> CheckForUpdates(bool force = false);
    std::string ResyncUpdates();

    std::shared_ptr<theme_installer> GetThemeUpdater();
    std::shared_ptr<plugin_installer> GetPluginUpdater();

    void dispatch_progress(const std::string& status, double progress, bool is_complete);

  private:
    std::string api_url = "https://steambrew.app/api/checkupdates";

    std::weak_ptr<millennium_backend> m_millennium_backend;
    std::shared_ptr<ipc_main> m_ipc_main;
    std::shared_ptr<theme_installer> theme_updater;
    std::shared_ptr<plugin_installer> plugin_updater;

    std::optional<json> cached_updates;
    bool has_checked_for_updates;
};
