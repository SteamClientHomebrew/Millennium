#pragma once
#include <string>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>

class SettingsStore
{
public:
    struct PluginTypeSchema
    {
        std::filesystem::path pluginBaseDirectory;
        std::filesystem::path backendAbsoluteDirectory;
        std::string pluginName;
        std::string frontendAbsoluteDirectory;
        nlohmann::basic_json<> pluginJson;
    };

    std::vector<std::string> GetEnabledPlugins();
    bool IsEnabledPlugin(std::string pluginName);

    bool TogglePluginStatus(std::string pluginName, bool enabled);
    std::vector<PluginTypeSchema> ParseAllPlugins();

    void SetSettings(std::string settingsData);
    nlohmann::json GetSettings();

    int InitializeSettingsStore();
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