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

#include "head/entry_point.h"
#include "head/library_updater.h"
#include "head/plugin_mgr.h"
#include "head/scan.h"

#include "millennium/filesystem.h"
#include "millennium/millennium.h"
#include "millennium/logger.h"
#include "millennium/http.h"
#include "millennium/environment.h"
#include "millennium/encoding.h"
#include "millennium/zip.h"

head::plugin_installer::plugin_installer(std::weak_ptr<millennium_backend> millennium_backend, std::shared_ptr<::plugin_manager> plugin_mgr,
                                         std::shared_ptr<library_updater> updater)
    : m_millennium_backend(std::move(millennium_backend)), m_plugin_manager(std::move(plugin_mgr)), m_updater(std::move(updater))
{
}

std::filesystem::path head::plugin_installer::get_plugins_path()
{
    return std::filesystem::path(platform::environment::get("MILLENNIUM__PLUGINS_PATH"));
}

bool head::plugin_installer::is_plugin_installed(const std::string& pluginName)
{
    return head::Plugins::GetPluginFromName(pluginName, m_plugin_manager).has_value();
}

bool head::plugin_installer::uninstall_plugin(const std::string& pluginName)
{
    try {
        auto pluginOpt = head::Plugins::GetPluginFromName(pluginName, m_plugin_manager);
        if (!pluginOpt) return false;

        if (m_plugin_manager->is_enabled(pluginName)) {
            g_millennium->get_plugin_loader()->set_plugin_enable(pluginName, false);
        }

        std::filesystem::path pluginPath = pluginOpt->at("path").get<std::string>();
        logger.log("Attempting to remove plugin directory: {}", pluginPath.string());

        if (!std::filesystem::exists(pluginPath)) {
            LOG_ERROR("Plugin path does not exist: {}", pluginPath.string());
            return false;
        }

        platform::safe_remove_directory(pluginPath);

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to uninstall {}: {}", pluginName, e.what());
        return false;
    }
}

nlohmann::json head::plugin_installer::install_plugin(const std::string& downloadUrl, size_t totalSize)
{
    try {
        logger.log("Requesting to install plugin -> " + downloadUrl);
        m_updater->dispatch_progress("Starting Plugin Installer...", 0, false);

        const auto downloadPath = get_plugins_path();
        std::filesystem::create_directories(downloadPath);
        std::string uuidStr = GenerateUUID();
        std::filesystem::path zipPath = downloadPath / (uuidStr + ".zip");

        auto progressCallback = [&](size_t downloaded, size_t)
        {
            double percent = totalSize ? (double(downloaded) / totalSize) * 100.0 : 0.0;
            m_updater->dispatch_progress("Download plugin archive...", 50.0 * (percent / 100.0), false);
        };

        Http::DownloadWithProgress({ downloadUrl, totalSize }, zipPath, progressCallback);
        m_updater->dispatch_progress("Setting up installed plugin...", 50, false);
        const bool extracted = Util::ExtractZipArchive(zipPath.string(), downloadPath.string(), [&](int current, int total, const char*)
        {
            double percent = (double(current) / total) * 100.0;
            m_updater->dispatch_progress("Extracting plugin archive...", 50.0 + (45.0 * (percent / 100.0)), false);
        });

        if (!extracted) {
            std::filesystem::remove(zipPath);
            throw std::runtime_error("Failed to extract plugin archive");
        }

        m_updater->dispatch_progress("Cleaning up...", 95, false);
        std::filesystem::remove(zipPath);
        m_updater->dispatch_progress("Done!", 100, true);

        return {
            { "success", true }
        };
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to install plugin: {}", e.what());

        return {
            { "success", false    },
            { "error",   e.what() }
        };
    }
}

std::optional<nlohmann::json> head::plugin_installer::read_plugin_metadata(const std::filesystem::path& pluginPath)
{
    std::filesystem::path metadataPath = pluginPath / "metadata.json";
    if (!std::filesystem::exists(metadataPath)) return std::nullopt;

    try {
        std::ifstream in(metadataPath);
        nlohmann::json metadata;
        in >> metadata;

        if (metadata.contains("id") && metadata.contains("commit")) {
            return nlohmann::json{
                { "id",     metadata["id"]                 },
                { "commit", metadata["commit"]             },
                { "name",   pluginPath.filename().string() }
            };
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to read metadata.json for plugin {}: {}", pluginPath.filename().string(), e.what());
    }
    return std::nullopt;
}

std::vector<nlohmann::json> head::plugin_installer::get_plugin_data()
{
    std::vector<nlohmann::json> pluginData;
    const auto pluginsPath = get_plugins_path();
    if (!std::filesystem::exists(pluginsPath)) {
        std::filesystem::create_directories(pluginsPath);
        return pluginData;
    }
    for (const auto& entry : std::filesystem::directory_iterator(pluginsPath)) {
        if (!entry.is_directory()) continue;

        auto metadata = read_plugin_metadata(entry.path());
        if (metadata) pluginData.push_back(*metadata);
    }
    return pluginData;
}

bool head::plugin_installer::update_plugin(const std::string& id, const std::string& name, const std::string& commit)
{
    std::filesystem::path tempDir;
    try {
        std::string url = "https://steambrew.app/api/v1/plugins/download?id=" + id + "&n=" + name + ".zip";
        logger.log("Starting plugin update for '{}'. Download URL: {}", name, url);

        tempDir = get_plugins_path() / ("__tmp_" + GenerateUUID());
        logger.log("Creating temporary extraction directory: {}", tempDir.string());
        std::filesystem::create_directories(tempDir);

        std::filesystem::path tempZipPath = tempDir / (name + ".zip");
        logger.log("Temporary zip path: {}", tempZipPath.string());

        m_updater->dispatch_progress("Downloading plugin update...", 5, false);
        logger.log("Downloading plugin archive...");
        Http::DownloadWithProgress({ url, 0 }, tempZipPath, [&](size_t downloaded, size_t total)
        {
            if (total > 0) {
                double percent = (double(downloaded) / total) * 100.0;
                m_updater->dispatch_progress("Downloading plugin update...", 5.0 + (45.0 * (percent / 100.0)), false);
            }
        });
        logger.log("Download complete. Extracting plugin '{}' into '{}'", name, tempDir.string());

        Util::ExtractZipArchive(tempZipPath.string(), tempDir.string(), [&](int current, int total, const char*)
        {
            double percent = (double(current) / total) * 100.0;
            m_updater->dispatch_progress("Extracting plugin archive...", 50.0 + (40.0 * (percent / 100.0)), false);
        });

        logger.log("Extraction complete.");

        std::filesystem::path extractedFolder;
        logger.log("Searching for extracted plugin directory...");
        for (auto& entry : std::filesystem::directory_iterator(tempDir)) {
            logger.log("Found entry in temp dir: {}", entry.path().string());
            if (entry.is_directory()) {
                extractedFolder = entry.path();
                logger.log("Extracted plugin directory found: {}", extractedFolder.string());
                break;
            }
        }

        if (extractedFolder.empty()) {
            logger.log("No directory found in extracted plugin '{}'. Cleaning up.", name);
            std::filesystem::remove_all(tempDir);
            return false;
        }

        std::filesystem::path targetFolder = get_plugins_path() / name;
        logger.log("Preparing target plugin directory: {}", targetFolder.string());
        std::filesystem::create_directories(targetFolder);

        m_updater->dispatch_progress("Copying plugin files...", 92, false);
        logger.log("Copying plugin files to target directory...");
        for (auto& p : std::filesystem::recursive_directory_iterator(extractedFolder)) {
            auto rel = std::filesystem::relative(p.path(), extractedFolder);
            std::filesystem::path dest = targetFolder / rel;
            if (std::filesystem::is_directory(p)) {
                std::filesystem::create_directories(dest);
            } else {
                std::filesystem::copy_file(p, dest, std::filesystem::copy_options::overwrite_existing);
            }
        }

        // Update metadata.json with the new commit hash so the next update check
        // sees the current version and doesn't report a stale update.
        if (!commit.empty()) {
            std::filesystem::path metadataPath = targetFolder / "metadata.json";
            nlohmann::json metadata;
            if (std::filesystem::exists(metadataPath)) {
                std::ifstream in(metadataPath);
                in >> metadata;
            }
            metadata["id"] = id;
            metadata["commit"] = commit;
            std::ofstream out(metadataPath);
            out << metadata.dump(4);
            logger.log("Updated metadata.json for '{}' with commit '{}'", name, commit);
        }

        logger.log("Plugin '{}' installed successfully to '{}'", name, targetFolder.string());
        m_updater->dispatch_progress("Cleaning up...", 96, false);
        logger.log("Cleaning up temporary files...");
        std::filesystem::remove_all(tempDir);
        m_updater->dispatch_progress("Done!", 100, true);
        logger.log("Update process for plugin '{}' completed successfully.", name);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to update plugin '{}': {}", name, e.what());
        if (!tempDir.empty()) {
            std::filesystem::remove_all(tempDir);
        }
        return false;
    }
}

nlohmann::json head::plugin_installer::get_updater_request_body()
{
    auto data = get_plugin_data();
    return data.empty() ? nlohmann::json::array() : nlohmann::json(data);
}
