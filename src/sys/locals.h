#pragma once
#include <string>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>

class SettingsStore
{
public:
    static constexpr const char* pluginConfigFile = "plugin.json";

    struct PluginTypeSchema
    {
        std::string pluginName;
        nlohmann::basic_json<> pluginJson;
        std::filesystem::path pluginBaseDirectory;
        std::filesystem::path backendAbsoluteDirectory;
        std::filesystem::path frontendAbsoluteDirectory;
    };

    std::vector<std::string> GetEnabledPlugins();
    std::vector<PluginTypeSchema> ParseAllPlugins();

    bool IsEnabledPlugin(std::string pluginName);
    bool TogglePluginStatus(std::string pluginName, bool enabled);

    nlohmann::json GetSettings();

    void SetSettings(std::string settingsData);
    int InitializeSettingsStore();

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
    nlohmann::json ReadJsonSync(const std::string& filename, bool* success = nullptr);
    std::string ReadFileSync(const std::string& filename);
    void WriteFileSync(const std::filesystem::path& filePath, std::string content);
    void WriteFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent);
}