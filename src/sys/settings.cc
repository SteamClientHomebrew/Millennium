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

#include "locals.h"
#include <fstream>
#include <internal_logger.h>
#include <fmt/core.h>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif
#include <env.h>


namespace FileSystem = std::filesystem;

SettingsStore::SettingsStore() : file(mINI::INIFile(std::string())), ini(mINI::INIStructure())
{
    const auto path = std::filesystem::path(GetEnv("MILLENNIUM__CONFIG_PATH")) / "millennium.ini";

    if (!FileSystem::exists(path))
    {
        FileSystem::create_directories(path.parent_path());
        std::ofstream outputFile(path.string());
    }

    try 
    {
        this->file = mINI::INIFile(path.string());

        this->file.read(ini);
        this->file.write(ini);
    }
    catch (const std::exception& ex)
    {
        Logger.Warn("An error occurred reading settings file -> {}", ex.what());
    }
}

/** 
 * @brief Set a setting in the settings store.
 * The setting key will be placed in the `Settings` section of the INI file.
 */
void SettingsStore::SetSetting(std::string key, std::string settingsData)
{
    this->ini["Settings"][key] = settingsData;
    this->file.write(ini);
}

/**
 * @brief Get a setting from the settings store.
 * The setting key will be retrieved from the `Settings` section of the INI file.
 */
std::string SettingsStore::GetSetting(std::string key, std::string defaultValue)
{
    if (!this->ini["Settings"].has(key))
    {
        this->ini["Settings"][key] = defaultValue;
    }

    return this->ini["Settings"][key];
}

/**
 * @brief Parse the list of enabled plugins.
 * INI files don't support arrays, so we need to convert the string to a vector.
 * We use a pipe delimiter to separate the plugins.
 */
std::vector<std::string> SettingsStore::ParsePluginList()
{
    std::vector<std::string> enabledPlugins;
    std::string token;
    std::istringstream tokenStream(this->GetSetting("enabled_plugins", "core"));

    while (std::getline(tokenStream, token, '|'))
    {
        enabledPlugins.push_back(token);
    }

    return enabledPlugins;
}

/** 
 * @brief Convert a vector of strings to a single string.
 * INI files don't support arrays, so we need to convert the vector to a string.
 * We use a pipe delimiter to separate the plugins.
 */
std::string ConvertVectorToString(std::vector<std::string> enabledPlugins)
{
    std::string strEnabledPlugins;
    for (const auto& plugin : enabledPlugins)
    {
        strEnabledPlugins += plugin + "|";
    }

    return strEnabledPlugins.substr(0, strEnabledPlugins.size() - 1);
}

/** 
 * @brief Initialize and optionally fix the settings store.
 */
int SettingsStore::InitializeSettingsStore()
{
    auto enabledPlugins = this->ParsePluginList();

    /** Ensure that the core (Millennium) plugin is enabled */
    if (std::find(enabledPlugins.begin(), enabledPlugins.end(), "core") == enabledPlugins.end())
    {
        enabledPlugins.push_back("core");
    }

    SetSetting("enabled_plugins", ConvertVectorToString(enabledPlugins));
    SetSetting("enabled_plugins", ConvertVectorToString(enabledPlugins));

    GetSetting("check_updates", "true"); // default to true
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
    auto SettingsStore = this->ParsePluginList();

    /** Enable the target plugin */
    if (enabled)
    {
        if (std::find(SettingsStore.begin(), SettingsStore.end(), pluginName) == SettingsStore.end())
        {
            SettingsStore.push_back(pluginName);
        }
    }
    /** Disable the target plugin */
    else if (!enabled)
    {
        if (std::find(SettingsStore.begin(), SettingsStore.end(), pluginName) != SettingsStore.end())
        {
            SettingsStore.erase(std::remove(SettingsStore.begin(), SettingsStore.end(), pluginName), SettingsStore.end());
        }
    }

    SetSetting("enabled_plugins", ConvertVectorToString(SettingsStore));
    return true;
}

/** 
 * @brief Check if a plugin is enabled.
 * @param plugin_name The name of the plugin.
 */
bool SettingsStore::IsEnabledPlugin(std::string plugin_name)
{
    for (const auto& plugin : this->ParsePluginList())
    {
        if (plugin == plugin_name)
        {
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
        { "name", true },
        { "description", false },
        { "common_name", false },
    };

    for (const auto& field : pluginFields)
    {
        auto [property, required] = field;

        if (!json.contains(property))
        {
            if (required)
            {
                throw std::runtime_error(fmt::format("plugin configuration must contain '{}'", property));
            }
            else
            {
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
    
    plugin.pluginJson = json;
    plugin.pluginName = json["name"];
    plugin.pluginBaseDirectory       = entry.path();
    plugin.backendAbsoluteDirectory  = entry.path() / json.value("backend", "backend") / "main.py";
    plugin.frontendAbsoluteDirectory = entry.path() / ".millennium" / "Dist" / "index.js";
    plugin.webkitAbsolutePath        = entry.path() / ".millennium" / "Dist" / "webkit.js";

    return plugin;
}

/**
 * @brief Insert the Millennium modules into the plugin list.
 * As the Millennium modules are internal, they are not stored in the plugins directory and needs to be added manually.
 * 
 * @param plugins The list of plugins to insert the Millennium modules into.
 */
void SettingsStore::InsertMillenniumModules(std::vector<SettingsStore::PluginTypeSchema>& plugins)
{
    const std::filesystem::directory_entry entry(std::filesystem::path(GetEnv("MILLENNIUM__ASSETS_PATH")));
    const auto pluginConfiguration = entry.path() / SettingsStore::pluginConfigFile;

    if (!FileSystem::exists(pluginConfiguration))
    {
        LOG_ERROR("No plugin configuration found in '{}'", entry.path().string());
        return;
    }

    try
    {
        SettingsStore::PluginTypeSchema plugin;
        const auto pluginJson = SystemIO::ReadJsonSync(pluginConfiguration.string());
        const std::string pluginDirName = entry.path().filename().string();

        LintPluginData(pluginJson, pluginDirName);

        plugin.pluginJson = pluginJson;
        plugin.pluginName = pluginJson["name"];
        plugin.pluginBaseDirectory       = entry.path();
        plugin.backendAbsoluteDirectory  = entry.path() / pluginJson.value("backend", "backend") / "main.py";
        plugin.frontendAbsoluteDirectory = std::filesystem::path(GetEnv("MILLENNIUM__ASSETS_PATH")) / ".millennium" / "Dist" / "index.js";
        plugin.webkitAbsolutePath        = std::filesystem::path(GetEnv("MILLENNIUM__ASSETS_PATH")) / ".millennium" / "Dist" / "webkit.js";
        plugin.isInternal = true; /** Internal plugin */

        plugins.push_back(plugin);
    }
    catch (SystemIO::FileException& exception)
    {
        LOG_ERROR("An error occurred reading plugin '{}', exception: {}", entry.path().string(), exception.what());
    }
    catch (std::exception& exception)
    {
        LOG_ERROR("An error occurred parsing plugin '{}', exception: {}", entry.path().string(), exception.what());
    }
}

std::vector<SettingsStore::PluginTypeSchema> SettingsStore::ParseAllPlugins()
{
    std::vector<SettingsStore::PluginTypeSchema> plugins;
    const auto plugin_path = std::filesystem::path(GetEnv("MILLENNIUM__PLUGINS_PATH"));

    try
    {
        /** Create the plugins folder if it doesn't exist. */
        std::filesystem::create_directories(plugin_path);
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR("An error occurred creating plugin directories -> {}", exception.what());
    }

    /** Insert the internal Millennium core plugin */
    this->InsertMillenniumModules(plugins);

    /** Find the remaining plugins from the user plugins folder. */
    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(plugin_path))
        {
            if (!entry.is_directory())
            {
                continue;
            }

            const auto pluginConfiguration = entry.path() / SettingsStore::pluginConfigFile;

            if (!FileSystem::exists(pluginConfiguration))
            {
                continue;
            }

            try
            {
                const auto pluginJson = SystemIO::ReadJsonSync(pluginConfiguration.string());
                const auto pluginData = GetPluginInternalData(pluginJson, entry);

                plugins.push_back(pluginData);
            }
            catch (SystemIO::FileException& exception)
            {
                LOG_ERROR("An error occurred reading plugin '{}', exception: {}", entry.path().string(), exception.what());
            }
            catch (std::exception& exception)
            {
                LOG_ERROR("An error occurred parsing plugin '{}', exception: {}", entry.path().string(), exception.what());
            }
        }
    }
    catch (const std::exception& ex)
    {
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

    for (auto& plugin : allPlugins)
    {
        if (this->IsEnabledPlugin(plugin.pluginName))
        {
            enabledPlugins.push_back(plugin);
        }
    }

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

    for (auto& plugin : allPlugins)
    {
        if (this->IsEnabledPlugin(plugin.pluginName) && plugin.pluginJson.value("useBackend", true))
        {
            enabledBackends.push_back(plugin);
        }
    }

    return enabledBackends;
}