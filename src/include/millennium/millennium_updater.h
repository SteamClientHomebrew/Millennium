#pragma once

#include <string>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>

class MillenniumUpdater
{
  public:
    static constexpr const char* GITHUB_API_URL = "https://api.github.com/repos/SteamClientHomebrew/Millennium/releases";

    /**
     * Parse version string, removing leading 'v' if present
     * @param version Version string to parse
     * @return Cleaned version string
     */
    static std::string ParseVersion(const std::string& version);

    /**
     * Check for Millennium updates from GitHub releases
     * Updates internal state with latest version information
     */
    static void CheckForUpdates();

    /**
     * Find platform-specific asset in release information
     * @param release_info JSON object containing release data
     * @return Optional JSON object with asset information, nullopt if not found
     */
    static std::optional<nlohmann::json> FindAsset(const nlohmann::json& release_info);

    /**
     * Get comprehensive update status information
     * @return JSON object with update status, version info, and platform release data
     */
    static nlohmann::json HasAnyUpdates();

    /**
     * Queue an update by writing the download URL to update.flag
     * @param downloadUrl URL of the update to download
     * @param downloadSize Size of the update to download
     * @param background Whether to run the update in the background (separate thread) or foreground (main thread)
     */
    static void StartUpdate(const std::string& downloadUrl, const size_t downloadSize, bool background);

    /**
     * Since NTFS (Windows file system) locks files that are in use, we need to move the old files
     * to a temporary location and schedule them for deletion on next reboot.
     *
     * This frees up the files so that the new version can be copied in place.
     */
    static void DeleteOldMillenniumVersion();

    /**
     * Cleanup temporary files created by the Millennium Updater
     * This is ran on startup to ensure no leftover files remain from the previous update
     */
    static void CleanupMillenniumUpdaterTempFiles();

    /**
     * Coroutine entry point for performing the update in the background or foreground
     * @param downloadUrl URL of the update to download
     * @param downloadSize Size of the update to download
     */
    static void Co_BeginUpdate(const std::string& downloadUrl, const size_t downloadSize);

  private:
    static inline bool __has_updates = false;
    static inline nlohmann::json __latest_version;
    static inline std::mutex _mutex;
};