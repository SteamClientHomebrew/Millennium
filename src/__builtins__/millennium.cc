/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
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

#include "__builtins__/config.h"
#include "mhttp.h"
#include <filesystem>
#include <fstream>
#include <internal_logger.h>
#include <iostream>
#include <locals.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>
#include <version.h>

class MillenniumUpdater
{
  public:
    static constexpr const char* GITHUB_API_URL = "https://api.github.com/repos/SteamClientHomebrew/Millennium/releases";

    static std::string ParseVersion(const std::string& version)
    {
        if (!version.empty() && version[0] == 'v')
            return version.substr(1);
        return version;
    }

    static void CheckForUpdates()
    {
        auto& config = CONFIG;
        if (!config.Get("general.checkForMillenniumUpdates", true).get<bool>())
        {
            Logger.Warn("User has disabled update checking for Millennium.");
            return;
        }

        Logger.Log("Checking for updates...");
        try
        {
            std::string response = Http::Get(GITHUB_API_URL, false);

            auto response_data = nlohmann::json::parse(response);
            if (!response_data.is_array() || response_data.empty())
            {
                LOG_ERROR("Invalid response from GitHub API: expected non-empty array.");
                return;
            }

            nlohmann::json latest_release;
            for (auto& release : response_data)
            {
                if (!release.is_object())
                {
                    Logger.Warn("Skipping invalid release data in API response.");
                    continue;
                }
                if (!release.value("prerelease", true))
                {
                    latest_release = release;
                    break;
                }
            }

            if (latest_release.empty())
            {
                Logger.Warn("No stable releases found in GitHub API response.");
                return;
            }

            if (!latest_release.contains("tag_name"))
            {
                LOG_ERROR("Latest release missing required 'tag_name' field.");
                return;
            }

            {
                std::lock_guard<std::mutex> lock(_mutex);
                __latest_version = latest_release;
            }

            std::string current_version;
            try
            {
                current_version = ParseVersion(MILLENNIUM_VERSION);
            }
            catch (...)
            {
                LOG_ERROR("Failed to parse current Millennium version.");
                return;
            }

            std::string latest_version;
            try
            {
                latest_version = ParseVersion(latest_release["tag_name"].get<std::string>());
            }
            catch (...)
            {
                LOG_ERROR("Failed to parse latest release version.");
                return;
            }

            // Simple semver comparison
            int compare = SemverCompare(current_version, latest_version);
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (compare == -1)
                {
                    __has_updates = true;
                    Logger.Log("New version available: " + latest_release["tag_name"].get<std::string>());
                }
                else if (compare == 1)
                {
                    Logger.Warn("Millennium is ahead of the latest release.");
                }
                else
                {
                    Logger.Log("Millennium is up to date.");
                }
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Unexpected error while checking for updates: {}", e.what());
        }
    }

    static std::optional<nlohmann::json> FindAsset(const nlohmann::json& release_info)
    {
        if (!release_info.contains("assets"))
            return std::nullopt;

        std::string platform_suffix;
#ifdef _WIN32
        platform_suffix = "windows-x86_64.zip";
#elif __linux__
        platform_suffix = "linux-x86_64.tar.gz";
#else
        Logger::Error("Invalid platform, cannot find platform-specific assets.");
        return std::nullopt;
#endif

        std::string target_name = "millennium-" + release_info.value("tag_name", "") + "-" + platform_suffix;
        for (auto& asset : release_info["assets"])
        {
            if (asset.value("name", "") == target_name)
                return asset;
        }
        return std::nullopt;
    }

    static bool IsUpdateInProgress()
    {
        std::filesystem::path flag_path = std::filesystem::path(std::getenv("MILLENNIUM__CONFIG_PATH")) / "ext" / "update.flag";
        return std::filesystem::exists(flag_path) && std::filesystem::file_size(flag_path) > 0;
    }

    static nlohmann::json HasAnyUpdates()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        nlohmann::json result;
        result["updateInProgress"] = IsUpdateInProgress();
        result["hasUpdate"] = __has_updates;
        result["newVersion"] = __latest_version;

        auto asset = __latest_version.is_null() ? std::nullopt : FindAsset(__latest_version);
        result["platformRelease"] = asset.has_value() ? asset.value() : nullptr;
        return result;
    }

    static void QueueUpdate(const std::string& downloadUrl)
    {
        std::filesystem::path flag_path = std::filesystem::path(SystemIO::GetInstallPath()) / "ext" / "update.flag";
        std::ofstream file(flag_path);
        if (file.is_open())
        {
            file << downloadUrl;
            Logger.Log("Update queued.");
        }
        else
        {
            LOG_ERROR("Failed to create update.flag file.");
        }
    }

  private:
    static inline bool __has_updates = false;
    static inline nlohmann::json __latest_version;
    static inline std::mutex _mutex;

    // Simple semantic version comparison: returns -1 if v1 < v2, 0 if equal, 1 if v1 > v2
    static int SemverCompare(const std::string& v1, const std::string& v2)
    {
        auto split = [](const std::string& s) -> std::vector<int>
        {
            std::vector<int> parts;
            size_t start = 0;
            size_t end;
            while ((end = s.find('.', start)) != std::string::npos)
            {
                parts.push_back(std::stoi(s.substr(start, end - start)));
                start = end + 1;
            }
            parts.push_back(std::stoi(s.substr(start)));
            return parts;
        };

        std::vector<int> a = split(v1);
        std::vector<int> b = split(v2);
        size_t n = max(a.size(), b.size());
        a.resize(n);
        b.resize(n, 0);

        for (size_t i = 0; i < n; ++i)
        {
            if (a[i] < b[i])
                return -1;
            if (a[i] > b[i])
                return 1;
        }
        return 0;
    }
};
