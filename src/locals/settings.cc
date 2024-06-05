#include "locals.h"
#include <fstream>
#include <utilities/log.hpp>
#include <fmt/core.h>
#include <iostream>
#include "base.h"

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

std::vector<SettingsStore::PluginTypeSchema> SettingsStore::ParseAllPlugins()
{
    const auto plugin_path = SystemIO::GetSteamPath() / "steamui" / "plugins";
    std::vector<SettingsStore::PluginTypeSchema> plugins;

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(plugin_path))
        {
            if (!entry.is_directory())
            {
                continue;
            }

            const auto pluginConfiguration = entry.path() / "plugin.json";

            if (!FileSystem::exists(pluginConfiguration))
            {
                continue;
            }

            try
            {
                const auto json = SystemIO::ReadJsonSync(pluginConfiguration.string());

                if (!json.contains("name"))
                {
                    throw std::runtime_error("plugin configuration must contain 'name'");
                }

                PluginTypeSchema plugin;
                plugin.pluginBaseDirectory = entry.path();
                plugin.backendAbsoluteDirectory = entry.path() / "backend" / "main.py";
                plugin.pluginName = (json.contains("name") && json["name"].is_string() ? json["name"].get<std::string>() : entry.path().filename().string());
                plugin.frontendAbsoluteDirectory = fmt::format("plugins/{}/dist/index.js", entry.path().filename().string());
                plugin.pluginJson = json;

                plugins.push_back(plugin);
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
        std::cout << "Error: " << ex.what() << std::endl;
    }

    return plugins;
}