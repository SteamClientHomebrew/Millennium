#include "__builtins__/theme_installer.h"
#include "__builtins__/scan.h"
#include <chrono>
#include <ffi.h>
#include <fstream>
#include <git2.h>
#include <internal_logger.h>
#include <locals.h>
#include <nlohmann/json.hpp>
#include <system_error>
#include <thread>

fs::path ThemeInstaller::SkinsRoot()
{
    return fs::path(SystemIO::GetSteamPath()) / "steamui" / "skins";
}

void ThemeInstaller::EmitMessage(const std::string& status, int progress, bool isComplete)
{
    Logger.Log("emitting message " + status + " " + std::to_string(progress) + " " + (isComplete ? "true" : "false"));
    json j = {
        { "status",     status     },
        { "progress",   progress   },
        { "isComplete", isComplete }
    };
    Logger.Log("emitting message " + j.dump());
}

nlohmann::json ThemeInstaller::ErrorMessage(const std::string& message)
{
    return json({
        { "success", false   },
        { "message", message }
    });
}

nlohmann::json ThemeInstaller::SuccessMessage()
{
    return json({
        { "success", true }
    });
}

void ThemeInstaller::MakeWritable(const fs::path& p)
{
    std::error_code ec;
    auto perms = fs::status(p, ec).permissions();
    if (!ec)
        fs::permissions(p, perms | fs::perms::owner_write, ec);
}

bool ThemeInstaller::DeleteFolder(const fs::path& p)
{
    if (!fs::exists(p))
        return true;

    std::error_code ec;
    fs::remove_all(p, ec);
    if (!ec)
        return true;

    Logger.Log("DeleteFolder failed initially: {} — retrying with writable perms.", ec.message());
    for (auto it = fs::recursive_directory_iterator(p, fs::directory_options::skip_permission_denied, ec); it != fs::recursive_directory_iterator(); ++it)
        MakeWritable(it->path());

    ec.clear();
    fs::remove_all(p, ec);
    if (ec) {
        LOG_ERROR("Failed to delete folder {}: {}", p.string(), ec.message());
        return false;
    }
    return true;
}

std::optional<json> ThemeInstaller::GetThemeFromGitPair(const std::string& repo, const std::string& owner, bool asString)
{
    json themes = Millennium::Themes::FindAllThemes();
    for (auto& theme : themes) {
        auto github = theme.value("data", json::object()).value("github", json::object());
        if (github.value("owner", "") == owner && github.value("repo_name", "") == repo)
            return asString ? json(theme.dump()) : theme;
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
    Logger.Log("UninstallTheme: " + owner + "/" + repo);
    auto themeOpt = GetThemeFromGitPair(repo, owner);
    if (!themeOpt)
        return ErrorMessage("Couldn't locate theme on disk!");

    fs::path path = SkinsRoot() / themeOpt->value("native", "");
    if (!fs::exists(path))
        return ErrorMessage("Theme path does not exist!");

    if (!DeleteFolder(path))
        return ErrorMessage("Failed to delete theme folder");

    return SuccessMessage();
}

static void SetupRemoteCallbacks(git_remote_callbacks& callbacks)
{
    git_remote_init_callbacks(&callbacks, GIT_REMOTE_CALLBACKS_VERSION);
}

int ThemeInstaller::CloneWithLibgit2(const std::string& url, const fs::path& dstPath, std::string& outErr)
{
    git_libgit2_init();
    git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
    clone_opts.checkout_opts = checkout_opts;

    git_remote_callbacks callbacks{};
    SetupRemoteCallbacks(callbacks);
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

std::string ThemeInstaller::InstallTheme(const std::string& repo, const std::string& owner)
{
    fs::path finalPath = SkinsRoot() / repo;
    std::error_code ec;

    // Remove existing folder
    if (fs::exists(finalPath)) {
        if (!DeleteFolder(finalPath))
            return ErrorMessage("Failed to remove existing target path");
    }

    std::string tmpName = repo + ".tmp-" + std::to_string(std::hash<std::thread::id>()(std::this_thread::get_id()));
    fs::path tmpPath = finalPath.parent_path() / tmpName;

    EmitMessage("Starting Installer...", 10, false);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EmitMessage("Receiving remote objects...", 40, false);

    std::string cloneErr;
    std::string url = fmt::format("https://github.com/{}/{}.git", owner, repo);
    int rc = CloneWithLibgit2(url, tmpPath, cloneErr);

    if (rc != 0) {
        fs::remove_all(tmpPath, ec);
        return ErrorMessage("Failed to clone theme repository: " + cloneErr);
    }

    fs::rename(tmpPath, finalPath, ec);
    if (ec) {
        Logger.Log("Rename failed, fallback to copy: " + ec.message());
        fs::copy(tmpPath, finalPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        DeleteFolder(tmpPath);
    }

    EmitMessage("Done!", 100, true);
    return SuccessMessage();
}

std::vector<std::pair<json, fs::path>> ThemeInstaller::QueryThemesForUpdate()
{
    std::vector<std::pair<json, fs::path>> updateQuery;
    json themes = Millennium::Themes::FindAllThemes();
    bool needsCopy = false;

    for (auto& theme : themes) {
        fs::path path = SkinsRoot() / theme.value("native", "");
        try {
            if (!fs::exists(path) || !IsGitRepo(path))
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
        fs::path src = SkinsRoot();
        fs::path dst = SkinsRoot().parent_path() / ("skins-backup-" + std::to_string(std::time(nullptr)));
        fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::skip_symlinks);
    }

    return updateQuery;
}

bool ThemeInstaller::UpdateTheme(const std::string& native)
{
    Logger.Log("Updating theme " + native);
    fs::path path = SkinsRoot() / native;

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

json ThemeInstaller::ProcessUpdates(const nlohmann::json& updateQuery, const json& remote)
{
    json updatedThemes = json::array();

    for (auto updateItem : updateQuery) {
        fs::path path = updateItem[1];
        json theme = updateItem[0];

        if (!theme["data"].contains("github"))
            continue;

        json githubData = theme["data"]["github"];
        std::string repo_name = githubData.value("repo_name", "");
        if (remote.is_array()) {
            auto it = std::find_if(remote.begin(), remote.end(), [&](const json& item) { return item.value("name", "") == repo_name; });
            if (it != remote.end()) {
                std::string remoteCommit = it->value("commit", "");
                std::string localCommit = GetLocalCommitHash(path);

                if (localCommit != remoteCommit) {
                    updatedThemes.push_back({
                        { "message", it->value("message", "No commit message.") },
                        { "date", it->value("date", "unknown") },
                        { "commit", it->value("url", "") },
                        { "native", theme["native"] },
                        { "name", theme["data"].value("name", theme["native"]) }
                    });
                }
            }
        }
    }

    return updatedThemes;
}

std::string ThemeInstaller::GetLocalCommitHash(const fs::path& repoPath)
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

bool ThemeInstaller::IsGitRepo(const fs::path& path)
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
