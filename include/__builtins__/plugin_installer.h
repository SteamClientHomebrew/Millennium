#pragma once
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

class PluginInstaller
{
  public:
    bool CheckInstall(const std::string& pluginName);
    bool UninstallPlugin(const std::string& pluginName);
    bool InstallPlugin(const std::string& downloadUrl, size_t totalSize);
    bool DownloadPluginUpdate(const std::string& id, const std::string& name);
    json GetRequestBody();

  private:
    void EmitMessage(const std::string& status, double progress, bool isComplete);
    void DownloadWithProgress(const std::string& url, const fs::path& destPath, std::function<void(size_t, size_t)> progressCallback);
    void ExtractZipWithProgress(const fs::path& zipPath, const fs::path& extractTo);
    fs::path PluginsPath();

    std::optional<json> ReadMetadata(const fs::path& pluginPath);
    std::vector<json> GetPluginData();
};
