#include "stream_parser.h"
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

            const auto skin_json = entry.path() / "plugin.json";

            if (!FileSystem::exists(skin_json)) 
            {
                continue;
            }

            try {
                const auto json = SystemIO::ReadJsonSync(skin_json.string());

                PluginTypeSchema plugin;
                plugin.pluginBaseDirectory = entry.path();
                plugin.backendAbsoluteDirectory = entry.path() / "backend" / "main.py";
                plugin.pluginName = (json.contains("name") && json["name"].is_string() ? json["name"].get<std::string>() : entry.path().filename().string());
                plugin.frontendAbsoluteDirectory = fmt::format("plugins/{}/dist/index.js", entry.path().filename().string());
                plugin.pluginJson = json;

                plugins.push_back(plugin);
            }
            catch (SystemIO::FileException& ex) {
                Logger.Error(ex.what());
            }
        }
    }
    catch (const std::exception& ex) {
        std::cout << "Error: " << ex.what() << std::endl;
    }

    return plugins;
}

namespace SystemIO {

    std::filesystem::path GetSteamPath() {
#ifdef _WIN32
        char buffer[MAX_PATH];
        DWORD bufferSize = GetEnvironmentVariableA("SteamPath", buffer, MAX_PATH);

        const std::string path = std::string(buffer, bufferSize);
        return path.empty() ? "C:/Program Files (x86)/Steam" : path;
#elif __linux__
        return fmt::format("{}/.steam/steam", std::getenv("HOME"));
#endif
    }

    nlohmann::json ReadJsonSync(const std::string& filename, bool* success)
    {
        std::ifstream outputLogStream(filename);

        if (!outputLogStream.is_open())
        {
            if (success != nullptr)
                *success = false;
        }

        std::string fileContent((std::istreambuf_iterator<char>(outputLogStream)), std::istreambuf_iterator<char>());

        if (nlohmann::json::accept(fileContent)) {
            if (success != nullptr) *success = true;
            return nlohmann::json::parse(fileContent);
        }
        else {
            Logger.Error("error reading [{}]", filename);
            if (success != nullptr) *success = false;
            return {};
        }
    }

    std::string ReadFileSync(const std::string& filename)
    {
        std::ifstream outputLogStream(filename);
        if (!outputLogStream.is_open())
        {
            Logger.Error("failed to open file [readJsonSync]");
            return {};
        }

        std::string fileContent((std::istreambuf_iterator<char>(outputLogStream)), std::istreambuf_iterator<char>());
        return fileContent;
    }

    void WriteFileSync(const std::filesystem::path& filePath, std::string content)
    {
        std::ofstream outFile(filePath);

        if (outFile.is_open()) {
            outFile << content;
            outFile.close();
        }
    }

    void WriteFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent)
    {
        Logger.Log(fmt::format("writing file to: {}", filePath.string()));

        std::ofstream fileStream(filePath, std::ios::binary);
        if (!fileStream)
        {
            Logger.Log(fmt::format("Failed to open file for writing: {}", filePath.string()));
            return;
        }

        fileStream.write(reinterpret_cast<const char*>(fileContent.data()), fileContent.size());

        if (!fileStream)
        {
            Logger.Log(fmt::format("Error writing to file: {}", filePath.string()));
        }

        fileStream.close();
    }
}