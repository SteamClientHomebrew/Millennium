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

#include "head/ipc_handler.h"
#include "head/theme_mgr.h"
#include "head/scan.h"

#include "millennium/logger.h"
#include "millennium/sysfs.h"

#include <git2.h>
#include <thread>

std::filesystem::path ThemeInstaller::SkinsRoot()
{
    return std::filesystem::path(SystemIO::GetSteamPath()) / "steamui" / "skins";
}

void ThemeInstaller::RPCLogMessage(const std::string& status, double progress, bool isComplete)
{
    static auto lastSent = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSent).count() < 100)
        return;
    lastSent = now;

    Logger.Log("Installer: {} ({}%)", status, progress);
    IpcForwardInstallLog({ status, progress, isComplete });
}

nlohmann::json ThemeInstaller::ErrorMessage(const std::string& message)
{
    return nlohmann::json({
        { "success", false   },
        { "message", message }
    });
}

nlohmann::json ThemeInstaller::SuccessMessage()
{
    return nlohmann::json({
        { "success", true }
    });
}

std::optional<nlohmann::json> ThemeInstaller::GetThemeFromGitPair(const std::string& repo, const std::string& owner, bool asString)
{
    nlohmann::json themes = Millennium::Themes::FindAllThemes();
    for (auto& theme : themes) {
        auto github = theme.value("data", nlohmann::json::object()).value("github", nlohmann::json::object());
        if (github.value("owner", "") == owner && github.value("repo_name", "") == repo)
            return asString ? nlohmann::json(theme.dump()) : theme;
    }
    return std::nullopt;
}

bool ThemeInstaller::CheckInstall(const std::string& repo, const std::string& owner)
{
    bool installed = GetThemeFromGitPair(repo, owner).has_value();
    Logger.Log("CheckInstall: {}/{} -> {}", owner, repo, installed);
    return installed;
}

nlohmann::json ThemeInstaller::UninstallTheme(const std::string& repo, const std::string& owner)
{
    Logger.Log("UninstallTheme: {}/{}", owner, repo);

    auto themeOpt = GetThemeFromGitPair(repo, owner);
    if (!themeOpt)
        return ErrorMessage("Couldn't locate theme on disk!");

    if (!themeOpt->contains("native"))
        return ErrorMessage("Theme does not have a native path!");

    std::filesystem::path path = SkinsRoot() / themeOpt->value("native", std::string());
    if (!std::filesystem::exists(path))
        return ErrorMessage("Theme path does not exist!");

    if (!SystemIO::DeleteFolder(path))
        return ErrorMessage("Failed to delete theme folder");

    return SuccessMessage();
}

static void SetupRemoteCallbacks(git_remote_callbacks& callbacks)
{
    git_remote_init_callbacks(&callbacks, GIT_REMOTE_CALLBACKS_VERSION);
}

struct CloneProgressData
{
    std::function<void(size_t received, size_t total, size_t indexed)> callback;
};

int TransferProgressCallback(const git_indexer_progress* stats, void* payload)
{
    auto* data = static_cast<CloneProgressData*>(payload);
    if (data && data->callback) {
        data->callback(stats->received_objects, stats->total_objects, stats->indexed_objects);
    }
    return 0;
}

int ThemeInstaller::CloneWithLibgit2(const std::string& url, const std::filesystem::path& dstPath, std::string& outErr,
                                     std::function<void(size_t, size_t, size_t)> progressCallback)
{
    git_libgit2_init();

    git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
    clone_opts.checkout_opts = checkout_opts;

    git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;
    SetupRemoteCallbacks(callbacks);

    CloneProgressData progressData{ progressCallback };
    if (progressCallback) {
        callbacks.transfer_progress = TransferProgressCallback;
        callbacks.payload = &progressData;
    }

    clone_opts.fetch_opts.callbacks = callbacks;

    git_repository* repo = nullptr;
    int ret = git_clone(&repo, url.c_str(), dstPath.string().c_str(), &clone_opts);

    if (ret != 0) {
        outErr = git_error_last() ? git_error_last()->message : "Unknown libgit2 error";
        if (repo)
            git_repository_free(repo);
        git_libgit2_shutdown();
        return ret;
    }

    git_repository_free(repo);
    git_libgit2_shutdown();
    return 0;
}

nlohmann::json ThemeInstaller::InstallTheme(const std::string& repo, const std::string& owner)
{
    std::error_code ec;
    std::filesystem::path finalPath = SkinsRoot() / repo;

    if (repo.empty() || owner.empty()) {
        return ErrorMessage("Repository name and owner cannot be empty");
    }

    if (!std::filesystem::exists(finalPath.parent_path(), ec)) {
        return ErrorMessage("Skins root directory does not exist: " + finalPath.parent_path().string());
    }

    if (std::filesystem::exists(finalPath, ec)) {
        if (!SystemIO::DeleteFolder(finalPath)) {
            return ErrorMessage("Failed to remove existing target path: " + finalPath.string());
        }
    } else if (ec) {
        return ErrorMessage("Failed to check if path exists: " + ec.message());
    }

    std::string tmpName = repo + ".tmp-" + std::to_string(std::hash<std::thread::id>()(std::this_thread::get_id()));
    std::filesystem::path tmpPath = finalPath.parent_path() / tmpName;

    if (std::filesystem::exists(tmpPath, ec)) {
        Logger.Log("Warning: Temporary path already exists, removing: " + tmpPath.string());
        if (!SystemIO::DeleteFolder(tmpPath)) {
            return ErrorMessage("Failed to clean up existing temporary path: " + tmpPath.string());
        }
    }

    RPCLogMessage("Starting Installer...", 10, false);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    RPCLogMessage("Receiving remote objects...", 40, false);

    std::string cloneErr;
    std::string url = fmt::format("https://github.com/{}/{}.git", owner, repo);
    int rc = CloneWithLibgit2(url, tmpPath, cloneErr, [&](size_t received, size_t total, size_t indexed)
    {
        if (total > 0) {
            int percentage = 40 + static_cast<int>((received * 50.0) / total);
            RPCLogMessage("Receiving objects: " + std::to_string(received) + "/" + std::to_string(total), percentage, false);
        }
    });

    if (rc != 0) {
        if (std::filesystem::exists(tmpPath, ec)) {
            if (!SystemIO::DeleteFolder(tmpPath)) {
                Logger.Log("Warning: Failed to clean up temporary path after clone failure: " + tmpPath.string());
            }
        }
        return ErrorMessage("Failed to clone theme repository: " + cloneErr);
    }

    if (!std::filesystem::exists(tmpPath, ec) || ec) {
        return ErrorMessage("Clone completed but temporary directory is missing");
    }

    std::filesystem::rename(tmpPath, finalPath, ec);

    if (ec) {
        Logger.Log("Rename failed (" + ec.message() + "), attempting copy fallback");
        std::filesystem::copy(tmpPath, finalPath, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);

        if (ec) {
            std::error_code cleanupEc;
            if (std::filesystem::exists(tmpPath)) {
                std::filesystem::remove_all(tmpPath, cleanupEc);
            }
            if (std::filesystem::exists(finalPath)) {
                std::filesystem::remove_all(finalPath, cleanupEc);
            }
            return ErrorMessage("Failed to install theme (copy failed): " + ec.message());
        }

        if (!SystemIO::DeleteFolder(tmpPath)) {
            Logger.Log("Warning: Failed to remove temporary directory: " + tmpPath.string());
        }
    }

    if (!std::filesystem::exists(finalPath, ec) || ec) {
        return ErrorMessage("Installation completed but theme directory is not accessible");
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    RPCLogMessage("Done!", 100, true);
    return SuccessMessage();
}

std::vector<std::pair<nlohmann::json, std::filesystem::path>> ThemeInstaller::QueryThemesForUpdate()
{
    std::vector<std::pair<nlohmann::json, std::filesystem::path>> updateQuery;
    nlohmann::json themes = Millennium::Themes::FindAllThemes();
    bool needsCopy = false;

    for (auto& theme : themes) {
        std::filesystem::path path = SkinsRoot() / theme.value("native", "");
        try {
            if (!std::filesystem::exists(path) || !IsGitRepo(path))
                throw std::runtime_error("Not a git repo");

            updateQuery.push_back({ theme, path });
        } catch (...) {
            if (theme["data"].contains("github")) {
                needsCopy = true;
                updateQuery.push_back({ theme, path });
            }
        }
    }

    if (needsCopy) {
        std::filesystem::path src = SkinsRoot();
        std::filesystem::path dst = SkinsRoot().parent_path() / ("skins-backup-" + std::to_string(std::time(nullptr)));
        std::filesystem::copy(src, dst, std::filesystem::copy_options::recursive | std::filesystem::copy_options::skip_symlinks);
    }

    return updateQuery;
}

bool ThemeInstaller::UpdateTheme(const std::string& native)
{
    Logger.Log("Updating theme " + native);
    std::filesystem::path path = SkinsRoot() / native;

    try {
        if (git_libgit2_init() < 0) {
            Logger.Log("Failed to initialize libgit2");
            return false;
        }

        git_repository* repo = nullptr;
        if (git_repository_open(&repo, path.string().c_str()) != 0) {
            Logger.Log("Failed to open Git repository: {}", path.string());
            git_libgit2_shutdown();
            return false;
        }

        git_remote* remote = nullptr;
        if (git_remote_lookup(&remote, repo, "origin") != 0) {
            git_repository_free(repo);
            Logger.Log("Failed to get remote 'origin' for repo: {}", path.string());
            git_libgit2_shutdown();
            return false;
        }

        git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
        git_remote_fetch(remote, nullptr, &fetch_opts, nullptr);

        git_object* obj = nullptr;
        if (git_revparse_single(&obj, repo, "refs/remotes/origin/HEAD") != 0) {
            git_remote_free(remote);
            git_repository_free(repo);
            Logger.Log("Failed to parse HEAD in repo: {}", path.string());
            git_libgit2_shutdown();
            return false;
        }

        if (git_reset(repo, obj, GIT_RESET_HARD, nullptr) != 0) {
            git_object_free(obj);
            git_remote_free(remote);
            git_repository_free(repo);
            Logger.Log("Failed to reset repository to origin/HEAD: {}", path.string());
            git_libgit2_shutdown();
            return false;
        }

        git_object_free(obj);
        git_remote_free(remote);
        git_repository_free(repo);

        // Shut down libgit2
        git_libgit2_shutdown();
    } catch (const std::exception& e) {
        Logger.Log("An exception occurred: {}", e.what());
        git_libgit2_shutdown();
        return false;
    }

    Logger.Log("Theme {} updated successfully.", native);
    return true;
}

nlohmann::json ThemeInstaller::ConstructPostBody(const std::vector<nlohmann::json>& update_query)
{
    nlohmann::json post_body = nlohmann::json::array();

    for (const auto& theme : update_query) {
        if (!theme.contains("data") || !theme["data"].contains("github"))
            continue;

        const auto& github_data = theme["data"]["github"];
        std::string owner = github_data.value("owner", "");
        std::string repo_name = github_data.value("repo_name", "");

        if (!owner.empty() && !repo_name.empty()) {
            post_body.push_back({
                { "owner", owner     },
                { "repo",  repo_name }
            });
        }
    }

    return post_body;
}

nlohmann::json ThemeInstaller::GetRequestBody(void)
{
    nlohmann::json result;

    try {
        auto update_query = QueryThemesForUpdate();
        auto post_body = ConstructPostBody([&update_query]
        {
            std::vector<nlohmann::json> themes;
            for (const auto& pair : update_query) {
                themes.push_back(pair.first);
            }
            return themes;
        }());

        if (update_query.empty()) {
            Logger.Log("No themes to update!");
            return {
                { "update_query", nullptr },
                { "post_body",    nullptr }
            };
        }

        result["update_query"] = update_query;
        result["post_body"] = post_body;
    } catch (const std::exception& e) {
        Logger.Log(std::string("Exception in GetRequestBody: ") + e.what());
        result["update_query"] = nullptr;
        result["post_body"] = nullptr;
    }

    return result;
}

nlohmann::json ThemeInstaller::ProcessUpdates(const nlohmann::json& updateQuery, const nlohmann::json& remote)
{
    nlohmann::json updatedThemes = nlohmann::json::array();

    for (const auto& updateItem : updateQuery) {
        const std::filesystem::path path = updateItem[1];
        const nlohmann::json theme = updateItem[0];

        if (!HasGithubData(theme)) {
            continue;
        }

        const std::string repoName = GetRepoName(theme);
        const auto remoteTheme = FindRemoteTheme(remote, repoName);

        if (remoteTheme != nullptr && HasUpdates(path, *remoteTheme)) {
            updatedThemes.push_back(CreateUpdateInfo(theme, *remoteTheme));
        }
    }

    return updatedThemes;
}

bool ThemeInstaller::HasGithubData(const nlohmann::json& theme)
{
    return theme["data"].contains("github");
}

std::string ThemeInstaller::GetRepoName(const nlohmann::json& theme)
{
    const nlohmann::json& githubData = theme["data"]["github"];
    return githubData.value("repo_name", "");
}

const nlohmann::json* ThemeInstaller::FindRemoteTheme(const nlohmann::json& remote, const std::string& repoName)
{
    if (!remote.is_array()) {
        return nullptr;
    }

    auto it = std::find_if(remote.begin(), remote.end(), [&repoName](const nlohmann::json& item) { return item.value("name", "") == repoName; });

    return (it != remote.end()) ? &(*it) : nullptr;
}

bool ThemeInstaller::HasUpdates(const std::filesystem::path& path, const nlohmann::json& remoteTheme)
{
    const std::string remoteCommit = remoteTheme.value("commit", "");
    const std::string localCommit = GetLocalCommitHash(path);
    return localCommit != remoteCommit;
}

nlohmann::json ThemeInstaller::CreateUpdateInfo(const nlohmann::json& theme, const nlohmann::json& remoteTheme)
{
    return nlohmann::json{
        { "message", remoteTheme.value("message", "No commit message.") },
        { "date", remoteTheme.value("date", "unknown") },
        { "commit", remoteTheme.value("url", "") },
        { "native", theme["native"] },
        { "name", theme["data"].value("name", theme["native"]) }
    };
}

std::string ThemeInstaller::GetLocalCommitHash(const std::filesystem::path& repoPath)
{
    git_libgit2_init();
    git_repository* repo = nullptr;
    if (git_repository_open(&repo, repoPath.string().c_str()) != 0) {
        git_libgit2_shutdown();
        return "";
    }

    git_object* obj = nullptr;
    if (git_revparse_single(&obj, repo, "HEAD") != 0) {
        git_repository_free(repo);
        git_libgit2_shutdown();
        return "";
    }

    const git_oid* oid = git_object_id(obj);
    char hash[GIT_OID_HEXSZ + 1];
    git_oid_tostr(hash, sizeof(hash), oid);

    git_object_free(obj);
    git_repository_free(repo);
    git_libgit2_shutdown();

    return std::string(hash);
}

bool ThemeInstaller::IsGitRepo(const std::filesystem::path& path)
{
    git_libgit2_init();
    git_repository* repo = nullptr;
    int ret = git_repository_open(&repo, path.string().c_str());
    if (ret == 0) {
        git_repository_free(repo);
        git_libgit2_shutdown();
        return true;
    }
    git_libgit2_shutdown();
    return false;
}
