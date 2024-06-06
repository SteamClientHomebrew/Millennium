#include "locals.h"
#include <fstream>
#include <sys/log.hpp>
#include <fmt/core.h>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

namespace FileSystem = std::filesystem;

void SettingsStore::SetSettings(std::string file_data)
{
    const auto path = SystemIO::GetSteamPath() / ".millennium" / "settings.json";

    if (!FileSystem::exists(path))
    {
        FileSystem::create_directories(path.parent_path());
        std::ofstream outputFile(path.string());
        outputFile << "{}";
    }
    SystemIO::WriteFileSync(path, file_data);
}

nlohmann::json SettingsStore::GetSettings()
{
    const auto path = SystemIO::GetSteamPath() / ".millennium" / "settings.json";

    if (!FileSystem::exists(path))
    {
        FileSystem::create_directories(path.parent_path());
        std::ofstream outputFile(path.string());
        outputFile << "{}";
    }

    bool success;
    const auto json = SystemIO::ReadJsonSync(path.string(), &success);

    if (!success)
    {
        std::ofstream outputFile(path.string());
        outputFile << "{}";
    }

    return success ? json : nlohmann::json({});
}

int SettingsStore::InitializeSettingsStore()
{
    auto SettingsStore = this->GetSettings();

    if (SettingsStore.contains("enabled") && SettingsStore["enabled"].is_array())
    {
        bool found = false;

        for (const auto& enabledPlugin : SettingsStore["enabled"])
        {
            if (enabledPlugin == "millennium__internal")
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            SettingsStore["enabled"].push_back("millennium__internal");
        }
    }
    else
    {
        SettingsStore["enabled"] = nlohmann::json::array({ "millennium__internal" });
    }

    SetSettings(SettingsStore.dump(4));
    return true;
}

bool SettingsStore::TogglePluginStatus(std::string pluginName, bool enabled)
{
    Logger.Log("opting to {} {}", enabled ? "enable" : "disable", pluginName);
    auto SettingsStore = this->GetSettings();

    if (enabled)
    {
        if (SettingsStore.contains("enabled") && SettingsStore["enabled"].is_array())
        {
            SettingsStore["enabled"].push_back(pluginName);
        }
        else {
            SettingsStore["enabled"] = nlohmann::json::array({ pluginName });
        }
    }
    // disable the plugin
    else if (!enabled && SettingsStore.contains("enabled") && SettingsStore["enabled"].is_array())
    {
        auto EnabledPlugins = SettingsStore["enabled"].get<std::vector<std::string>>();
        auto Iterator = std::find(EnabledPlugins.begin(), EnabledPlugins.end(), pluginName);

        if (Iterator != EnabledPlugins.end())
        {
            EnabledPlugins.erase(Iterator);
        }

        SettingsStore["enabled"] = EnabledPlugins;
    }

    SetSettings(SettingsStore.dump(4));
    return true;
}

std::vector<std::string> SettingsStore::GetEnabledPlugins()
{
    auto json = this->GetSettings();
    return json["enabled"].get<std::vector<std::string>>();
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
    plugin.backendAbsoluteDirectory = entry.path() / "backend" / "main.py";
    plugin.frontendAbsoluteDirectory = (FileSystem::path) "plugins" / pluginDirName / "dist" / "index.js";

    return plugin;
}

std::vector<SettingsStore::PluginTypeSchema> SettingsStore::ParseAllPlugins()
{
    std::vector<SettingsStore::PluginTypeSchema> plugins;
    const auto plugin_path = SystemIO::GetSteamPath() / "steamui" / "plugins";

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
                Logger.Error("An error occurred reading plugin '{}', exception: {}", entry.path().string(), exception.what());
            }
            catch (std::exception& exception)
            {
                Logger.Error("An error occurred parsing plugin '{}', exception: {}", entry.path().string(), exception.what());
            }
        }
    }
    catch (const std::exception& ex)
    {
        Logger.Error("Fall back exception caught trying to parse plugins. {}", ex.what());
    }

    return plugins;
}