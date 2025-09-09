#pragma once

#include "head/plugin_mgr.h"
#include "head/theme_mgr.h"

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

using json = nlohmann::json;

class Updater
{
  public:
    Updater();

    bool DownloadPluginUpdate(const std::string& id, const std::string& name);
    bool DownloadThemeUpdate(const std::string& native);

    std::optional<json> GetCachedUpdates() const;
    bool HasCheckedForUpdates() const;

    std::optional<json> CheckForUpdates(bool force = false);
    std::string ResyncUpdates();

    ThemeInstaller& GetThemeUpdater();
    PluginInstaller& GetPluginUpdater();

  private:
    std::string api_url;

    ThemeInstaller theme_updater;
    PluginInstaller plugin_updater;

    std::optional<json> cached_updates;
    bool has_checked_for_updates;
};