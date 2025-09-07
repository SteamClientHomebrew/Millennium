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

using nlohmann::json;
namespace fs = std::filesystem;

class ThemeInstaller
{
  public:
    fs::path SkinsRoot();

    void EmitMessage(const std::string& status, int progress, bool isComplete);
    nlohmann::json ErrorMessage(const std::string& message);
    nlohmann::json SuccessMessage();

    void MakeWritable(const fs::path& p);
    bool DeleteFolder(const fs::path& p);

    std::optional<json> GetThemeFromGitPair(const std::string& repo, const std::string& owner, bool asString = false);
    bool CheckInstall(const std::string& repo, const std::string& owner);
    nlohmann::json UninstallTheme(const std::string& repo, const std::string& owner);
    std::string InstallTheme(const std::string& repo, const std::string& owner);

    int CloneWithLibgit2(const std::string& url, const fs::path& dstPath, std::string& outErr);
    bool UpdateTheme(const std::string& native);

    std::vector<std::pair<json, fs::path>> QueryThemesForUpdate();
    json ProcessUpdates(const nlohmann::json& updateQuery, const json& remote);

    nlohmann::json GetRequestBody(void);
    nlohmann::json ConstructPostBody(const std::vector<nlohmann::json>& update_query);

  private:
    std::string MakeTempDirName(const fs::path& base, const std::string& repo);
    bool IsGitRepo(const fs::path& path);
    std::string GetLocalCommitHash(const fs::path& path);
};