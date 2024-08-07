#include "locals.h"
#include <fstream>
#include <sys/log.h>
#include <fmt/core.h>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

namespace FileSystem = std::filesystem;

SettingsStore::SettingsStore() : file(mINI::INIFile(std::string())), ini(mINI::INIStructure())
{
    const auto path = SystemIO::GetInstallPath() / "ext" / "millennium.ini";

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

void SettingsStore::SetSetting(std::string key, std::string settingsData)
{
    this->ini["Settings"][key] = settingsData;
    this->file.write(ini);
}

std::string SettingsStore::GetSetting(std::string key, std::string defaultValue)
{
    if (!this->ini["Settings"].has(key))
    {
        this->ini["Settings"][key] = defaultValue;
    }

    return this->ini["Settings"][key];
}

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

std::string ConvertVectorToString(std::vector<std::string> enabledPlugins)
{
    std::string strEnabledPlugins;
    for (const auto& plugin : enabledPlugins)
    {
        strEnabledPlugins += plugin + "|";
    }

    return strEnabledPlugins.substr(0, strEnabledPlugins.size() - 1);
}

int SettingsStore::InitializeSettingsStore()
{
    auto enabledPlugins = this->ParsePluginList();

    // check if core is in the list
    if (std::find(enabledPlugins.begin(), enabledPlugins.end(), "core") == enabledPlugins.end())
    {
        enabledPlugins.push_back("core");
    }

    SetSetting("enabled_plugins", ConvertVectorToString(enabledPlugins));
    SetSetting("enabled_plugins", ConvertVectorToString(enabledPlugins));

    GetSetting("check_updates", "true"); // default to true
    return 0;
}

bool SettingsStore::TogglePluginStatus(std::string pluginName, bool enabled)
{
    Logger.Log("opting to {} {}", enabled ? "enable" : "disable", pluginName);
    auto SettingsStore = this->ParsePluginList();

    if (enabled)
    {
        if (std::find(SettingsStore.begin(), SettingsStore.end(), pluginName) == SettingsStore.end())
        {
            SettingsStore.push_back(pluginName);
        }
    }
    // disable the plugin
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

std::vector<std::string> SettingsStore::GetEnabledPlugins()
{
    return this->ParsePluginList();
}

bool SettingsStore::IsEnabledPlugin(std::string plugin_name)
{
    for (const auto& plugin : SettingsStore::GetEnabledPlugins())
    {
        if (plugin == plugin_name)
        {
            return true;
        }
    }
    return false;
}

void SettingsStore::LintPluginData(nlohmann::json json, std::string pluginName)
{
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

SettingsStore::PluginTypeSchema SettingsStore::GetPluginInternalData(nlohmann::json json, std::filesystem::directory_entry entry)
{
    SettingsStore::PluginTypeSchema plugin;
    const std::string pluginDirName = entry.path().filename().string();

    LintPluginData(json, pluginDirName);
    
    plugin.pluginJson = json;
    plugin.pluginName = json["name"];
    plugin.pluginBaseDirectory      = entry.path();
    plugin.backendAbsoluteDirectory = entry.path() / json.value("backend", "backend") / "main.py";
    plugin.frontendAbsoluteDirectory = (FileSystem::path)pluginDirName / ".millennium" / "Dist" / "index.js";

    return plugin;
}

void SettingsStore::InsertMillenniumModules(std::vector<SettingsStore::PluginTypeSchema>& plugins)
{
    const std::filesystem::directory_entry entry(SystemIO::GetInstallPath() / "ext" / "data" / "assets");
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
        plugin.pluginBaseDirectory      = entry.path();
        plugin.backendAbsoluteDirectory = entry.path() / pluginJson.value("backend", "backend") / "main.py";
        plugin.frontendAbsoluteDirectory = (FileSystem::path)"assets" / ".millennium" / "Dist" / "index.js";
        plugin.isInternal = true;

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
    const auto plugin_path = SystemIO::GetInstallPath() / "plugins";

    try
    {
        std::filesystem::create_directories(plugin_path);
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR("An error occurred creating plugin directories -> {}", exception.what());
    }

    this->InsertMillenniumModules(plugins);

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