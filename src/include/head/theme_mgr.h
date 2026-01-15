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

#include "head/theme_cfg.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class ThemeInstaller
{
  public:
    static std::filesystem::path SkinsRoot();

    static void RPCLogMessage(const std::string& status, double progress, bool isComplete);
    static nlohmann::json ErrorMessage(const std::string& message);
    static nlohmann::json SuccessMessage();

    static std::optional<nlohmann::json> GetThemeFromGitPair(const std::string& repo, const std::string& owner, bool asString = false);
    static bool CheckInstall(const std::string& repo, const std::string& owner);
    static nlohmann::json UninstallTheme(std::shared_ptr<ThemeConfig> themeConfig, const std::string& repo, const std::string& owner);
    static nlohmann::json InstallTheme(std::shared_ptr<ThemeConfig> themeConfig, const std::string& repo, const std::string& owner);

    static int CloneWithLibgit2(const std::string& url, const std::filesystem::path& dstPath, std::string& outErr, std::function<void(size_t, size_t, size_t)> progressCallback);
    static bool UpdateTheme(std::shared_ptr<ThemeConfig> themeConfig, const std::string& native);

    static std::vector<std::pair<nlohmann::json, std::filesystem::path>> QueryThemesForUpdate();
    static nlohmann::json ProcessUpdates(const nlohmann::json& updateQuery, const nlohmann::json& remote);

    static nlohmann::json GetRequestBody();
    static nlohmann::json ConstructPostBody(const std::vector<nlohmann::json>& update_query);

  private:
    std::string MakeTempDirName(const std::filesystem::path& base, const std::string& repo);

    /**
     * @brief Check if a directory is a Git repository
     * @param path Directory path to check
     * @return true if directory is a Git repository, false otherwise
     */
    static bool IsGitRepo(const std::filesystem::path& path);

    /**
     * @brief Check if theme contains GitHub data
     * @param theme Theme JSON object to check
     * @return true if theme has GitHub data, false otherwise
     */
    static bool HasGithubData(const nlohmann::json& theme);

    /**
     * @brief Extract repository name from theme's GitHub data
     * @param theme Theme JSON object
     * @return Repository name as string, empty if not found
     */
    static std::string GetRepoName(const nlohmann::json& theme);

    /**
     * @brief Find matching remote theme by repository name
     * @param remote Array of remote themes
     * @param repoName Repository name to search for
     * @return Pointer to matching remote theme, nullptr if not found
     */
    static const nlohmann::json* FindRemoteTheme(const nlohmann::json& remote, const std::string& repoName);

    /**
     * @brief Check if local theme has updates available
     * @param path Local theme path
     * @param remoteTheme Remote theme information
     * @return true if updates are available, false otherwise
     */
    static bool HasUpdates(const std::filesystem::path& path, const nlohmann::json& remoteTheme);

    /**
     * @brief Create update information object
     * @param theme Local theme data
     * @param remoteTheme Remote theme data
     * @return JSON object containing update information
     */
    static nlohmann::json CreateUpdateInfo(const nlohmann::json& theme, const nlohmann::json& remoteTheme);

    /**
     * @brief Get local commit hash for a theme
     * @param path Local theme path
     * @return Commit hash as string
     */
    static std::string GetLocalCommitHash(const std::filesystem::path& path);
};
