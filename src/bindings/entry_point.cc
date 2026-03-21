/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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
#include "millennium/linux_distro_helpers.h"
#include "head/library_updater.h"
#include "mep/crash_event_bus.h"
#include "head/scan.h"
#include "head/theme_cfg.h"
#include "head/webkit.h"

#include "millennium/environment.h"
#include "millennium/encoding.h"
#include "millennium/http_hooks.h"
#include "millennium/logger.h"
#include "millennium/millennium.h"
#include "millennium/millennium_updater.h"
#include "millennium/plugin_api_init.h"
#include "millennium/plugin_loader.h"
#include "millennium/logger.h"
#include "millennium/config.h"
#include "millennium/filesystem.h"
#include <exception>
#include <fstream>
#include <thread>

std::weak_ptr<head::theme_webkit_mgr> head::millennium_backend::get_theme_webkit_mgr()
{
    return m_theme_webkit_mgr;
}

int head::millennium_backend::get_operating_system()
{
#if defined(_WIN32)
    return 0; // Windows
#elif defined(__linux__)
    return 1; // Linux
#elif defined(__APPLE__)
    return 2; // macOS
#else
#error "Unsupported platform"
    return -1; // Unknown
#endif
}

head::millennium_backend::~millennium_backend()
{
    logger.log("Successfully shut down millennium_backend...");
}

void head::millennium_backend::init()
{
    m_updater = std::make_shared<library_updater>(shared_from_this(), m_ipc_main);
    m_updater->init(m_plugin_manager);

    m_theme_webkit_mgr = std::make_shared<theme_webkit_mgr>(m_plugin_manager, m_network_hook_ctl);
    m_theme_config = std::make_shared<theme_config_store>(m_plugin_manager, m_theme_webkit_mgr);
}

head::millennium_backend::millennium_backend(std::shared_ptr<network_hook_ctl> network_hook_ctl, std::shared_ptr<plugin_manager> plugin_manager,
                                             std::shared_ptr<millennium_updater> millennium_updater)
    : m_plugin_manager(std::move(plugin_manager)), m_millennium_updater(std::move(millennium_updater)), m_network_hook_ctl(network_hook_ctl)
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
        register_function(Core_KillPluginBackend),
        register_function(Core_InstallPlugin),
        register_function(Core_IsPluginInstalled),
        register_function(Core_UninstallPlugin),
        register_function(Core_GetPluginBackendLogs),
        register_function(Core_UpdateMillennium),
        register_function(Core_HasPendingMillenniumUpdateRestart),
        register_function(Core_GetPendingCrashes),
        register_function(Core_AcknowledgeCrash),
    };
}

std::string head::millennium_backend::get_millennium_updater_script()
{
#ifdef __linux__
    constexpr const char* default_script = "curl -fsSL 'https://steambrew.app/install.sh' | bash";

    /**
     * check if we are installed from nix flake
     * TODO: if we get merged into nixpkgs, does this work?
     */
    if (access("/etc/NIXOS", F_OK) == 0) {
        return "nix flake update millennium";
    }

    /* check if we're on arch being managed by an AUR manager */
    if (program_in_path("pacman") && millennium_is_pacman_managed()) {
        return get_aur_update_command();
    }

    return default_script;
#else
    return {};
#endif
}

/**
 * Enable/disable plugins
 */
builtin_payload head::millennium_backend::Core_ChangePluginStatus(const builtin_payload& args)
{
    ::plugin_loader::plugin_state plugins;
    auto pluginJson = nlohmann::json::parse(args["pluginJson"].get<std::string>());

    for (const auto& item : pluginJson) {
        try {
            if (item.contains("plugin_name") && item.contains("enabled")) {
                plugins.push_back({ item["plugin_name"], item["enabled"] });
            }
        } catch (const std::exception& ex) {
            LOG_ERROR("Failed to add a plugin to list: {}", ex.what());
        }
    }

    g_millennium->get_plugin_loader()->set_plugins_enabled(plugins);
    return {};
}

/**
 * Get the configuration needed to start the frontend
 * @note exceptions are caught by the IPC handler
 */
builtin_payload head::millennium_backend::Core_GetStartConfig(const builtin_payload&)
{
    /* Build pending-crash array inline so the frontend receives it
       synchronously on init — no separate poll/timing needed. */
    nlohmann::json pending_crashes_json = nlohmann::json::array();
    {
        const auto crashes = mep::crash_event_bus::instance().get_pending_crashes();
        const auto all_plugins = m_plugin_manager->get_all_plugins();

        for (const auto& ev : crashes) {
            std::string display_name = ev.display_name;
            if (display_name.empty()) {
                for (const auto& p : all_plugins) {
                    if (p.plugin_name == ev.plugin_name) {
                        display_name = p.plugin_json.value("common_name", ev.plugin_name);
                        break;
                    }
                }
            }
            if (display_name.empty()) display_name = ev.plugin_name;

            pending_crashes_json.push_back({
                { "plugin",       ev.plugin_name         },
                { "displayName",  display_name           },
                { "exitCode",     (uint32_t)ev.exit_code },
                { "crashDumpDir", ev.crash_dump_dir      }
            });
        }
    }

    return {
        { "accent_color", m_theme_config->get_accent_color() },
        { "conditions", CONFIG.get({ "themes", "conditions" }, nlohmann::json::object()) },
        { "active_theme", m_theme_config->get_active_theme() },
        { "settings", CONFIG.get_all() },
        { "steamPath", platform::get_steam_path() },
        { "installPath", platform::get_install_path() },
        { "themesPath", (platform::get_millennium_path() / "themes").string() },
        { "millenniumVersion", MILLENNIUM_VERSION },
        { "enabledPlugins", m_plugin_manager->get_enabled_plugin_names() },
        { "updates", m_updater->check_for_updates() },
        { "hasCheckedForUpdates", m_updater->has_checked_for_updates() },
        { "millenniumUpdates", m_millennium_updater->has_any_updates() },
        { "buildDate", GetBuildTimestamp() },
        { "platformType", get_operating_system() },
        { "millenniumLinuxUpdateScript", this->get_millennium_updater_script() },
        { "quickCss", Core_LoadQuickCss(nullptr) },
        { "pendingCrashes", pending_crashes_json }
    };
}

/** Quick CSS utilities */
builtin_payload head::millennium_backend::Core_LoadQuickCss(const builtin_payload&)
{
    const std::string quickCssPath = fmt::format("{}/quick.css", platform::environment::get("MILLENNIUM__CONFIG_PATH"));

    if (!std::filesystem::exists(quickCssPath)) {
        platform::write_file(quickCssPath, "/* Quick CSS file created by Millennium */\n");
    }

    return platform::read_file(quickCssPath);
}
builtin_payload head::millennium_backend::Core_SaveQuickCss(const builtin_payload& args)
{
    const std::string quickCssPath = fmt::format("{}/quick.css", platform::environment::get("MILLENNIUM__CONFIG_PATH"));
    platform::write_file(quickCssPath, args["css"].get<std::string>());
    return {};
}

/** General utilities */
builtin_payload head::millennium_backend::Core_GetSteamPath(const builtin_payload&)
{
    return platform::get_steam_path();
}
builtin_payload head::millennium_backend::Core_FindAllThemes(const builtin_payload&)
{
    return head::Themes::FindAllThemes();
}
builtin_payload head::millennium_backend::Core_FindAllPlugins(const builtin_payload&)
{
    return head::Plugins::FindAllPlugins(m_plugin_manager);
}
builtin_payload head::millennium_backend::Core_GetEnvironmentVar(const builtin_payload& args)
{
    return platform::environment::get(args["variable"]);
}
builtin_payload head::millennium_backend::Core_GetBackendConfig(const builtin_payload&)
{
    return CONFIG.get_all();
}
builtin_payload head::millennium_backend::Core_SetBackendConfig(const builtin_payload& args)
{
    return CONFIG.set_all(nlohmann::json::parse(args["config"].get<std::string>()), args.value("skipPropagation", false));
}

/** Theme and Plugin update API */
builtin_payload head::millennium_backend::Core_GetUpdates(const builtin_payload& args)
{
    return m_updater->check_for_updates(args.value("force", false)).value_or(nullptr);
}

/** Theme manager API */
builtin_payload head::millennium_backend::Core_GetActiveTheme(const builtin_payload&)
{
    return m_theme_config->get_active_theme();
}
builtin_payload head::millennium_backend::Core_ChangeActiveTheme(const builtin_payload& args)
{
    m_theme_config->change_theme(args["theme_name"]);
    return {};
}
builtin_payload head::millennium_backend::Core_GetSystemColors(const builtin_payload&)
{
    return m_theme_config->get_accent_color();
}
builtin_payload head::millennium_backend::Core_ChangeAccentColor(const builtin_payload& args)
{
    m_theme_config->set_accent_color(args["new_color"]);
    return {};
}
builtin_payload head::millennium_backend::Core_ChangeColor(const builtin_payload& args)
{
    return m_theme_config->set_theme_color(args["theme"], args["color_name"], args["new_color"], args["color_type"]);
}
builtin_payload head::millennium_backend::Core_ChangeCondition(const builtin_payload& args)
{
    return m_theme_config->set_condition(args["theme"], args["newData"], args["condition"]);
}
builtin_payload head::millennium_backend::Core_GetRootColors(const builtin_payload&)
{
    return m_theme_config->get_colors();
}
builtin_payload head::millennium_backend::Core_DoesThemeUseAccentColor(const builtin_payload&)
{
    return true; /** placeholder, too lazy to implement */
}
builtin_payload head::millennium_backend::Core_GetThemeColorOptions(const builtin_payload& args)
{
    return m_theme_config->get_color_options(args["theme_name"]);
}

/** Theme installer related API's */
builtin_payload head::millennium_backend::Core_InstallTheme(const builtin_payload& args)
{
    auto theme_updater_weak = m_updater->get_theme_updater();
    auto updater = m_updater;
    auto themeConfig = m_theme_config;
    auto repo = args["repo"].get<std::string>();
    auto owner = args["owner"].get<std::string>();
    int op_id = updater->start_operation();

    std::thread([theme_updater_weak, updater, themeConfig, repo, owner, op_id]()
    {
        updater->set_thread_op_id(op_id);
        try {
            if (auto theme_updater = theme_updater_weak.lock()) {
                auto result = theme_updater->install_theme(themeConfig, repo, owner);
                bool success = result.value("success", false);
                if (!success) {
                    updater->dispatch_progress(result.value("message", "Install failed"), 0, true, false);
                }
            } else {
                LOG_ERROR("Failed to lock theme_updater, it likely shutdown.");
                updater->dispatch_progress("Install failed", 0, true, false);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Background theme install failed: {}", e.what());
            updater->dispatch_progress("Install failed", 0, true, false);
        }
    }).detach();

    return nlohmann::json{
        { "success", true  },
        { "started", true  },
        { "opId",    op_id }
    };
}
builtin_payload head::millennium_backend::Core_UninstallTheme(const builtin_payload& args)
{
    if (auto theme_updater = m_updater->get_theme_updater().lock()) {
        return theme_updater->uninstall_theme(m_theme_config, args["repo"], args["owner"]);
    }

    LOG_ERROR("Failed to lock theme_updater, it likely shutdown.");
    return false;
}
builtin_payload head::millennium_backend::Core_DownloadThemeUpdate(const builtin_payload& args)
{
    auto updater = m_updater;
    auto themeConfig = m_theme_config;
    auto native = args["native"].get<std::string>();
    int op_id = updater->start_operation();

    std::thread([updater, themeConfig, native, op_id]()
    {
        updater->set_thread_op_id(op_id);
        try {
            bool success = updater->download_theme_update(themeConfig, native);
            if (!success) {
                updater->dispatch_progress("Update failed", 0, true, false);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Background theme update failed: {}", e.what());
            updater->dispatch_progress("Update failed", 0, true, false);
        }
    }).detach();

    return nlohmann::json{
        { "opId", op_id }
    };
}
builtin_payload head::millennium_backend::Core_IsThemeInstalled(const builtin_payload& args)
{
    if (auto theme_updater = m_updater->get_theme_updater().lock()) {
        return theme_updater->is_theme_installed(args["repo"], args["owner"]);
    }

    LOG_ERROR("Failed to lock theme_updater, it likely shutdown.");
    return false;
}
builtin_payload head::millennium_backend::Core_GetThemeFromGitPair(const builtin_payload& args)
{
    if (auto theme_updater = m_updater->get_theme_updater().lock()) {
        return theme_updater->get_theme_from_github(args["repo"], args["owner"], args.value("asString", false)).value_or(nullptr);
    }

    LOG_ERROR("Failed to lock theme_updater, it likely shutdown.");
    return false;
}

/** Plugin related API's */
builtin_payload head::millennium_backend::Core_DownloadPluginUpdate(const builtin_payload& args)
{
    auto updater = m_updater;
    auto id = args["id"].get<std::string>();
    auto name = args["name"].get<std::string>();
    auto commit = args.value("commit", std::string{});
    int op_id = updater->start_operation();

    std::thread([updater, id, name, commit, op_id]()
    {
        updater->set_thread_op_id(op_id);
        try {
            bool success = updater->download_plugin_update(id, name, commit);
            if (!success) {
                updater->dispatch_progress("Update failed", 0, true, false);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Background plugin update failed: {}", e.what());
            updater->dispatch_progress("Update failed", 0, true, false);
        }
    }).detach();

    return nlohmann::json{
        { "opId", op_id }
    };
}
builtin_payload head::millennium_backend::Core_KillPluginBackend(const builtin_payload& args)
{
    auto pluginName = args["pluginName"].get<std::string>();
    g_millennium->get_plugin_loader()->get_backend_manager()->destroy_plugin(pluginName);
    return {};
}
builtin_payload head::millennium_backend::Core_InstallPlugin(const builtin_payload& args)
{
    if (!args.contains("download_url") || !args["download_url"].is_string()) {
        LOG_ERROR("Core_InstallPlugin called without a valid 'download_url' string.");
        return {
            { "success", false                               },
            { "error",   "missing or invalid 'download_url'" }
        };
    }

    auto plugin_updater_weak = m_updater->get_plugin_updater();
    auto updater = m_updater;
    auto download_url = args["download_url"].get<std::string>();
    int op_id = updater->start_operation();

    size_t total_size = 0;
    if (args.contains("total_size") && args["total_size"].is_number()) {
        total_size = args["total_size"].get<size_t>();
    }

    std::thread([plugin_updater_weak, updater, download_url, total_size, op_id]()
    {
        updater->set_thread_op_id(op_id);
        try {
            if (auto plugin_updater = plugin_updater_weak.lock()) {
                auto result = plugin_updater->install_plugin(download_url, total_size);
                bool success = result.value("success", false);
                if (!success) {
                    updater->dispatch_progress(result.value("error", "Install failed"), 0, true, false);
                }
            } else {
                LOG_ERROR("Failed to lock plugin_updater, it likely shutdown.");
                updater->dispatch_progress("Install failed", 0, true, false);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Background plugin install failed: {}", e.what());
            updater->dispatch_progress("Install failed", 0, true, false);
        }
    }).detach();

    return nlohmann::json{
        { "success", true  },
        { "started", true  },
        { "opId",    op_id }
    };
}
builtin_payload head::millennium_backend::Core_IsPluginInstalled(const builtin_payload& args)
{
    if (auto plugin_updater = m_updater->get_plugin_updater().lock()) {
        std::string plugin_name;

        if (args.contains("plugin_name") && args["plugin_name"].is_string()) {
            plugin_name = args["plugin_name"].get<std::string>();
        } else if (args.contains("pluginName") && args["pluginName"].is_string()) {
            plugin_name = args["pluginName"].get<std::string>();
        } else {
            LOG_ERROR("Core_IsPluginInstalled called without 'plugin_name'/'pluginName' string.");
            return false;
        }

        return plugin_updater->is_plugin_installed(plugin_name);
    }

    LOG_ERROR("Failed to lock plugin_updater, it likely shutdown.");
    return false;
}
builtin_payload head::millennium_backend::Core_UninstallPlugin(const builtin_payload& args)
{
    if (auto plugin_updater = m_updater->get_plugin_updater().lock()) {
        std::string plugin_name;

        if (args.contains("pluginName") && args["pluginName"].is_string()) {
            plugin_name = args["pluginName"].get<std::string>();
        } else if (args.contains("plugin_name") && args["plugin_name"].is_string()) {
            plugin_name = args["plugin_name"].get<std::string>();
        } else {
            LOG_ERROR("Core_UninstallPlugin called without 'pluginName'/'plugin_name' string.");
            return false;
        }

        return plugin_updater->uninstall_plugin(plugin_name);
    }

    LOG_ERROR("Failed to lock plugin_updater, it likely shutdown.");
    return false;
}

/** Get plugin backend logs */
builtin_payload head::millennium_backend::Core_GetPluginBackendLogs(const builtin_payload&)
{
    nlohmann::json logData = nlohmann::json::array();
    std::vector<plugin_manager::plugin_t> plugins = m_plugin_manager->get_all_plugins();

    for (auto& logger : get_plugin_logger_mgr()) {
        nlohmann::json logDataItem;

        for (auto [message, logLevel, timestamp] : logger->collect_logs()) {
            logDataItem.push_back({
                { "message",   Base64Encode(message) },
                { "level",     logLevel              },
                { "timestamp", timestamp             }
            });
        }

        std::string pluginName = logger->get_plugin_name(false);

        for (auto& plugin : plugins) {
            if (plugin.plugin_json.contains("name") && plugin.plugin_json["name"] == logger->get_plugin_name(false)) {
                pluginName = plugin.plugin_json.value("common_name", pluginName);
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

    const auto pending_crashes = mep::crash_event_bus::instance().get_pending_crashes();
    for (const auto& crash : pending_crashes) {
        if (crash.crash_dump_dir.empty()) continue;

        auto crash_log_path = std::filesystem::path(crash.crash_dump_dir) / "crash.log";
        if (!std::filesystem::is_regular_file(crash_log_path)) continue;

        std::ifstream file(crash_log_path);
        if (!file.is_open()) continue;

        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            content += line + "\n";
            if (line.find("--- End of Crash Report ---") != std::string::npos) break;
        }
        if (content.empty()) continue;

        content += "\nCrash dump: " + crash.crash_dump_dir + "\n";

        nlohmann::json crash_entry = {
            { "message",   Base64Encode(content) },
            { "level",     2 /* error */         },
            { "timestamp", ""                    }
        };

        auto& loggers = get_plugin_logger_mgr();
        for (size_t i = 0; i < loggers.size() && i < logData.size(); ++i) {
            if (loggers[i]->get_plugin_name(false) == crash.plugin_name) {
                logData[i]["logs"].push_back(crash_entry);
                break;
            }
        }
    }

    return logData;
}

/** Update Millennium API */
builtin_payload head::millennium_backend::Core_UpdateMillennium(const builtin_payload& args)
{
    m_millennium_updater->update(args["downloadUrl"], args["downloadSize"], args["background"]);
    return {};
}

builtin_payload head::millennium_backend::Core_HasPendingMillenniumUpdateRestart(const builtin_payload&)
{
    return m_millennium_updater->is_pending_restart();
}

builtin_payload head::millennium_backend::Core_GetPendingCrashes(const builtin_payload&)
{
    const auto crashes = mep::crash_event_bus::instance().get_pending_crashes();
    const auto all_plugins = m_plugin_manager->get_all_plugins();
    nlohmann::json result = nlohmann::json::array();

    for (const auto& ev : crashes) {
        std::string display_name = ev.display_name;
        if (display_name.empty()) {
            for (const auto& p : all_plugins) {
                if (p.plugin_name == ev.plugin_name) {
                    display_name = p.plugin_json.value("common_name", ev.plugin_name);
                    break;
                }
            }
        }
        if (display_name.empty()) display_name = ev.plugin_name;

        result.push_back({
            { "plugin",       ev.plugin_name         },
            { "displayName",  display_name           },
            { "exitCode",     (uint32_t)ev.exit_code },
            { "crashDumpDir", ev.crash_dump_dir      }
        });
    }

    return result.dump();
}

builtin_payload head::millennium_backend::Core_AcknowledgeCrash(const builtin_payload& args)
{
    mep::crash_event_bus::instance().acknowledge_crash(args["plugin"].get<std::string>());
    return {};
}

builtin_payload head::millennium_backend::ipc_message_hdlr(const std::string& functionName, const builtin_payload& args)
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

void head::millennium_backend::set_ipc_main(std::shared_ptr<ipc_main> ipc_main)
{
    m_ipc_main = std::move(ipc_main);

    if (m_updater) {
        m_updater->set_ipc_main(m_ipc_main);
    }
}
