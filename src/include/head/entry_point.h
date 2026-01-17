/*
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

#pragma once

#include "millennium/fwd_decl.h"
#include "millennium/http_hooks.h"
#include "millennium/millennium_updater.h"
#include "millennium/sysfs.h"

#include "head/library_updater.h"
#include "head/theme_cfg.h"

#include <functional>
#include <memory>
#include <map>
#include <string>

using builtin_payload = nlohmann::ordered_json;

class millennium_backend : public std::enable_shared_from_this<millennium_backend>
{
  public:
    void init();
    millennium_backend(std::shared_ptr<network_hook_ctl> network_hook_ctl, std::shared_ptr<SettingsStore> settings_store, std::shared_ptr<millennium_updater> millennium_updater);
    ~millennium_backend();

    std::weak_ptr<theme_webkit_mgr> get_theme_webkit_mgr();
    int get_operating_system();

    /** Enable/disable plugins */
    builtin_payload Core_ChangePluginStatus(const builtin_payload& args);

    /** Get the configuration needed to start the frontend */
    builtin_payload Core_GetStartConfig(const builtin_payload& args);

    /** Quick CSS utilities */
    builtin_payload Core_LoadQuickCss(const builtin_payload& args);
    builtin_payload Core_SaveQuickCss(const builtin_payload& args);

    /** General utilities */
    builtin_payload Core_GetSteamPath(const builtin_payload& args);
    builtin_payload Core_FindAllThemes(const builtin_payload& args);
    builtin_payload Core_FindAllPlugins(const builtin_payload& args);
    builtin_payload Core_GetEnvironmentVar(const builtin_payload& args);
    builtin_payload Core_GetBackendConfig(const builtin_payload& args);
    builtin_payload Core_SetBackendConfig(const builtin_payload& args);

    /** Theme and Plugin update API */
    builtin_payload Core_GetUpdates(const builtin_payload& args);

    /** Theme manager API */
    builtin_payload Core_GetActiveTheme(const builtin_payload& args);
    builtin_payload Core_ChangeActiveTheme(const builtin_payload& args);
    builtin_payload Core_GetSystemColors(const builtin_payload& args);
    builtin_payload Core_ChangeAccentColor(const builtin_payload& args);
    builtin_payload Core_ChangeColor(const builtin_payload& args);
    builtin_payload Core_ChangeCondition(const builtin_payload& args);
    builtin_payload Core_GetRootColors(const builtin_payload& args);
    builtin_payload Core_DoesThemeUseAccentColor(const builtin_payload& args);
    builtin_payload Core_GetThemeColorOptions(const builtin_payload& args);

    /** Theme installer related API's */
    builtin_payload Core_InstallTheme(const builtin_payload& args);
    builtin_payload Core_UninstallTheme(const builtin_payload& args);
    builtin_payload Core_DownloadThemeUpdate(const builtin_payload& args);
    builtin_payload Core_IsThemeInstalled(const builtin_payload& args);
    builtin_payload Core_GetThemeFromGitPair(const builtin_payload& args);

    /** Plugin related API's */
    builtin_payload Core_DownloadPluginUpdate(const builtin_payload& args);
    builtin_payload Core_InstallPlugin(const builtin_payload& args);
    builtin_payload Core_IsPluginInstalled(const builtin_payload& args);
    builtin_payload Core_UninstallPlugin(const builtin_payload& args);

    /** Get plugin backend logs */
    builtin_payload Core_GetPluginBackendLogs(const builtin_payload& args);

    /** Update Millennium API */
    builtin_payload Core_UpdateMillennium(const builtin_payload& args);
    builtin_payload Core_HasPendingMillenniumUpdateRestart(const builtin_payload& args);

    builtin_payload ipc_message_hdlr(const std::string& functionName, const builtin_payload& args);
    void set_ipc_main(std::shared_ptr<ipc_main> ipc_main);

  private:
    std::shared_ptr<ipc_main> m_ipc_main;
    std::shared_ptr<SettingsStore> m_settings_store;
    std::shared_ptr<millennium_updater> m_millennium_updater;
    std::shared_ptr<ThemeConfig> m_theme_config;
    std::shared_ptr<library_updater> m_updater;
    std::shared_ptr<theme_webkit_mgr> m_theme_webkit_mgr;
    std::shared_ptr<network_hook_ctl>& m_network_hook_ctl; /** hold ref to pass to webkit mgr without ref inc */

    std::map<std::string, std::function<builtin_payload(const builtin_payload&)>> function_map;
};
