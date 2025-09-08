#pragma once
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

class PluginInstaller
{
  public:
    bool CheckInstall(const std::string& pluginName);
    bool UninstallPlugin(const std::string& pluginName);
    bool InstallPlugin(const std::string& downloadUrl, size_t totalSize);
    bool DownloadPluginUpdate(const std::string& id, const std::string& name);
    nlohmann::json GetRequestBody();

  private:
    void EmitMessage(const std::string& status, double progress, bool isComplete);
    void DownloadWithProgress(const std::string& url, const std::filesystem::path& destPath, std::function<void(size_t, size_t)> progressCallback);
    void ExtractZipWithProgress(const std::filesystem::path& zipPath, const std::filesystem::path& extractTo);
    std::filesystem::path PluginsPath();

    std::optional<nlohmann::json> ReadMetadata(const std::filesystem::path& pluginPath);
    std::vector<nlohmann::json> GetPluginData();
};
