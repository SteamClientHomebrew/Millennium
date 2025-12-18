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
#include "millennium/plat_msg.h"
#include "millennium/sysfs.h"
#include "millennium/env.h"

#include <fmt/core.h>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#endif

#include "head/default_cfg.h"

SettingsStore::SettingsStore()
{
    const auto path = std::filesystem::path(GetEnv("MILLENNIUM__CONFIG_PATH")) / "millennium.ini";

    if (std::filesystem::exists(path)) {
        Logger.Log("Found legacy config file at {}, migrating to new config system...", path.string());

        mINI::INIFile iniFile(path.string());
        mINI::INIStructure ini;
        iniFile.read(ini);

        if (ini["Settings"].has("enabled_plugins")) {
            Logger.Log("Migrating enabled plugins from legacy config...");

            std::string token;
            std::vector<std::string> enabledPlugins;
            std::istringstream tokenStream(ini["Settings"]["enabled_plugins"]);

            while (std::getline(tokenStream, token, '|')) {
                if (token == "core") {
                    continue; /** no need to migrate it, it doesn't exist anymore */
                }

                Logger.Log(" - Migrated plugin: {}", token);
                enabledPlugins.push_back(token);
            }

            Logger.Log("Successfully migrated {} enabled plugins from legacy config.", enabledPlugins.size());
            CONFIG.SetNested("plugins.enabledPlugins", enabledPlugins);
        } else {
            Logger.Log("No enabled plugins found in legacy config.");
        }

        Logger.Warn("Removing legacy config file at {}, it is no longer needed.", path.string());
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
    CONFIG.SetNested("plugins.enabledPlugins", std::vector<std::string>{});
    return {};
}

/**
 * @brief Initialize and optionally fix the settings store.
 */
int SettingsStore::InitializeSettingsStore()
{
    nlohmann::json enabledPlugins = CONFIG.GetNested("plugins.enabledPlugins", std::vector<std::string>{});

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
bool SettingsStore::TogglePluginStatus(std::string pluginName, bool enabled)
{
    Logger.Log("Opting to {} {}", enabled ? "enable" : "disable", pluginName);

    // Get the current enabled plugins as JSON array
    nlohmann::json enabledPluginsJson = CONFIG.GetNested("plugins.enabledPlugins", std::vector<std::string>{});

    // Convert to vector for easier manipulation
    std::vector<std::string> enabledPlugins;
    if (enabledPluginsJson.is_array()) {
        for (const auto& plugin : enabledPluginsJson) {
            if (plugin.is_string()) {
                enabledPlugins.push_back(plugin.get<std::string>());
            }
        }
    }

    /** Enable the target plugin */
    if (enabled) {
        if (std::find(enabledPlugins.begin(), enabledPlugins.end(), pluginName) == enabledPlugins.end()) {
            enabledPlugins.push_back(pluginName);
        }
    }
    /** Disable the target plugin */
    else {
        auto it = std::find(enabledPlugins.begin(), enabledPlugins.end(), pluginName);
        if (it != enabledPlugins.end()) {
            enabledPlugins.erase(it);
        }
    }

    CONFIG.SetNested("plugins.enabledPlugins", enabledPlugins);
    return true;
}

/**
 * @brief Check if a plugin is enabled.
 * @param plugin_name The name of the plugin.
 */
bool SettingsStore::IsEnabledPlugin(std::string plugin_name)
{
    nlohmann::json enabledPluginsJson = CONFIG.GetNested("plugins.enabledPlugins", std::vector<std::string>{});

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
void SettingsStore::LintPluginData(nlohmann::json json, std::string pluginName)
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
                Logger.Warn("plugin '{}' doesn't contain a property field '{}' in '{}'", pluginName, property, SettingsStore::pluginConfigFile);
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
SettingsStore::PluginTypeSchema SettingsStore::GetPluginInternalData(nlohmann::json json, std::filesystem::directory_entry entry)
{
    SettingsStore::PluginTypeSchema plugin;
    const std::string pluginDirName = entry.path().filename().string();

    /** Check if the plugin json contains all the required fields */
    LintPluginData(json, pluginDirName);

    plugin.backendType = json.value("backendType", "python") == "lua" ? Lua : Python;
    plugin.pluginJson = json;
    plugin.pluginName = json["name"];
    plugin.pluginBaseDirectory = entry.path();
    plugin.backendAbsoluteDirectory = entry.path() / json.value("backend", "backend") / (plugin.backendType == Lua ? "main.lua" : "main.py");
    plugin.frontendAbsoluteDirectory = entry.path() / ".millennium" / "Dist" / "index.js";
    plugin.webkitAbsolutePath = entry.path() / ".millennium" / "Dist" / "webkit.js";
    plugin.backendType = json.value("backendType", "python") == "lua" ? Lua : Python;

    return plugin;
}

std::vector<SettingsStore::PluginTypeSchema> SettingsStore::ParseAllPlugins()
{
    std::vector<SettingsStore::PluginTypeSchema> plugins;
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

            const auto pluginConfiguration = entry.path() / SettingsStore::pluginConfigFile;

            if (!std::filesystem::exists(pluginConfiguration)) {
                continue;
            }

            try {
                const auto pluginJson = SystemIO::ReadJsonSync(pluginConfiguration.string());
                const auto pluginData = GetPluginInternalData(pluginJson, entry);

                plugins.push_back(pluginData);
            } catch (SystemIO::FileException& exception) {
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
std::vector<SettingsStore::PluginTypeSchema> SettingsStore::GetEnabledPlugins()
{
    const auto allPlugins = this->ParseAllPlugins();
    std::vector<SettingsStore::PluginTypeSchema> enabledPlugins;

    for (auto& plugin : allPlugins) {
        if (this->IsEnabledPlugin(plugin.pluginName)) {
            enabledPlugins.push_back(plugin);
        }
    }

    return enabledPlugins;
}

/**
 * @brief Get the names of all enabled plugins.
 */
std::vector<std::string> SettingsStore::GetEnabledPluginNames()
{
    std::vector<std::string> enabledPlugins;
    std::vector<SettingsStore::PluginTypeSchema> plugins = this->GetEnabledPlugins();

    /** only add the plugin name. */
    std::transform(plugins.begin(), plugins.end(), std::back_inserter(enabledPlugins), [](auto& plugin) { return plugin.pluginName; });
    return enabledPlugins;
}

/**
 * @brief Get all the enabled backends from the plugin list.
 *
 * @note This function filters out the plugins that are not enabled or have the useBackend flag set to false.
 * Not all enabled plugins have backends.
 */
std::vector<SettingsStore::PluginTypeSchema> SettingsStore::GetEnabledBackends()
{
    const auto allPlugins = this->ParseAllPlugins();
    std::vector<SettingsStore::PluginTypeSchema> enabledBackends;

    for (auto& plugin : allPlugins) {
        if (this->IsEnabledPlugin(plugin.pluginName) && plugin.pluginJson.value("useBackend", true)) {
            enabledBackends.push_back(plugin);
        }
    }

    return enabledBackends;
}

ConfigManager& ConfigManager::Instance()
{
    static ConfigManager instance;
    return instance;
}

nlohmann::json ConfigManager::GetNested(const std::string& path, const nlohmann::json& def)
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

void ConfigManager::SetNested(const std::string& path, const nlohmann::json& value, bool skipPropagation)
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
        NotifyListeners(path, old_value, value);
        if (!skipPropagation) SaveToFile();
    }
}

void ConfigManager::Delete(const std::string& key)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (_data.contains(key)) {
        nlohmann::json old_value = _data[key];
        _data.erase(key);
        NotifyListeners(key, old_value, nullptr);
        SaveToFile();
    }
}

void ConfigManager::RegisterListener(Listener listener)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _listeners.push_back(listener);
}

void ConfigManager::UnregisterListener(Listener listener)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _listeners.erase(std::remove_if(_listeners.begin(), _listeners.end(),
                                    [&](const Listener& l)
    {
        return l.target_type() == listener.target_type() && l.target<void(const std::string&, const nlohmann::json&, const nlohmann::json&)>() ==
                                                                listener.target<void(const std::string&, const nlohmann::json&, const nlohmann::json&)>();
    }),
                     _listeners.end());
}

void ConfigManager::MergeDefaults(nlohmann::json& current, const nlohmann::json& incoming, const std::string& path)
{
    for (auto& [key, value] : incoming.items()) {
        std::string fullKey = path.empty() ? key : path + "." + key;
        if (value.is_object()) {
            if (!current.contains(key) || !current[key].is_object()) {
                current[key] = nlohmann::json::object();
            }
            MergeDefaults(current[key], value, fullKey);
        } else {
            if (!current.contains(key)) {
                current[key] = value;
                NotifyListeners(fullKey, nullptr, value);
            }
        }
    }
}

void ConfigManager::LoadFromFile()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    std::ifstream file(_filename);

    if (file.is_open()) {
        try {
            file >> _data;
        } catch (...) {
            Logger.Warn("Invalid JSON in config file: {}", _filename);
#ifdef _WIN32
            Plat_ShowMessageBox("Millennium", fmt::format("The config file at '{}' contains invalid JSON and will be reset to defaults.", _filename).c_str(), MESSAGEBOX_WARNING);
#endif
            _data = nlohmann::json::object();
        }
    } else {
        Logger.Warn("Failed to open config file: {}", _filename);
#ifdef _WIN32
        Plat_ShowMessageBox("Millennium", fmt::format("The config file at '{}' could not be opened and will be reset to defaults.", _filename).c_str(), MESSAGEBOX_WARNING);
#endif
        _data = nlohmann::json::object();
    }

    MergeDefaults(_data, _defaults, "");
    SaveToFile();
}

void ConfigManager::SaveToFile()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    std::ofstream file(_filename);

    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file for writing: {}", _filename);
        return;
    }

    file << _data.dump(2);
}

void ConfigManager::SetDefault(const std::string& key, const nlohmann::json& value)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _defaults[key] = value;
}

nlohmann::json ConfigManager::SetAll(const nlohmann::json& newConfig, bool skipPropagation)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    try {
        nlohmann::json old_data = _data;
        _data = newConfig;

        if (!skipPropagation) {
            for (auto& [k, v] : newConfig.items()) {
                NotifyListeners(k, old_data.value(k, nlohmann::json(nullptr)), v);
            }
        }

        SaveToFile();
        return _data;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to set entire config: {}", e.what());
        return nlohmann::json::object();
    }
}

nlohmann::json ConfigManager::GetAll()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    nlohmann::json result = _defaults;
    for (auto& [k, v] : _data.items())
        result[k] = v;
    return result;
}

ConfigManager::ConfigManager()
{
    _filename = GetEnv("MILLENNIUM__CONFIG_PATH") + "/config.json";
    _defaults = GetDefaultConfig();

    if (!std::filesystem::exists(_filename)) {
        Logger.Warn("Could not find config file at: {}, attempting to create it...", _filename);
        std::filesystem::create_directories(std::filesystem::path(_filename).parent_path());

        /** create the config file itself */
        std::ofstream file(_filename);
        file << _defaults.dump(4);
        file.close();
    }

    LoadFromFile();
}

ConfigManager::~ConfigManager()
{
    SaveToFile();
}

void ConfigManager::NotifyListeners(const std::string& key, const nlohmann::json& old_value, const nlohmann::json& new_value)
{
    // Make copies of the data and get a copy of listeners while holding the lock
    std::string key_copy = key;
    nlohmann::json old_value_copy = old_value;
    nlohmann::json new_value_copy = new_value;
    std::vector<Listener> listeners_copy;

    {
        // Only hold the lock long enough to copy the listeners
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        listeners_copy = _listeners;
    }
    // Lock is released here before calling listeners

    // Call listeners without holding the lock
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
