/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2023 - 2026. Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once
#include <mutex>
#include <optional>
#include <string>
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
    static void StartUpdate(const std::string& downloadUrl, size_t downloadSize, bool background, bool forwardToIpc = true);

    /**
     * Check if there is a pending Millennium update that requires a restart
     * @return true if a restart is pending, false otherwise
     */
    static bool HasPendingRestart();

    /**
     * Since NTFS (Windows file system) locks files that are in use, we need to move the old files
     * to a temporary location and schedule them for deletion on next reboot.
     *
     * This frees up the files so that the new version can be copied in place.
     */
    static void DeleteOldMillenniumVersion(std::vector<std::string> lockedFiles);

    /**
     * Cleanup temporary files created by the Millennium Updater
     * This is ran on startup to ensure no leftover files remain from the previous update
     */
    static void CleanupMillenniumUpdaterTempFiles();

    /**
     * Coroutine entry point for performing the update in the background or foreground
     * @param downloadUrl URL of the update to download
     * @param downloadSize Size of the update to download
     * @param forwardToIpc Whether to forward IPC messages to the main thread
     */
    static void Co_BeginUpdate(const std::string& downloadUrl, size_t downloadSize, bool forwardToIpc);

    /**
     * Shutdown the updater, waiting for any background threads to complete
     */
    static void Shutdown();

    static void UpdateLegacyUser32Shim();

  private:
    static inline bool __has_updates = false;
    static inline nlohmann::json __latest_version;
    static inline std::mutex _mutex;
};
