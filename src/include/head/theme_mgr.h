/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
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

#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

class ThemeInstaller
{
  public:
    std::filesystem::path SkinsRoot();

    void RPCLogMessage(const std::string& status, double progress, bool isComplete);
    nlohmann::json ErrorMessage(const std::string& message);
    nlohmann::json SuccessMessage();

    std::optional<nlohmann::json> GetThemeFromGitPair(const std::string& repo, const std::string& owner, bool asString = false);
    bool CheckInstall(const std::string& repo, const std::string& owner);
    nlohmann::json UninstallTheme(const std::string& repo, const std::string& owner);
    nlohmann::json InstallTheme(const std::string& repo, const std::string& owner);

    int CloneWithLibgit2(const std::string& url, const std::filesystem::path& dstPath, std::string& outErr, std::function<void(size_t, size_t, size_t)> progressCallback);
    bool UpdateTheme(const std::string& native);

    std::vector<std::pair<nlohmann::json, std::filesystem::path>> QueryThemesForUpdate();
    nlohmann::json ProcessUpdates(const nlohmann::json& updateQuery, const nlohmann::json& remote);

    nlohmann::json GetRequestBody(void);
    nlohmann::json ConstructPostBody(const std::vector<nlohmann::json>& update_query);

  private:
    /**
     * @brief Attempt to delete a folder, making files writable if necessary
     * @param p Path to the folder to delete
     */
    std::string MakeTempDirName(const std::filesystem::path& base, const std::string& repo);

    /**
     * @brief Check if a directory is a Git repository
     * @param path Directory path to check
     * @return true if directory is a Git repository, false otherwise
     */
    bool IsGitRepo(const std::filesystem::path& path);

    /**
     * @brief Check if theme contains GitHub data
     * @param theme Theme JSON object to check
     * @return true if theme has GitHub data, false otherwise
     */
    bool HasGithubData(const nlohmann::json& theme);

    /**
     * @brief Extract repository name from theme's GitHub data
     * @param theme Theme JSON object
     * @return Repository name as string, empty if not found
     */
    std::string GetRepoName(const nlohmann::json& theme);

    /**
     * @brief Find matching remote theme by repository name
     * @param remote Array of remote themes
     * @param repoName Repository name to search for
     * @return Pointer to matching remote theme, nullptr if not found
     */
    const nlohmann::json* FindRemoteTheme(const nlohmann::json& remote, const std::string& repoName);

    /**
     * @brief Check if local theme has updates available
     * @param path Local theme path
     * @param remoteTheme Remote theme information
     * @return true if updates are available, false otherwise
     */
    bool HasUpdates(const std::filesystem::path& path, const nlohmann::json& remoteTheme);

    /**
     * @brief Create update information object
     * @param theme Local theme data
     * @param remoteTheme Remote theme data
     * @return JSON object containing update information
     */
    nlohmann::json CreateUpdateInfo(const nlohmann::json& theme, const nlohmann::json& remoteTheme);

    /**
     * @brief Get local commit hash for a theme
     * @param path Local theme path
     * @return Commit hash as string
     */
    std::string GetLocalCommitHash(const std::filesystem::path& path);
};