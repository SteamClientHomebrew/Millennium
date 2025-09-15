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
    bool DownloadPluginUpdate(const std::string& id, const std::string& name);
    nlohmann::json InstallPlugin(const std::string& downloadUrl, size_t totalSize);
    nlohmann::json GetRequestBody();

  private:
    void RPCLogMessage(const std::string& status, double progress, bool isComplete);
    std::filesystem::path PluginsPath();

    std::optional<nlohmann::json> ReadMetadata(const std::filesystem::path& pluginPath);
    std::vector<nlohmann::json> GetPluginData();
};
