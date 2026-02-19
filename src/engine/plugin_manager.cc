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

#include "millennium/plugin_manager.h"
#include "millennium/filesystem.h"
#include "millennium/logger.h"
#include "millennium/config.h"
#include "millennium/environment.h"
#include "mini/ini.h"

#include <fmt/core.h>
#ifdef _WIN32
#include <windows.h>
#endif

plugin_manager::plugin_manager()
{
    const auto path = std::filesystem::path(platform::environment::get("MILLENNIUM__CONFIG_PATH")) / "millennium.ini";

    if (std::filesystem::exists(path)) {
        logger.log("Found legacy config file at {}, migrating to new config system...", path.string());

        mINI::INIFile iniFile(path.string());
        mINI::INIStructure ini;
        iniFile.read(ini);

        if (ini["Settings"].has("enabled_plugins")) {
            logger.log("Migrating enabled plugins from legacy config...");

            std::string token;
            std::vector<std::string> enabledPlugins;
            std::istringstream tokenStream(ini["Settings"]["enabled_plugins"]);

            while (std::getline(tokenStream, token, '|')) {
                if (token == "core") {
                    continue; /** no need to migrate it, it doesn't exist anymore */
                }

                logger.log(" - Migrated plugin: {}", token);
                enabledPlugins.push_back(token);
            }

            logger.log("Successfully migrated {} enabled plugins from legacy config.", enabledPlugins.size());
            CONFIG.set("plugins.enabledPlugins", enabledPlugins);
        } else {
            logger.log("No enabled plugins found in legacy config.");
        }

        logger.warn("Removing legacy config file at {}, it is no longer needed.", path.string());
        std::filesystem::remove(path);
    }
}

nlohmann::json reset_enabled_plugins()
{
    CONFIG.set("plugins.enabledPlugins", std::vector<std::string>{});
    return {};
}

/**
 * @brief Initialize and optionally fix the settings store.
 */
int plugin_manager::init()
{
    nlohmann::json enabledPlugins = CONFIG.get("plugins.enabledPlugins", std::vector<std::string>{});

    if (!enabledPlugins.is_array()) {
        enabledPlugins = reset_enabled_plugins();
    }

    for (const auto& plugin : enabledPlugins) {
        if (!plugin.is_string()) {
            enabledPlugins = reset_enabled_plugins();
            break;
        }
    }
    return 0;
}

/**
 * @brief Toggle the status of a plugin.
 *
 * @param pluginName The name of the plugin.
 * @param enabled The status of the plugin.
 */
bool plugin_manager::set_plugin_enabled(std::string pluginName, bool enabled)
{
    logger.log("Opting to {} {}", enabled ? "enable" : "disable", pluginName);
    nlohmann::json enabledPluginsJson = CONFIG.get("plugins.enabledPlugins", std::vector<std::string>{});

    std::vector<std::string> enabledPlugins;
    if (enabledPluginsJson.is_array()) {
        for (const auto& plugin : enabledPluginsJson) {
            if (plugin.is_string()) {
                enabledPlugins.push_back(plugin.get<std::string>());
            }
        }
    }

    if (enabled) {
        if (std::find(enabledPlugins.begin(), enabledPlugins.end(), pluginName) == enabledPlugins.end()) {
            enabledPlugins.push_back(pluginName);
        }
    } else {
        auto it = std::find(enabledPlugins.begin(), enabledPlugins.end(), pluginName);
        if (it != enabledPlugins.end()) {
            enabledPlugins.erase(it);
        }
    }

    CONFIG.set("plugins.enabledPlugins", enabledPlugins);
    return true;
}

/**
 * @brief Check if a plugin is enabled.
 * @param plugin_name The name of the plugin.
 */
bool plugin_manager::is_enabled(std::string plugin_name)
{
    nlohmann::json enabledPluginsJson = CONFIG.get("plugins.enabledPlugins", std::vector<std::string>{});

    if (!enabledPluginsJson.is_array()) {
        return false;
    }

    for (const auto& plugin : enabledPluginsJson) {
        if (plugin.is_string() && plugin.get<std::string>() == plugin_name) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Lint the plugin data to ensure that it contains the required fields.
 *
 * @param json The JSON object to lint.
 * @param pluginName The name of the plugin.
 */
void plugin_manager::lint_plugin(nlohmann::json json, std::string pluginName)
{
    /** Find a total list of all plugin.json keys in `./plugin-schema.json` */
    const std::map<std::string, bool> pluginFields = {
        { "name",        true  },
        { "description", false },
        { "common_name", false },
    };

    for (const auto& field : pluginFields) {
        auto [property, required] = field;

        if (!json.contains(property)) {
            if (required) {
                throw std::runtime_error(fmt::format("plugin configuration must contain '{}'", property));
            } else {
                logger.warn("plugin '{}' doesn't contain a property field '{}' in '{}'", pluginName, property, plugin_manager::plugin_config_file);
            }
        }
    }
}

/**
 * @brief Get the internal data for a plugin.
 *
 * @param json The JSON object from the plugins plugin.json file.
 * @param entry The directory entry for the plugin.
 * @return SettingsStore::PluginTypeSchema The plugin data.
 */
plugin_manager::plugin_t plugin_manager::get_plugin_internal_metadata(nlohmann::json json, std::filesystem::directory_entry entry)
{
    /** Check if the plugin json contains all the required fields */
    lint_plugin(json, entry.path().filename().string());

    const auto backend_type = json.value("backendType", "python") == "lua" ? backend_t::Lua : backend_t::Python;

    plugin_manager::plugin_t plugin;
    plugin.plugin_json = json;
    plugin.plugin_name = json["name"];
    plugin.plugin_base_dir = entry.path();
    plugin.plugin_backend_dir = entry.path() / json.value("backend", "backend") / (backend_type == backend_t::Lua ? "main.lua" : "main.py");
    plugin.plugin_frontend_dir = entry.path() / ".millennium" / "Dist" / "index.js";
    plugin.plugin_webkit_path = entry.path() / ".millennium" / "Dist" / "webkit.js";
    plugin.backend_type = backend_type;

    return plugin;
}

std::vector<plugin_manager::plugin_t> plugin_manager::get_all_plugins()
{
    std::vector<plugin_manager::plugin_t> plugins;
    const auto plugin_path = std::filesystem::path(platform::environment::get("MILLENNIUM__PLUGINS_PATH"));

    try {
        /** Create the plugins folder if it doesn't exist. */
        std::filesystem::create_directories(plugin_path);
    } catch (const std::exception& exception) {
        LOG_ERROR("An error occurred creating plugin directories -> {}", exception.what());
    }

    /** Find the remaining plugins from the user plugins folder. */
    try {
        for (const auto& entry : std::filesystem::directory_iterator(plugin_path)) {
            if (!entry.is_directory()) {
                continue;
            }

            const auto pluginConfiguration = entry.path() / plugin_manager::plugin_config_file;

            if (!std::filesystem::exists(pluginConfiguration)) {
                continue;
            }

            try {
                const auto pluginJson = platform::read_file_json(pluginConfiguration.string());
                const auto pluginData = get_plugin_internal_metadata(pluginJson, entry);

                plugins.push_back(pluginData);
            } catch (platform::file_exception& exception) {
                LOG_ERROR("An error occurred reading plugin '{}', exception: {}", entry.path().string(), exception.what());
            } catch (std::exception& exception) {
                LOG_ERROR("An error occurred parsing plugin '{}', exception: {}", entry.path().string(), exception.what());
            }
        }
    } catch (const std::exception& ex) {
        LOG_ERROR("Fall back exception caught trying to parse plugins. {}", ex.what());
    }

    return plugins;
}

/**
 * @brief Get all the enabled plugins from the plugin list.
 */
std::vector<plugin_manager::plugin_t> plugin_manager::get_enabled_plugins()
{
    const auto allPlugins = this->get_all_plugins();
    std::vector<plugin_manager::plugin_t> enabledPlugins;

    for (auto& plugin : allPlugins) {
        if (this->is_enabled(plugin.plugin_name)) {
            enabledPlugins.push_back(plugin);
        }
    }

    return enabledPlugins;
}

/**
 * @brief Get the names of all enabled plugins.
 */
std::vector<std::string> plugin_manager::get_enabled_plugin_names()
{
    std::vector<std::string> enabledPlugins;
    std::vector<plugin_manager::plugin_t> plugins = this->get_enabled_plugins();

    /** only add the plugin name. */
    std::transform(plugins.begin(), plugins.end(), std::back_inserter(enabledPlugins), [](auto& plugin) { return plugin.plugin_name; });
    return enabledPlugins;
}

/**
 * @brief Get all the enabled backends from the plugin list.
 *
 * @note This function filters out the plugins that are not enabled or have the useBackend flag set to false.
 * Not all enabled plugins have backends.
 */
std::vector<plugin_manager::plugin_t> plugin_manager::get_enabled_backends()
{
    const auto allPlugins = this->get_all_plugins();
    std::vector<plugin_manager::plugin_t> enabledBackends;

    for (auto& plugin : allPlugins) {
        if (this->is_enabled(plugin.plugin_name) && plugin.plugin_json.value("useBackend", true)) {
            enabledBackends.push_back(plugin);
        }
    }

    return enabledBackends;
}
