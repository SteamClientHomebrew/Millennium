#pragma once
#define MINI_CASE_SENSITIVE
#include <string>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>
#include <mini/ini.h>

class SettingsStore
{
public:
    mINI::INIFile file;
    mINI::INIStructure ini;

    static constexpr const char* pluginConfigFile = "plugin.json";

    struct PluginTypeSchema
    {
        std::string pluginName;
        nlohmann::basic_json<> pluginJson;
        std::filesystem::path pluginBaseDirectory;
        std::filesystem::path backendAbsoluteDirectory;
        std::filesystem::path frontendAbsoluteDirectory;
        std::filesystem::path webkitAbsolutePath;
        bool isInternal = false;
    };

    std::vector<std::string> GetEnabledPlugins();
    std::vector<PluginTypeSchema> ParseAllPlugins();
    std::vector<PluginTypeSchema> GetEnabledBackends();

    bool IsEnabledPlugin(std::string pluginName);
    bool TogglePluginStatus(std::string pluginName, bool enabled);

    std::string GetSetting(std::string key, std::string defaultValue);
    void SetSetting(std::string key, std::string settingsData);

    int InitializeSettingsStore();
    std::vector<std::string> ParsePluginList();

    SettingsStore();

private:
    void LintPluginData(nlohmann::json json, std::string pluginName);
    PluginTypeSchema GetPluginInternalData(nlohmann::json json, std::filesystem::directory_entry entry);
    void InsertMillenniumModules(std::vector<SettingsStore::PluginTypeSchema>& plugins);
};

namespace SystemIO
{
    class FileException : public std::exception {
    public:
        FileException(const std::string& message) : msg(message) {}
        virtual const char* what() const noexcept override {
            return msg.c_str();
        }
    private:
        std::string msg;
    };

    std::filesystem::path GetSteamPath();
    std::filesystem::path GetInstallPath();
    nlohmann::json ReadJsonSync(const std::string& filename, bool* success = nullptr);
    std::string ReadFileSync(const std::string& filename);
    void WriteFileSync(const std::filesystem::path& filePath, std::string content);
    void WriteFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent);
}