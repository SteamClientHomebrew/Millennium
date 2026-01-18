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

#include "millennium/logger.h"
#include "millennium/config.h"
#include "millennium/environment.h"
#include "millennium/plat_msg.h"

#include <fmt/core.h>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

#include "head/default_cfg.h"

settings_store::settings_store()
{
    const auto path = std::filesystem::path(GetEnv("MILLENNIUM__CONFIG_PATH")) / "millennium.ini";

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

/**
 * @brief Convert a vector of strings to a single string.
 * INI files don't support arrays, so we need to convert the vector to a string.
 * We use a pipe delimiter to separate the plugins.
 */
std::string ConvertVectorToString(std::vector<std::string> enabledPlugins)
{
    std::string strEnabledPlugins;
    for (const auto& plugin : enabledPlugins) {
        strEnabledPlugins += plugin + "|";
    }

    return strEnabledPlugins.substr(0, strEnabledPlugins.size() - 1);
}

nlohmann::json ResetEnabledPlugins()
{
    CONFIG.set("plugins.enabledPlugins", std::vector<std::string>{});
    return {};
}

/**
 * @brief Initialize and optionally fix the settings store.
 */
int settings_store::init()
{
    nlohmann::json enabledPlugins = CONFIG.get("plugins.enabledPlugins", std::vector<std::string>{});

    if (!enabledPlugins.is_array()) {
        enabledPlugins = ResetEnabledPlugins();
    }

    for (const auto& plugin : enabledPlugins) {
        if (!plugin.is_string()) {
            enabledPlugins = ResetEnabledPlugins();
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
bool settings_store::set_plugin_enabled(std::string pluginName, bool enabled)
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
bool settings_store::is_enabled(std::string plugin_name)
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
void settings_store::lint_plugin(nlohmann::json json, std::string pluginName)
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
                logger.warn("plugin '{}' doesn't contain a property field '{}' in '{}'", pluginName, property, settings_store::plugin_config_file);
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
settings_store::plugin_t settings_store::get_plugin_internal_metadata(nlohmann::json json, std::filesystem::directory_entry entry)
{
    /** Check if the plugin json contains all the required fields */
    lint_plugin(json, entry.path().filename().string());

    const auto backend_type = json.value("backendType", "python") == "lua" ? backend_t::Lua : backend_t::Python;

    settings_store::plugin_t plugin;
    plugin.plugin_json = json;
    plugin.plugin_name = json["name"];
    plugin.plugin_base_dir = entry.path();
    plugin.plugin_backend_dir = entry.path() / json.value("backend", "backend") / (backend_type == backend_t::Lua ? "main.lua" : "main.py");
    plugin.plugin_frontend_dir = entry.path() / ".millennium" / "Dist" / "index.js";
    plugin.plugin_webkit_path = entry.path() / ".millennium" / "Dist" / "webkit.js";
    plugin.backend_type = backend_type;

    return plugin;
}

std::vector<settings_store::plugin_t> settings_store::get_all_plugins()
{
    std::vector<settings_store::plugin_t> plugins;
    const auto plugin_path = std::filesystem::path(GetEnv("MILLENNIUM__PLUGINS_PATH"));

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

            const auto pluginConfiguration = entry.path() / settings_store::plugin_config_file;

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
std::vector<settings_store::plugin_t> settings_store::get_enabled_plugins()
{
    const auto allPlugins = this->get_all_plugins();
    std::vector<settings_store::plugin_t> enabledPlugins;

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
std::vector<std::string> settings_store::get_enabled_plugin_names()
{
    std::vector<std::string> enabledPlugins;
    std::vector<settings_store::plugin_t> plugins = this->get_enabled_plugins();

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
std::vector<settings_store::plugin_t> settings_store::get_enabled_backends()
{
    const auto allPlugins = this->get_all_plugins();
    std::vector<settings_store::plugin_t> enabledBackends;

    for (auto& plugin : allPlugins) {
        if (this->is_enabled(plugin.plugin_name) && plugin.plugin_json.value("useBackend", true)) {
            enabledBackends.push_back(plugin);
        }
    }

    return enabledBackends;
}

nlohmann::json config_manager::get(const std::string& path, const nlohmann::json& def)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    const nlohmann::json* current = &_data;
    size_t start = 0, end = 0;

    while ((end = path.find('.', start)) != std::string::npos) {
        std::string key = path.substr(start, end - start);
        if (!current->contains(key)) return def;
        current = &(*current)[key];
        start = end + 1;
    }

    std::string last_key = path.substr(start);
    if (!current->contains(last_key)) return def;
    return (*current)[last_key];
}

void config_manager::set(const std::string& path, const nlohmann::json& value, bool skipPropagation)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    nlohmann::json* current = &_data;
    size_t start = 0, end = 0;

    while ((end = path.find('.', start)) != std::string::npos) {
        std::string key = path.substr(start, end - start);
        if (!current->contains(key) || !(*current)[key].is_object()) (*current)[key] = nlohmann::json::object();

        current = &(*current)[key];
        start = end + 1;
    }

    std::string last_key = path.substr(start);
    if (!current->is_object()) {
        *current = nlohmann::json::object();
    }

    nlohmann::json old_value = nullptr;
    if (current->contains(last_key)) {
        old_value = (*current)[last_key];
    }

    if (old_value != value) {
        (*current)[last_key] = value;
        notify_listeners(path, old_value, value);
        if (!skipPropagation) save_to_disk();
    }
}

void config_manager::register_listener(listener listener)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _listeners.push_back(listener);
}

void config_manager::unregister_listener(listener _listener)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _listeners.erase(std::remove_if(_listeners.begin(), _listeners.end(),
                                    [&](const listener& l)
    {
        return l.target_type() == _listener.target_type() && l.target<void(const std::string&, const nlohmann::json&, const nlohmann::json&)>() ==
                                                                 _listener.target<void(const std::string&, const nlohmann::json&, const nlohmann::json&)>();
    }),
                     _listeners.end());
}

void config_manager::merge_default_config(nlohmann::json& current, const nlohmann::json& incoming, const std::string& path)
{
    for (auto& [key, value] : incoming.items()) {
        std::string fullKey = path.empty() ? key : path + "." + key;
        if (value.is_object()) {
            if (!current.contains(key) || !current[key].is_object()) {
                current[key] = nlohmann::json::object();
            }
            merge_default_config(current[key], value, fullKey);
        } else {
            if (!current.contains(key)) {
                current[key] = value;
                notify_listeners(fullKey, nullptr, value);
            }
        }
    }
}

void config_manager::load_from_disk()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    /** Clean up any stale temp file from interrupted writes */
    std::string tempFilename = _filename + ".tmp";
    if (std::filesystem::exists(tempFilename)) {
        logger.warn("Found stale temp config file, removing: {}", tempFilename);
        std::filesystem::remove(tempFilename);
    }

    {
        std::ifstream file(_filename);

        if (file.is_open()) {
            try {
                file >> _data;
            } catch (...) {
                logger.warn("Invalid JSON in config file: {}", _filename);
                Plat_ShowMessageBox("Millennium", fmt::format("The config file at '{}' contains invalid JSON and will be reset to defaults.", _filename).c_str(),
                                    MESSAGEBOX_WARNING);
                _data = nlohmann::json::object();
            }
        } else {
            logger.warn("Failed to open config file: {}", _filename);
            Plat_ShowMessageBox("Millennium", fmt::format("The config file at '{}' could not be opened and will be reset to defaults.", _filename).c_str(), MESSAGEBOX_WARNING);
            _data = nlohmann::json::object();
        }
    }

    merge_default_config(_data, _defaults, "");
    save_to_disk();
}

void config_manager::save_to_disk()
{
    // Serialize all file I/O operations to prevent concurrent writes
    std::lock_guard<std::mutex> save_lock(_save_mutex);
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    /**
     * Use atomic write pattern: write to temp file, then rename
     * This prevents corruption if the app is terminated mid-write
     */
    std::string tempFilename = _filename + ".tmp";

    {
        std::ofstream file(tempFilename);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open temp config file for writing: {}", tempFilename);
            return;
        }

        file << _data.dump(2);
        file.flush();

        if (file.fail()) {
            LOG_ERROR("Failed to write config data to temp file: {}", tempFilename);
            file.close();
            std::filesystem::remove(tempFilename);
            return;
        }
    }

    /** atomic rename - either the old file exists or the new one, never partial */
    std::error_code ec;
    std::filesystem::rename(tempFilename, _filename, ec);

    if (ec) {
        LOG_ERROR("Failed to rename temp config file: {} -> {}: {}", tempFilename, _filename, ec.message());
        std::filesystem::remove(tempFilename);
    }
}

void config_manager::set_default_config(const std::string& key, const nlohmann::json& value)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _defaults[key] = value;
}

nlohmann::json config_manager::set_all(const nlohmann::json& newConfig, bool skipPropagation)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    try {
        nlohmann::json old_data = _data;
        _data = newConfig;

        if (!skipPropagation) {
            for (auto& [k, v] : newConfig.items()) {
                notify_listeners(k, old_data.value(k, nlohmann::json(nullptr)), v);
            }
        }

        save_to_disk();
        return _data;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to set entire config: {}", e.what());
        return nlohmann::json::object();
    }
}

nlohmann::json config_manager::get_all()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    nlohmann::json result = _defaults;
    for (auto& [k, v] : _data.items())
        result[k] = v;
    return result;
}

config_manager::config_manager()
{
    _filename = GetEnv("MILLENNIUM__CONFIG_PATH") + "/config.json";
    _defaults = GetDefaultConfig();

    if (!std::filesystem::exists(_filename)) {
        logger.warn("Could not find config file at: {}, attempting to create it...", _filename);
        std::filesystem::create_directories(std::filesystem::path(_filename).parent_path());

        {
            /** create the config file itself */
            std::ofstream file(_filename);
            file << _defaults.dump(4);
        }
    }

    load_from_disk();
}

config_manager::~config_manager()
{
    save_to_disk();
}

void config_manager::notify_listeners(const std::string& key, const nlohmann::json& old_value, const nlohmann::json& new_value)
{
    std::string key_copy = key;
    nlohmann::json old_value_copy = old_value;
    nlohmann::json new_value_copy = new_value;
    std::vector<listener> listeners_copy;

    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        listeners_copy = _listeners;
    }

    for (auto& listener : listeners_copy) {
        try {
            listener(key_copy, old_value_copy, new_value_copy);
        } catch (const nlohmann::json::exception& e) {
            LOG_ERROR("Listener JSON exception for key: {}: {}", key_copy, e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Listener exception for key: {}: {}", key_copy, e.what());
        } catch (...) {
            LOG_ERROR("Listener exception for key: {}", key_copy);
        }
    }
}
