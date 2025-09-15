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

#include "head/ipc_handler.h"
#include "head/library_updater.h"
#include "head/scan.h"
#include "head/theme_cfg.h"
#include "head/theme_mgr.h"

#include "millennium/encode.h"
#include "millennium/millennium_api.h"
#include "millennium/millennium_updater.h"
#include "millennium/plugin_api_init.h"
#include "millennium/plugin_logger.h"
#include "millennium/sysfs.h"

std::shared_ptr<ThemeConfig> themeConfig;
std::shared_ptr<Updater> updater;
std::unique_ptr<SettingsStore> settingsStore;

int GetOperatingSystemType()
{
#if defined(_WIN32)
    return 0; // Windows
#elif defined(__APPLE__) || defined(__linux__)
    return 1; // macOS or Linux
#else
#error "Unsupported platform"
    return -1; // Unknown
#endif
}

/**
 * Enable/disable plugins
 */
MILLENNIUM_IPC_DECL(Core_ChangePluginStatus)
{
    std::vector<PluginStatus> plugins;
    auto pluginJson = nlohmann::json::parse(ARGS["pluginJson"].get<std::string>());

    for (const auto& item : pluginJson) {
        if (item.contains("plugin_name") && item.contains("enabled")) {
            plugins.push_back({ item["plugin_name"], item["enabled"] });
        }
    }

    Millennium_TogglePluginStatus(plugins);
    return {};
}

/**
 * Get the configuration needed to start the frontend
 * @note exceptions are caught by the IPC handler
 */
MILLENNIUM_IPC_DECL(Core_GetStartConfig)
{
    return {
        { "accent_color", themeConfig->GetAccentColor() },
        { "conditions", CONFIG.GetNested("themes.conditions", nlohmann::json::object()) },
        { "active_theme", themeConfig->GetActiveTheme() },
        { "settings", CONFIG.GetAll() },
        { "steamPath", SystemIO::GetSteamPath() },
        { "installPath", SystemIO::GetInstallPath() },
        { "millenniumVersion", MILLENNIUM_VERSION },
        { "enabledPlugins", settingsStore->GetEnabledPluginNames() },
        { "updates", nlohmann::json::object() },
        { "hasCheckedForUpdates", false },
        { "millenniumUpdates", MillenniumUpdater::HasAnyUpdates() },
        { "buildDate", GetBuildTimestamp() },
        { "millenniumUpdates", nlohmann::json::object() },
        { "platformType", GetOperatingSystemType() },
        { "millenniumLinuxUpdateScript", GetEnv("MILLENNIUM_UPDATE_SCRIPT_PROMPT") }
    };
}

/** General utilities */
IPC_RET(Core_GetSteamPath, SystemIO::GetSteamPath())
IPC_RET(Core_FindAllThemes, Millennium::Themes::FindAllThemes())
IPC_RET(Core_FindAllPlugins, Millennium::Plugins::FindAllPlugins())
IPC_RET(Core_GetEnvironmentVar, GetEnv(ARGS["variable"]))
IPC_RET(Core_GetBackendConfig, CONFIG.GetAll())
IPC_RET(Core_SetBackendConfig, CONFIG.SetAll(nlohmann::json::parse(ARGS["config"].get<std::string>()), ARGS.value("skipPropagation", false)))

/** Theme and Plugin update API */
IPC_RET(Core_GetUpdates, updater->CheckForUpdates(ARGS.value("force", false)).value_or(nullptr))

/** Theme manager API */
IPC_RET(Core_GetActiveTheme, themeConfig->GetActiveTheme())
IPC_NIL(Core_ChangeActiveTheme, themeConfig->ChangeTheme(ARGS["theme_name"]))
IPC_RET(Core_GetSystemColors, themeConfig->GetAccentColor())
IPC_NIL(Core_ChangeAccentColor, themeConfig->ChangeAccentColor(ARGS["new_color"]))
IPC_RET(Core_ChangeColor, themeConfig->ChangeColor(ARGS["theme"], ARGS["color_name"], ARGS["new_color"], ARGS["color_type"]))
IPC_RET(Core_ChangeCondition, themeConfig->ChangeCondition(ARGS["theme"], ARGS["newData"], ARGS["condition"]))
IPC_RET(Core_GetRootColors, themeConfig->GetColors())
IPC_RET(Core_DoesThemeUseAccentColor, true) /** placeholder, too lazy to implement */
IPC_RET(Core_GetThemeColorOptions, themeConfig->GetColorOpts(ARGS["theme_name"]))

/** Theme installer related API's */
IPC_RET(Core_InstallTheme, updater->GetThemeUpdater().InstallTheme(ARGS["repo"], ARGS["owner"]))
IPC_RET(Core_UninstallTheme, updater->GetThemeUpdater().UninstallTheme(ARGS["repo"], ARGS["owner"]))
IPC_RET(Core_IsThemeInstalled, updater->GetThemeUpdater().CheckInstall(ARGS["repo"], ARGS["owner"]))
IPC_RET(Core_GetThemeFromGitPair, updater->GetThemeUpdater().GetThemeFromGitPair(ARGS["repo"], ARGS["owner"], ARGS.value("asString", false)).value_or(nullptr))
IPC_RET(Core_DownloadThemeUpdate, updater->DownloadThemeUpdate(ARGS["native"]))

/** Plugin related API's */
IPC_RET(Core_DownloadPluginUpdate, updater->DownloadPluginUpdate(ARGS["id"], ARGS["name"]))
IPC_RET(Core_InstallPlugin, updater->GetPluginUpdater().InstallPlugin(ARGS["download_url"], ARGS["total_size"]))
IPC_RET(Core_IsPluginInstalled, updater->GetPluginUpdater().CheckInstall(ARGS["plugin_name"]))
IPC_RET(Core_UninstallPlugin, updater->GetPluginUpdater().UninstallPlugin(ARGS["pluginName"]))

/** Get plugin backend logs */
IPC_RET(Core_GetPluginBackendLogs, Millennium_GetPluginLogs())

/** Update Millennium API */
IPC_NIL(Core_UpdateMillennium, MillenniumUpdater::StartUpdate(ARGS["downloadUrl"], ARGS["downloadSize"], ARGS["background"]))

/**
 * Initialize core components
 * Called before any other plugin is loaded.
 */
void Core_Load()
{
    settingsStore = std::make_unique<SettingsStore>();
    updater = std::make_shared<Updater>();
    themeConfig = std::make_shared<ThemeConfig>();
}

/** TODO: unused, impl later. shouldn't cause any issues on shutdown though. */
void Core_Unload()
{
    settingsStore.reset();
    updater.reset();
    themeConfig.reset();
}