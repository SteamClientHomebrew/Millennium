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
#include "head/library_updater.h"
#include "head/scan.h"
#include "head/theme_cfg.h"
#include "head/webkit.h"

#include "millennium/backend_mgr.h"
#include "millennium/encode.h"
#include "millennium/http_hooks.h"
#include "millennium/millennium.h"
#include "millennium/millennium_updater.h"
#include "millennium/plugin_api_init.h"
#include "millennium/sysfs.h"

int millennium_backend::GetOperatingSystemType()
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

void millennium_backend::Millennium_TogglePluginStatus(const std::vector<PluginStatus>& plugins)
{
    std::shared_ptr<backend_manager> backend_manager_ptr = g_millennium->get_plugin_loader()->get_backend_manager();

    std::unordered_map<std::string, bool> pluginStatusMap;
    for (const auto& plugin : plugins) {
        pluginStatusMap[plugin.pluginName] = plugin.enabled;
    }

    bool hasEnableRequests = false;
    std::vector<std::string> pluginsToDisable;

    for (const auto& entry : pluginStatusMap) {
        const std::string& pluginName = entry.first;
        const bool newStatus = entry.second;

        m_settings_store->TogglePluginStatus(pluginName.c_str(), newStatus);

        if (newStatus) {
            hasEnableRequests = true;
            Logger.Log("requested to enable plugin [{}]", pluginName);
        } else {
            pluginsToDisable.push_back(pluginName);
            Logger.Log("requested to disable plugin [{}]", pluginName);
        }
    }

    if (hasEnableRequests) {
        g_millennium->get_plugin_loader()->start_plugin_backends();
    }

    for (const auto& pluginName : pluginsToDisable) {
        const auto backendType = backend_manager_ptr->get_plugin_backend_type(pluginName);

        if (backendType == SettingsStore::PluginBackendType::Lua) {
            backend_manager_ptr->destroy_lua_vm(pluginName);
        } else if (backendType == SettingsStore::PluginBackendType::Python) {
            backend_manager_ptr->python_destroy_vm(pluginName);
        }
    }

    g_millennium->get_plugin_loader()->inject_frontend_shims(true);
}

bool millennium_backend::Millennium_RemoveBrowserModule(unsigned long long moduleId)
{
    return m_theme_webkit_mgr->remove_browser_hook(moduleId);
}

unsigned long long millennium_backend::Millennium_AddBrowserModule(const char* moduleItem, const char* regexSelector, network_hook_ctl::TagTypes type)
{
    return m_theme_webkit_mgr->add_browser_hook(moduleItem, regexSelector, type);
}

builtin_payload millennium_backend::Millennium_GetPluginLogs()
{
    nlohmann::json logData = nlohmann::json::array();
    std::vector<SettingsStore::plugin_t> plugins = m_settings_store->ParseAllPlugins();

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

millennium_backend::~millennium_backend()
{
    Logger.Log("Successfully shut down millennium_backend...");
}

void millennium_backend::init()
{
    m_updater = std::make_shared<Updater>(shared_from_this(), m_ipc_main);
    m_updater->init(m_settings_store);
    m_theme_webkit_mgr = std::make_shared<theme_webkit_mgr>(m_settings_store, m_network_hook_ctl);
    m_theme_config = std::make_shared<ThemeConfig>(m_settings_store, m_theme_webkit_mgr);
}

millennium_backend::millennium_backend(std::shared_ptr<ipc_main> ipc_main, std::shared_ptr<SettingsStore> settings_store, std::shared_ptr<network_hook_ctl> hook_ctl,
                                       std::shared_ptr<millennium_updater> millennium_updater)
    : m_ipc_main(std::move(ipc_main)), m_settings_store(std::move(settings_store)), m_millennium_updater(std::move(millennium_updater)), m_network_hook_ctl(std::move(hook_ctl))
{
#define register_function(name) { #name, std::bind(&millennium_backend::name, this, std::placeholders::_1) }
    function_map = {
        register_function(Core_ChangePluginStatus),
        register_function(Core_GetStartConfig),
        register_function(Core_LoadQuickCss),
        register_function(Core_SaveQuickCss),
        register_function(Core_GetSteamPath),
        register_function(Core_FindAllThemes),
        register_function(Core_FindAllPlugins),
        register_function(Core_GetEnvironmentVar),
        register_function(Core_GetBackendConfig),
        register_function(Core_SetBackendConfig),
        register_function(Core_GetUpdates),
        register_function(Core_GetActiveTheme),
        register_function(Core_ChangeActiveTheme),
        register_function(Core_GetSystemColors),
        register_function(Core_ChangeAccentColor),
        register_function(Core_ChangeColor),
        register_function(Core_ChangeCondition),
        register_function(Core_GetRootColors),
        register_function(Core_DoesThemeUseAccentColor),
        register_function(Core_GetThemeColorOptions),
        register_function(Core_InstallTheme),
        register_function(Core_UninstallTheme),
        register_function(Core_DownloadThemeUpdate),
        register_function(Core_IsThemeInstalled),
        register_function(Core_GetThemeFromGitPair),
        register_function(Core_DownloadPluginUpdate),
        register_function(Core_InstallPlugin),
        register_function(Core_IsPluginInstalled),
        register_function(Core_UninstallPlugin),
        register_function(Core_GetPluginBackendLogs),
        register_function(Core_UpdateMillennium),
        register_function(Core_HasPendingMillenniumUpdateRestart),
    };
}

const char* Millennium_GetUpdateScript()
{
#ifdef MILLENNIUM__UPDATE_SCRIPT_PROMPT
    return MILLENNIUM__UPDATE_SCRIPT_PROMPT;
#else
    #error "missing MILLENNIUM__UPDATE_SCRIPT_PROMPT";
    return nullptr;
#endif
}

/**
 * Enable/disable plugins
 */
builtin_payload millennium_backend::Core_ChangePluginStatus(const builtin_payload& args)
{
    std::vector<PluginStatus> plugins;
    auto pluginJson = nlohmann::json::parse(args["pluginJson"].get<std::string>());

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
builtin_payload millennium_backend::Core_GetStartConfig(const builtin_payload&)
{
    return {
        { "accent_color", m_theme_config->GetAccentColor() },
        { "conditions", CONFIG.GetNested("themes.conditions", nlohmann::json::object()) },
        { "active_theme", m_theme_config->GetActiveTheme() },
        { "settings", CONFIG.GetAll() },
        { "steamPath", SystemIO::GetSteamPath() },
        { "installPath", SystemIO::GetInstallPath() },
        { "millenniumVersion", MILLENNIUM_VERSION },
        { "enabledPlugins", m_settings_store->GetEnabledPluginNames() },
        { "updates", m_updater->CheckForUpdates() },
        { "hasCheckedForUpdates", m_updater->HasCheckedForUpdates() },
        { "millenniumUpdates", m_millennium_updater->has_any_updates() },
        { "buildDate", GetBuildTimestamp() },
        { "platformType", GetOperatingSystemType() },
        { "millenniumLinuxUpdateScript", Millennium_GetUpdateScript() },
        { "quickCss", Core_LoadQuickCss(nullptr) }  // TODO check this works
    };
}

/** Quick CSS utilities */
builtin_payload millennium_backend::Core_LoadQuickCss(const builtin_payload&)
{
    const std::string quickCssPath = fmt::format("{}/quickcss.css", GetEnv("MILLENNIUM__CONFIG_PATH"));

    if (!std::filesystem::exists(quickCssPath)) {
        SystemIO::WriteFileSync(quickCssPath, "/* Quick CSS file created by Millennium */\n");
    }

    return SystemIO::ReadFileSync(quickCssPath);
}
builtin_payload millennium_backend::Core_SaveQuickCss(const builtin_payload& args)
{
    const std::string quickCssPath = fmt::format("{}/quickcss.css", GetEnv("MILLENNIUM__CONFIG_PATH"));
    SystemIO::WriteFileSync(quickCssPath, args["css"].get<std::string>());
    return {};
}

/** General utilities */
builtin_payload millennium_backend::Core_GetSteamPath(const builtin_payload&)
{
    return SystemIO::GetSteamPath();
}
builtin_payload millennium_backend::Core_FindAllThemes(const builtin_payload&)
{
    return Millennium::Themes::FindAllThemes();
}
builtin_payload millennium_backend::Core_FindAllPlugins(const builtin_payload&)
{
    return Millennium::Plugins::FindAllPlugins(m_settings_store);
}
builtin_payload millennium_backend::Core_GetEnvironmentVar(const builtin_payload& args)
{
    return GetEnv(args["variable"]);
}
builtin_payload millennium_backend::Core_GetBackendConfig(const builtin_payload&)
{
    return CONFIG.GetAll();
}
builtin_payload millennium_backend::Core_SetBackendConfig(const builtin_payload& args)
{
    return CONFIG.SetAll(nlohmann::json::parse(args["config"].get<std::string>()), args.value("skipPropagation", false));
}

/** Theme and Plugin update API */
builtin_payload millennium_backend::Core_GetUpdates(const builtin_payload& args)
{
    return m_updater->CheckForUpdates(args.value("force", false)).value_or(nullptr);
}

/** Theme manager API */
builtin_payload millennium_backend::Core_GetActiveTheme(const builtin_payload&)
{
    return m_theme_config->GetActiveTheme();
}
builtin_payload millennium_backend::Core_ChangeActiveTheme(const builtin_payload& args)
{
    m_theme_config->ChangeTheme(args["theme_name"]);
    return {};
}
builtin_payload millennium_backend::Core_GetSystemColors(const builtin_payload&)
{
    return m_theme_config->GetAccentColor();
}
builtin_payload millennium_backend::Core_ChangeAccentColor(const builtin_payload& args)
{
    m_theme_config->ChangeAccentColor(args["new_color"]);
    return {};
}
builtin_payload millennium_backend::Core_ChangeColor(const builtin_payload& args)
{
    return m_theme_config->ChangeColor(args["theme"], args["color_name"], args["new_color"], args["color_type"]);
}
builtin_payload millennium_backend::Core_ChangeCondition(const builtin_payload& args)
{
    return m_theme_config->ChangeCondition(args["theme"], args["newData"], args["condition"]);
}
builtin_payload millennium_backend::Core_GetRootColors(const builtin_payload&)
{
    return m_theme_config->GetColors();
}
builtin_payload millennium_backend::Core_DoesThemeUseAccentColor(const builtin_payload&)
{
    return true; /** placeholder, too lazy to implement */
}
builtin_payload millennium_backend::Core_GetThemeColorOptions(const builtin_payload& args)
{
    return m_theme_config->GetColorOpts(args["theme_name"]);
}

/** Theme installer related API's */
builtin_payload millennium_backend::Core_InstallTheme(const builtin_payload& args)
{
    return m_updater->GetThemeUpdater()->install_theme(m_theme_config, args["repo"], args["owner"]);
}
builtin_payload millennium_backend::Core_UninstallTheme(const builtin_payload& args)
{
    return m_updater->GetThemeUpdater()->uninstall_theme(m_theme_config, args["repo"], args["owner"]);
}
builtin_payload millennium_backend::Core_DownloadThemeUpdate(const builtin_payload& args)
{
    return m_updater->DownloadThemeUpdate(m_theme_config, args["native"]);
}
builtin_payload millennium_backend::Core_IsThemeInstalled(const builtin_payload& args)
{
    return m_updater->GetThemeUpdater()->is_theme_installed(args["repo"], args["owner"]);
}
builtin_payload millennium_backend::Core_GetThemeFromGitPair(const builtin_payload& args)
{
    return m_updater->GetThemeUpdater()->get_theme_from_github(args["repo"], args["owner"], args.value("asString", false)).value_or(nullptr);
}

/** Plugin related API's */
builtin_payload millennium_backend::Core_DownloadPluginUpdate(const builtin_payload& args)
{
    return m_updater->DownloadPluginUpdate(args["id"], args["name"]);
}
builtin_payload millennium_backend::Core_InstallPlugin(const builtin_payload& args)
{
    return m_updater->GetPluginUpdater()->install_plugin(args["download_url"], args["total_size"]);
}
builtin_payload millennium_backend::Core_IsPluginInstalled(const builtin_payload& args)
{
    return m_updater->GetPluginUpdater()->is_plugin_installed(args["plugin_name"]);
}
builtin_payload millennium_backend::Core_UninstallPlugin(const builtin_payload& args)
{
    return m_updater->GetPluginUpdater()->uninstall_plugin(args["pluginName"]);
}

/** Get plugin backend logs */
builtin_payload millennium_backend::Core_GetPluginBackendLogs(const builtin_payload&)
{
    return Millennium_GetPluginLogs();
}

/** Update Millennium API */
builtin_payload millennium_backend::Core_UpdateMillennium(const builtin_payload& args)
{
    m_millennium_updater->update(args["downloadUrl"], args["downloadSize"], args["background"]);
    return {};
}

builtin_payload millennium_backend::Core_HasPendingMillenniumUpdateRestart(const builtin_payload&)
{
    return m_millennium_updater->is_pending_restart();
}

builtin_payload millennium_backend::HandleIpcMessage(const std::string& functionName, const builtin_payload& args)
{
    // Skip plugin settings parser as it's not applicable
    if (functionName == "__builtins__.__millennium_plugin_settings_parser__") {
        throw std::runtime_error("Not applicable to this plugin");
    }

    const auto it = function_map.find(functionName);

    if (it == function_map.end()) {
        const std::string errorMsg = "Function not found: " + functionName;
        LOG_ERROR("{}", errorMsg);
        throw std::runtime_error(errorMsg);
    }

    /** call the functions with provided args */
    return (std::any_cast<std::function<builtin_payload(const builtin_payload&)>>(it->second))(args);
}
