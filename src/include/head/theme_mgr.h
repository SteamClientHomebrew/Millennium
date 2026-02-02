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
#include "millennium/fwd_decl.h"
#include "head/theme_cfg.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace head
{
class theme_installer
{
  public:
    theme_installer(std::shared_ptr<plugin_manager> settings_store_ptr, std::shared_ptr<library_updater> updater);
    std::filesystem::path get_skins_folder();

    nlohmann::json create_error_response(const std::string& message);
    nlohmann::json create_successful_response();

    std::optional<nlohmann::json> get_theme_from_github(const std::string& repo, const std::string& owner, bool asString = false);
    bool is_theme_installed(const std::string& repo, const std::string& owner);
    nlohmann::json uninstall_theme(std::shared_ptr<theme_config_store> themeConfig, const std::string& repo, const std::string& owner);
    nlohmann::json install_theme(std::shared_ptr<theme_config_store> themeConfig, const std::string& repo, const std::string& owner);

    int clone(const std::string& url, const std::filesystem::path& dstPath, std::string& outErr, std::function<void(size_t, size_t, size_t)> progressCallback);
    bool update_theme(std::shared_ptr<theme_config_store> themeConfig, const std::string& native);

    std::vector<std::pair<nlohmann::json, std::filesystem::path>> query_themes_for_updates();
    nlohmann::json process_update(const nlohmann::json& updateQuery, const nlohmann::json& remote);

    nlohmann::json get_request_body(void);
    nlohmann::json make_post_body(const std::vector<nlohmann::json>& update_query);

  private:
    std::shared_ptr<plugin_manager> m_settings_store_ptr;
    std::shared_ptr<library_updater> m_updater;

    std::string make_temp_dir_name(const std::filesystem::path& base, const std::string& repo);
    bool is_git_repository(const std::filesystem::path& path);
    bool has_github_data(const nlohmann::json& theme);
    std::string get_repository_name(const nlohmann::json& theme);
    const nlohmann::json* find_remote_theme(const nlohmann::json& remote, const std::string& repoName);
    bool has_updates(const std::filesystem::path& path, const nlohmann::json& remoteTheme);
    nlohmann::json create_update_info(const nlohmann::json& theme, const nlohmann::json& remoteTheme);
    std::string get_commit_hash(const std::filesystem::path& path);
};
} // namespace head
