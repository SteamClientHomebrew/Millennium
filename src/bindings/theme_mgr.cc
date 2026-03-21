/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

#include "head/library_updater.h"
#include "head/theme_mgr.h"
#include "head/scan.h"

#include "millennium/logger.h"
#include "millennium/filesystem.h"
#include "millennium/http.h"
#include "millennium/encoding.h"
#include "millennium/zip.h"

#include <fstream>

head::theme_installer::theme_installer(std::shared_ptr<::plugin_manager> plugin_manager, std::shared_ptr<library_updater> updater)
    : m_plugin_manager(std::move(plugin_manager)), m_updater(std::move(updater))
{
    migrate_legacy_themes();
}

std::filesystem::path head::theme_installer::get_themes_folder()
{
    return platform::get_millennium_path() / "themes";
}

nlohmann::json head::theme_installer::create_error_response(const std::string& message)
{
    return nlohmann::json({
        { "success", false   },
        { "message", message }
    });
}

nlohmann::json head::theme_installer::create_successful_response()
{
    return nlohmann::json({
        { "success", true }
    });
}

std::optional<nlohmann::json> head::theme_installer::get_theme_from_github(const std::string& repo, const std::string& owner, [[maybe_unused]] bool asString)
{
    nlohmann::json themes = head::Themes::FindAllThemes();
    for (auto& theme : themes) {
        auto github = theme.value("data", nlohmann::json::object()).value("github", nlohmann::json::object());
        if (github.value("owner", "") == owner && github.value("repo_name", "") == repo) return theme;
    }
    return std::nullopt;
}

bool head::theme_installer::is_theme_installed(const std::string& repo, const std::string& owner)
{
    auto theme = get_theme_from_github(repo, owner);
    if (!theme.has_value()) {
        logger.log("is_theme_installed: {}/{} -> false (not found)", owner, repo);
        return false;
    }

    std::filesystem::path path = get_themes_folder() / theme->value("native", std::string());
    bool installed = std::filesystem::exists(path);
    logger.log("is_theme_installed: {}/{} -> {} (path: {})", owner, repo, installed, path.string());
    return installed;
}

nlohmann::json head::theme_installer::uninstall_theme(std::shared_ptr<theme_config_store> themeConfig, const std::string& repo, const std::string& owner)
{
    logger.log("uninstall_theme: {}/{}", owner, repo);

    auto themeOpt = get_theme_from_github(repo, owner);
    if (!themeOpt) return create_error_response("Couldn't locate theme on disk!");

    if (!themeOpt->contains("native")) return create_error_response("Theme does not have a native path!");

    std::filesystem::path path = get_themes_folder() / themeOpt->value("native", std::string());
    if (!std::filesystem::exists(path)) return create_error_response("Theme path does not exist!");

    if (!platform::remove_directory(path)) return create_error_response("Failed to delete theme folder");

    /** trigger config update to regenerate config */
    themeConfig->on_config_change_hdlr();
    return create_successful_response();
}

bool head::theme_installer::write_metadata(const std::filesystem::path& themePath, const std::string& owner, const std::string& repo, const std::string& commit)
{
    nlohmann::json metadata = {
        { "owner",  owner  },
        { "repo",   repo   },
        { "commit", commit }
    };

    std::ofstream out(themePath / "metadata.json");
    if (!out) {
        LOG_ERROR("Failed to open metadata.json for writing in '{}'", themePath.string());
        return false;
    }

    out << metadata.dump(4);
    out.flush();

    if (!out.good()) {
        LOG_ERROR("Failed to write metadata.json in '{}'", themePath.string());
        return false;
    }

    return true;
}

std::optional<nlohmann::json> head::theme_installer::read_metadata(const std::filesystem::path& themePath)
{
    std::filesystem::path metadataPath = themePath / "metadata.json";
    if (!std::filesystem::exists(metadataPath)) return std::nullopt;

    try {
        std::ifstream in(metadataPath);
        nlohmann::json metadata;
        in >> metadata;

        if (metadata.contains("owner") && metadata.contains("repo") && metadata.contains("commit")) {
            return metadata;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to read metadata.json for theme {}: {}", themePath.filename().string(), e.what());
    }
    return std::nullopt;
}

std::string head::theme_installer::get_commit_hash(const std::filesystem::path& repoPath)
{
    auto metadata = read_metadata(repoPath);
    if (metadata) {
        return metadata->value("commit", "");
    }

    /*
     * migrate legacy git themes by parsing the .git files
     * this only runs once per theme — after migration, metadata.json is used instead.
     */
    std::filesystem::path gitDir = repoPath / ".git";
    if (!std::filesystem::exists(gitDir) || !std::filesystem::is_directory(gitDir)) return "";

    try {
        std::ifstream headFile(gitDir / "HEAD");
        std::string headContent;
        std::getline(headFile, headContent);

        /* strip trailing \r CRLF on windows */
        if (!headContent.empty() && headContent.back() == '\r') {
            headContent.pop_back();
        }

        /* HEAD is either "ref: refs/heads/main" (symbolic) or a raw 40-char hex hash (detached) */
        std::string ref;
        if (headContent.size() > 5 && headContent.substr(0, 5) == "ref: ") {
            ref = headContent.substr(5);
        } else {
            return headContent;
        }

        /* try the loose ref file first: .git/refs/heads/main */
        std::filesystem::path looseRefPath = gitDir / ref;
        if (std::filesystem::exists(looseRefPath)) {
            std::ifstream refFile(looseRefPath);
            std::string commitHash;
            std::getline(refFile, commitHash);
            if (!commitHash.empty() && commitHash.back() == '\r') {
                commitHash.pop_back();
            }
            return commitHash;
        }

        /* loose ref missing, with git gc or git pack-refs */
        std::filesystem::path packedRefsPath = gitDir / "packed-refs";
        if (std::filesystem::exists(packedRefsPath)) {
            std::ifstream packedFile(packedRefsPath);
            std::string line;
            while (std::getline(packedFile, line)) {
                if (line.empty() || line[0] == '#' || line[0] == '^') continue;
                if (!line.empty() && line.back() == '\r') line.pop_back();

                /* fmt -> <40-char hash> <ref path> */
                auto spacePos = line.find(' ');
                if (spacePos != std::string::npos && line.substr(spacePos + 1) == ref) {
                    return line.substr(0, spacePos);
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to read git HEAD for {}: {}", repoPath.filename().string(), e.what());
    }
    return "";
}

void head::theme_installer::migrate_legacy_themes()
{
    try {
        std::filesystem::path themesDir = get_themes_folder();
        if (!std::filesystem::exists(themesDir)) {
            return;
        }

        for (const auto& entry : std::filesystem::directory_iterator(themesDir)) {
            if (!entry.is_directory()) {
                continue;
            }

            std::filesystem::path themePath = entry.path();

            /* skip if already has metadata.json */
            if (std::filesystem::exists(themePath / "metadata.json")) {
                continue;
            }

            /* only migrate if it has .git (was installed via old system) */
            if (!std::filesystem::exists(themePath / ".git")) {
                continue;
            }

            /* read owner/repo from skin.json's github field */
            std::filesystem::path skinPath = themePath / "skin.json";
            if (!std::filesystem::exists(skinPath)) {
                continue;
            }

            try {
                std::ifstream skinFile(skinPath);
                nlohmann::json skinData;
                skinFile >> skinData;

                if (!skinData.contains("github")) continue;

                std::string owner = skinData["github"].value("owner", "");
                std::string repo = skinData["github"].value("repo_name", "");
                if (owner.empty() || repo.empty()) continue;

                std::string commit = get_commit_hash(themePath);
                if (commit.empty()) continue;

                if (!write_metadata(themePath, owner, repo, commit)) {
                    LOG_ERROR("Failed to write metadata during migration of '{}'", themePath.filename().string());
                    continue;
                }
                logger.log("Migrated legacy theme '{}' ({}/{}) with commit {}", themePath.filename().string(), owner, repo, commit);
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to migrate legacy theme '{}': {}", themePath.filename().string(), e.what());
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to migrate legacy themes: {}", e.what());
    }
}

nlohmann::json head::theme_installer::install_theme(std::shared_ptr<theme_config_store> themeConfig, const std::string& repo, const std::string& owner)
{
    std::filesystem::path tempDir;
    try {
        if (repo.empty() || owner.empty()) {
            return create_error_response("Repository name and owner cannot be empty");
        }

        std::filesystem::path finalPath = get_themes_folder() / repo;

        if (!std::filesystem::exists(finalPath.parent_path())) {
            return create_error_response("Themes root directory does not exist: " + finalPath.parent_path().string());
        }

        if (std::filesystem::exists(finalPath)) {
            if (!platform::remove_directory(finalPath)) {
                return create_error_response("Failed to remove existing target path: " + finalPath.string());
            }
        }

        m_updater->dispatch_progress("Starting Installer...", 5, false);

        nlohmann::json postBody = {
            { "owner", owner },
            { "repo",  repo  }
        };
        auto response_str = Http::Post("https://steambrew.app/api/v2/update", postBody.dump());
        auto response = nlohmann::json::parse(response_str);

        std::string downloadUrl = response.value("data", nlohmann::json::object()).value("download", "");
        std::string commitHash = response.value("data", nlohmann::json::object()).value("latestHash", "");

        if (downloadUrl.empty()) {
            return create_error_response("Failed to resolve download URL for theme");
        }

        logger.log("Downloading theme from: {}", downloadUrl);
        m_updater->dispatch_progress("Downloading theme archive...", 10, false);

        tempDir = get_themes_folder() / ("__tmp_" + GenerateUUID());
        std::filesystem::create_directories(tempDir);
        std::filesystem::path zipPath = tempDir / (repo + ".zip");

        Http::DownloadWithProgress({ downloadUrl, 0 }, zipPath, [&](size_t downloaded, size_t total)
        {
            if (total > 0) {
                double percent = (double(downloaded) / total) * 100.0;
                m_updater->dispatch_progress("Downloading theme archive...", 10.0 + (40.0 * (percent / 100.0)), false);
            }
        });

        m_updater->dispatch_progress("Extracting theme archive...", 55, false);
        logger.log("Extracting theme to: {}", tempDir.string());

        const bool extracted = Util::ExtractZipArchive(zipPath.string(), tempDir.string(), [&](int current, int total, const char*)
        {
            double percent = (double(current) / total) * 100.0;
            m_updater->dispatch_progress("Extracting theme archive...", 55.0 + (30.0 * (percent / 100.0)), false);
        });

        if (!extracted) {
            std::filesystem::remove_all(tempDir);
            return create_error_response("Failed to extract theme archive");
        }

        /* GitHub zips contain a single top-level directory named {repo}-{branch} */
        std::filesystem::path extractedFolder;
        for (const auto& entry : std::filesystem::directory_iterator(tempDir)) {
            if (entry.is_directory()) {
                extractedFolder = entry.path();
                break;
            }
        }

        if (extractedFolder.empty()) {
            std::filesystem::remove_all(tempDir);
            return create_error_response("No directory found in extracted theme archive");
        }

        m_updater->dispatch_progress("Installing theme...", 88, false);

        std::error_code ec;
        std::filesystem::rename(extractedFolder, finalPath, ec);

        if (ec) {
            logger.log("Rename failed ({}), attempting copy fallback", ec.message());
            std::filesystem::create_directories(finalPath);
            std::filesystem::copy(extractedFolder, finalPath, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);

            if (ec) {
                std::filesystem::remove_all(tempDir);
                if (std::filesystem::exists(finalPath)) {
                    std::filesystem::remove_all(finalPath);
                }
                return create_error_response("Failed to install theme (copy failed): " + ec.message());
            }
        }

        if (!write_metadata(finalPath, owner, repo, commitHash)) {
            std::filesystem::remove_all(tempDir);
            if (std::filesystem::exists(finalPath)) {
                std::filesystem::remove_all(finalPath);
            }
            return create_error_response("Failed to write metadata after theme install");
        }

        m_updater->dispatch_progress("Cleaning up...", 96, false);
        std::filesystem::remove_all(tempDir);
        tempDir.clear();

        /** trigger config update to regenerate config */
        themeConfig->on_config_change_hdlr();

        m_updater->dispatch_progress("Done!", 100, true);
        return create_successful_response();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to install theme: {}", e.what());
        if (!tempDir.empty() && std::filesystem::exists(tempDir)) {
            std::filesystem::remove_all(tempDir);
        }
        return create_error_response(std::string("Failed to install theme: ") + e.what());
    }
}

bool head::theme_installer::update_theme(std::shared_ptr<theme_config_store> themeConfig, const std::string& native)
{
    std::filesystem::path tempDir;
    try {
        std::filesystem::path themePath = get_themes_folder() / native;
        logger.log("Updating theme '{}'", native);

        auto metadata = read_metadata(themePath);
        if (!metadata) {
            LOG_ERROR("Cannot update theme '{}': no metadata.json found", native);
            return false;
        }

        std::string owner = metadata->value("owner", "");
        std::string repo = metadata->value("repo", "");
        if (owner.empty() || repo.empty()) {
            LOG_ERROR("Cannot update theme '{}': metadata.json missing owner/repo", native);
            return false;
        }

        nlohmann::json postBody = {
            { "owner", owner },
            { "repo",  repo  }
        };
        auto response_str = Http::Post("https://steambrew.app/api/v2/update", postBody.dump());
        auto response = nlohmann::json::parse(response_str);

        std::string downloadUrl = response.value("data", nlohmann::json::object()).value("download", "");
        std::string latestCommit = response.value("data", nlohmann::json::object()).value("latestHash", "");

        logger.log("[update-download] theme='{}' latestHash='{}' downloadUrl='{}'", native, latestCommit, downloadUrl);

        if (latestCommit.empty()) {
            logger.warn("[update-download] theme='{}': API returned empty latestHash — metadata will be written with blank commit", native);
        }

        if (downloadUrl.empty()) {
            LOG_ERROR("Cannot update theme '{}': API did not return a download URL", native);
            return false;
        }

        logger.log("Downloading theme update from: {}", downloadUrl);
        m_updater->dispatch_progress("Downloading theme update...", 5, false);

        tempDir = get_themes_folder() / ("__tmp_" + GenerateUUID());
        std::filesystem::create_directories(tempDir);
        std::filesystem::path zipPath = tempDir / (repo + ".zip");

        Http::DownloadWithProgress({ downloadUrl, 0 }, zipPath, [&](size_t downloaded, size_t total)
        {
            if (total > 0) {
                double percent = (double(downloaded) / total) * 100.0;
                m_updater->dispatch_progress("Downloading theme update...", 5.0 + (45.0 * (percent / 100.0)), false);
            }
        });
        logger.log("Download complete. Extracting...");

        const bool extracted = Util::ExtractZipArchive(zipPath.string(), tempDir.string(), [&](int current, int total, const char*)
        {
            double percent = (double(current) / total) * 100.0;
            m_updater->dispatch_progress("Extracting theme archive...", 50.0 + (40.0 * (percent / 100.0)), false);
        });

        if (!extracted) {
            std::filesystem::remove_all(tempDir);
            LOG_ERROR("Failed to extract theme update for '{}'", native);
            return false;
        }

        std::filesystem::path extractedFolder;
        for (const auto& entry : std::filesystem::directory_iterator(tempDir)) {
            if (entry.is_directory()) {
                extractedFolder = entry.path();
                break;
            }
        }

        if (extractedFolder.empty()) {
            std::filesystem::remove_all(tempDir);
            LOG_ERROR("No directory found in extracted theme archive for '{}'", native);
            return false;
        }

        m_updater->dispatch_progress("Copying updated theme files...", 92, false);
        logger.log("Copying updated theme files to: {}", themePath.string());
        for (const auto& p : std::filesystem::recursive_directory_iterator(extractedFolder)) {
            auto rel = std::filesystem::relative(p.path(), extractedFolder);
            std::filesystem::path dest = themePath / rel;
            if (std::filesystem::is_directory(p)) {
                std::filesystem::create_directories(dest);
            } else {
                std::filesystem::copy_file(p, dest, std::filesystem::copy_options::overwrite_existing);
            }
        }

        if (!write_metadata(themePath, owner, repo, latestCommit)) {
            std::filesystem::remove_all(tempDir);
            LOG_ERROR("[update-download] theme='{}': write_metadata FAILED — this will cause a repeated update loop", native);
            return false;
        }

        /* Verify the write round-trips correctly */
        {
            std::string verify = get_commit_hash(themePath);
            logger.log("[update-download] theme='{}' metadata written, verify read-back='{}'", native, verify);
            if (verify != latestCommit) {
                LOG_ERROR("[update-download] theme='{}': metadata read-back mismatch! wrote='{}' read='{}'", native, latestCommit, verify);
            }
        }

        m_updater->dispatch_progress("Cleaning up...", 96, false);
        logger.log("Cleaning up temporary files...");
        std::filesystem::remove_all(tempDir);
        tempDir.clear();

        /** trigger config update to regenerate config */
        themeConfig->on_config_change_hdlr();

        m_updater->dispatch_progress("Done!", 100, true);
        logger.log("Theme '{}' updated successfully.", native);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to update theme '{}': {}", native, e.what());
        if (!tempDir.empty() && std::filesystem::exists(tempDir)) {
            std::filesystem::remove_all(tempDir);
        }
        return false;
    }
}

std::vector<std::pair<nlohmann::json, std::filesystem::path>> head::theme_installer::query_themes_for_updates()
{
    std::vector<std::pair<nlohmann::json, std::filesystem::path>> updateQuery;
    nlohmann::json themes = head::Themes::FindAllThemes();

    for (auto& theme : themes) {
        if (!theme["data"].contains("github")) continue;

        std::filesystem::path path = get_themes_folder() / theme.value("native", "");
        if (!std::filesystem::exists(path)) continue;

        bool hasMetadata = std::filesystem::exists(path / "metadata.json");
        bool hasGit = std::filesystem::exists(path / ".git");

        if (hasMetadata || hasGit) {
            updateQuery.push_back({ theme, path });
        }
    }

    return updateQuery;
}

nlohmann::json head::theme_installer::make_post_body(const std::vector<nlohmann::json>& update_query)
{
    nlohmann::json post_body = nlohmann::json::array();

    for (const auto& theme : update_query) {
        if (!theme.contains("data") || !theme["data"].contains("github")) continue;

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

nlohmann::json head::theme_installer::get_request_body(void)
{
    nlohmann::json result;

    try {
        auto update_query = query_themes_for_updates();
        auto post_body = make_post_body([&update_query]
        {
            std::vector<nlohmann::json> themes;
            for (const auto& pair : update_query) {
                themes.push_back(pair.first);
            }
            return themes;
        }());

        if (update_query.empty()) {
            logger.log("No themes to update!");
            return {
                { "update_query", nullptr },
                { "post_body",    nullptr }
            };
        }

        result["update_query"] = update_query;
        result["post_body"] = post_body;
    } catch (const std::exception& e) {
        logger.log(std::string("Exception in GetRequestBody: ") + e.what());
        result["update_query"] = nullptr;
        result["post_body"] = nullptr;
    }

    return result;
}

nlohmann::json head::theme_installer::process_update(const nlohmann::json& updateQuery, const nlohmann::json& remote)
{
    nlohmann::json updatedThemes = nlohmann::json::array();

    for (const auto& updateItem : updateQuery) {
        const std::filesystem::path path = updateItem[1];
        const nlohmann::json theme = updateItem[0];

        if (!has_github_data(theme)) {
            continue;
        }

        const std::string repoName = get_repository_name(theme);
        const auto remoteTheme = find_remote_theme(remote, repoName);

        if (remoteTheme == nullptr) {
            logger.log("[update-check] theme='{}' (repo='{}'): no matching entry in API response, skipping", path.filename().string(), repoName);
            continue;
        }

        if (has_updates(path, *remoteTheme)) {
            updatedThemes.push_back(create_update_info(theme, *remoteTheme));
        }
    }

    return updatedThemes;
}

bool head::theme_installer::has_github_data(const nlohmann::json& theme)
{
    return theme["data"].contains("github");
}

std::string head::theme_installer::get_repository_name(const nlohmann::json& theme)
{
    const nlohmann::json& githubData = theme["data"]["github"];
    return githubData.value("repo_name", "");
}

const nlohmann::json* head::theme_installer::find_remote_theme(const nlohmann::json& remote, const std::string& repoName)
{
    if (!remote.is_array()) {
        return nullptr;
    }

    auto it = std::find_if(remote.begin(), remote.end(), [&repoName](const nlohmann::json& item) { return item.value("name", "") == repoName; });

    return (it != remote.end()) ? &(*it) : nullptr;
}

bool head::theme_installer::has_updates(const std::filesystem::path& path, const nlohmann::json& remoteTheme)
{
    const std::string remoteCommit = remoteTheme.value("commit", "");
    const std::string localCommit = get_commit_hash(path);
    const bool differs = localCommit != remoteCommit;

    logger.log("[update-check] theme='{}' local='{}' remote='{}' has_update={}",
               path.filename().string(), localCommit, remoteCommit, differs);

    return differs;
}

nlohmann::json head::theme_installer::create_update_info(const nlohmann::json& theme, const nlohmann::json& remoteTheme)
{
    return nlohmann::json{
        { "message", remoteTheme.value("message", "No commit message.") },
        { "date", remoteTheme.value("date", "unknown") },
        { "commit", remoteTheme.value("url", "") },
        { "native", theme["native"] },
        { "name", theme["data"].value("name", theme["native"]) }
    };
}
