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
#include "head/ipc_handler.h"
#include "head/library_updater.h"
#include "head/scan.h"
#include "head/theme_cfg.h"
#include "head/webkit.h"

#include "millennium/backend_mgr.h"
#include "millennium/init.h"
#include "millennium/millennium_updater.h"
#include "millennium/plugin_api_init.h"
#include "millennium/plugin_logger.h"
#include "millennium/sysfs.h"
#include "millennium/encode.h"

std::shared_ptr<ThemeConfig> g_theme_config;
std::shared_ptr<Updater> g_updater;
std::shared_ptr<SettingsStore> g_settings_store;
std::shared_ptr<theme_webkit_mgr> g_theme_webkit_mgr;
std::shared_ptr<network_hook_ctl> g_network_hook_ctl;

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

std::string Millennium_GetQuickCss()
{
    const std::string quickCssPath = fmt::format("{}/quickcss.css", GetEnv("MILLENNIUM__CONFIG_PATH"));

    if (!std::filesystem::exists(quickCssPath)) {
        SystemIO::WriteFileSync(quickCssPath, "/* Quick CSS file created by Millennium */\n");
    }

    return SystemIO::ReadFileSync(quickCssPath);
}

void Millennium_TogglePluginStatus(const std::vector<PluginStatus>& plugins)
{
    std::shared_ptr<backend_manager> backend_manager_ptr = g_plugin_loader->get_backend_manager();

    std::unordered_map<std::string, bool> pluginStatusMap;
    for (const auto& plugin : plugins) {
        pluginStatusMap[plugin.pluginName] = plugin.enabled;
    }

    bool hasEnableRequests = false;
    std::vector<std::string> pluginsToDisable;

    for (const auto& entry : pluginStatusMap) {
        const std::string& pluginName = entry.first;
        const bool newStatus = entry.second;

        g_settings_store->TogglePluginStatus(pluginName.c_str(), newStatus);

        if (newStatus) {
            hasEnableRequests = true;
            Logger.Log("requested to enable plugin [{}]", pluginName);
        } else {
            pluginsToDisable.push_back(pluginName);
            Logger.Log("requested to disable plugin [{}]", pluginName);
        }
    }

    if (hasEnableRequests) {
        g_plugin_loader->start_plugin_backends();
    }

    for (const auto& pluginName : pluginsToDisable) {
        const auto backendType = backend_manager_ptr->get_plugin_backend_type(pluginName);

        if (backendType == SettingsStore::PluginBackendType::Lua) {
            backend_manager_ptr->destroy_lua_vm(pluginName);
        } else if (backendType == SettingsStore::PluginBackendType::Python) {
            backend_manager_ptr->python_destroy_vm(pluginName);
        }
    }

    g_plugin_loader->inject_frontend_shims(true);
}

bool Millennium_RemoveBrowserModule(unsigned long long moduleId)
{
    return g_theme_webkit_mgr->remove_browser_hook(moduleId);
}

unsigned long long Millennium_AddBrowserModule(const char* moduleItem, const char* regexSelector, network_hook_ctl::TagTypes type)
{
    return g_theme_webkit_mgr->add_browser_hook(moduleItem, regexSelector, type);
}

nlohmann::json Millennium_GetPluginLogs()
{
    nlohmann::json logData = nlohmann::json::array();
    std::vector<SettingsStore::plugin_t> plugins = g_settings_store->ParseAllPlugins();

    for (auto& logger : get_plugin_logger_mgr()) {
        nlohmann::json logDataItem;

        for (auto [message, logLevel] : logger->CollectLogs()) {
            logDataItem.push_back({
                { "message", Base64Encode(message) },
                { "level",   logLevel              }
            });
        }

        std::string pluginName = logger->GetPluginName(false);

        for (auto& plugin : plugins) {
            if (plugin.pluginJson.contains("name") && plugin.pluginJson["name"] == logger->GetPluginName(false)) {
                pluginName = plugin.pluginJson.value("common_name", pluginName);
                break;
            }
        }

        // Handle package manager plugin
        if (pluginName == "pipx") pluginName = "Package Manager";

        logData.push_back({
            { "name", pluginName  },
            { "logs", logDataItem }
        });
    }

    return logData;
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
        { "accent_color", g_theme_config->GetAccentColor() },
        { "conditions", CONFIG.GetNested("themes.conditions", nlohmann::json::object()) },
        { "active_theme", g_theme_config->GetActiveTheme() },
        { "settings", CONFIG.GetAll() },
        { "steamPath", SystemIO::GetSteamPath() },
        { "installPath", SystemIO::GetInstallPath() },
        { "millenniumVersion", MILLENNIUM_VERSION },
        { "enabledPlugins", g_settings_store->GetEnabledPluginNames() },
        { "updates", g_updater->CheckForUpdates() },
        { "hasCheckedForUpdates", g_updater->HasCheckedForUpdates() },
        { "millenniumUpdates", MillenniumUpdater::HasAnyUpdates() },
        { "buildDate", GetBuildTimestamp() },
        { "platformType", GetOperatingSystemType() },
        { "millenniumLinuxUpdateScript", GetEnv("MILLENNIUM_UPDATE_SCRIPT_PROMPT") },
        { "quickCss", Millennium_GetQuickCss() }
    };
}

/** Quick CSS utilities */
IPC_RET(Core_LoadQuickCss, Millennium_GetQuickCss());
IPC_NIL(Core_SaveQuickCss, SystemIO::WriteFileSync(fmt::format("{}/quickcss.css", GetEnv("MILLENNIUM__CONFIG_PATH")), ARGS["css"].get<std::string>()));

/** General utilities */
IPC_RET(Core_GetSteamPath, SystemIO::GetSteamPath())
IPC_RET(Core_FindAllThemes, Millennium::Themes::FindAllThemes())
IPC_RET(Core_FindAllPlugins, Millennium::Plugins::FindAllPlugins(g_settings_store))
IPC_RET(Core_GetEnvironmentVar, GetEnv(ARGS["variable"]))
IPC_RET(Core_GetBackendConfig, CONFIG.GetAll())
IPC_RET(Core_SetBackendConfig, CONFIG.SetAll(nlohmann::json::parse(ARGS["config"].get<std::string>()), ARGS.value("skipPropagation", false)))

/** Theme and Plugin update API */
IPC_RET(Core_GetUpdates, g_updater->CheckForUpdates(ARGS.value("force", false)).value_or(nullptr))

/** Theme manager API */
IPC_RET(Core_GetActiveTheme, g_theme_config->GetActiveTheme())
IPC_NIL(Core_ChangeActiveTheme, g_theme_config->ChangeTheme(ARGS["theme_name"]))
IPC_RET(Core_GetSystemColors, g_theme_config->GetAccentColor())
IPC_NIL(Core_ChangeAccentColor, g_theme_config->ChangeAccentColor(ARGS["new_color"]))
IPC_RET(Core_ChangeColor, g_theme_config->ChangeColor(ARGS["theme"], ARGS["color_name"], ARGS["new_color"], ARGS["color_type"]))
IPC_RET(Core_ChangeCondition, g_theme_config->ChangeCondition(ARGS["theme"], ARGS["newData"], ARGS["condition"]))
IPC_RET(Core_GetRootColors, g_theme_config->GetColors())
IPC_RET(Core_DoesThemeUseAccentColor, true) /** placeholder, too lazy to implement */
IPC_RET(Core_GetThemeColorOptions, g_theme_config->GetColorOpts(ARGS["theme_name"]))

/** Theme installer related API's */
IPC_RET(Core_InstallTheme, g_updater->GetThemeUpdater().InstallTheme(g_theme_config, ARGS["repo"], ARGS["owner"]))
IPC_RET(Core_UninstallTheme, g_updater->GetThemeUpdater().UninstallTheme(g_theme_config, ARGS["repo"], ARGS["owner"]))
IPC_RET(Core_DownloadThemeUpdate, g_updater->DownloadThemeUpdate(g_theme_config, ARGS["native"]))
IPC_RET(Core_IsThemeInstalled, g_updater->GetThemeUpdater().CheckInstall(ARGS["repo"], ARGS["owner"]))
IPC_RET(Core_GetThemeFromGitPair, g_updater->GetThemeUpdater().GetThemeFromGitPair(ARGS["repo"], ARGS["owner"], ARGS.value("asString", false)).value_or(nullptr))

/** Plugin related API's */
IPC_RET(Core_DownloadPluginUpdate, g_updater->DownloadPluginUpdate(ARGS["id"], ARGS["name"]))
IPC_RET(Core_InstallPlugin, g_updater->GetPluginUpdater().InstallPlugin(ARGS["download_url"], ARGS["total_size"]))
IPC_RET(Core_IsPluginInstalled, g_updater->GetPluginUpdater().CheckInstall(ARGS["plugin_name"]))
IPC_RET(Core_UninstallPlugin, g_updater->GetPluginUpdater().UninstallPlugin(ARGS["pluginName"]))

/** Get plugin backend logs */
IPC_RET(Core_GetPluginBackendLogs, Millennium_GetPluginLogs())

/** Update Millennium API */
IPC_NIL(Core_UpdateMillennium, MillenniumUpdater::StartUpdate(ARGS["downloadUrl"], ARGS["downloadSize"], ARGS["background"]))
IPC_RET(Core_HasPendingMillenniumUpdateRestart, MillenniumUpdater::HasPendingRestart())

/**
 * Initialize core components
 * Called before any other plugin is loaded.
 */
void Core_Load(std::shared_ptr<SettingsStore> settings_store_ptr, std::shared_ptr<network_hook_ctl> network_hook_ctl_ptr)
{
    g_settings_store = std::move(settings_store_ptr);
    g_network_hook_ctl = std::move(network_hook_ctl_ptr);
    g_updater = std::make_shared<Updater>(settings_store_ptr);
    g_theme_webkit_mgr = std::make_shared<theme_webkit_mgr>(settings_store_ptr, g_network_hook_ctl);
    g_theme_config = std::make_shared<ThemeConfig>(settings_store_ptr, g_theme_webkit_mgr);
}

/** TODO: unused, impl later. shouldn't cause any issues on shutdown though. */
void Core_Unload()
{
    g_settings_store.reset();
    g_updater.reset();
    g_theme_config.reset();
}
