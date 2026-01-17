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

#include "head/entry_point.h"
#include "head/library_updater.h"
#include "head/plugin_mgr.h"
#include "head/scan.h"

#include "millennium/millennium.h"
#include "millennium/logger.h"
#include "millennium/http.h"
#include "millennium/environment.h"
#include "millennium/encoding.h"
#include "millennium/zip.h"

plugin_installer::plugin_installer(std::weak_ptr<millennium_backend> millennium_backend, std::shared_ptr<SettingsStore> settings_store_ptr,
                                   std::shared_ptr<library_updater> updater)
    : m_millennium_backend(std::move(millennium_backend)), settings_store_ptr(std::move(settings_store_ptr)), m_updater(std::move(updater))
{
}

std::filesystem::path plugin_installer::get_plugins_path()
{
    return std::filesystem::path(GetEnv("MILLENNIUM__PLUGINS_PATH"));
}

bool plugin_installer::is_plugin_installed(const std::string& pluginName)
{
    return Millennium::Plugins::GetPluginFromName(pluginName, settings_store_ptr).has_value();
}

bool plugin_installer::uninstall_plugin(const std::string& pluginName)
{
    try {
        auto pluginOpt = Millennium::Plugins::GetPluginFromName(pluginName, settings_store_ptr);
        if (!pluginOpt) return false;

        if (settings_store_ptr->IsEnabledPlugin(pluginName)) {
            g_millennium->get_plugin_loader()->set_plugin_enable(pluginName, false);
        }

        std::filesystem::path pluginPath = pluginOpt->at("path").get<std::string>();
        Logger.Log("Attempting to remove plugin directory: {}", pluginPath.string());

        if (!std::filesystem::exists(pluginPath)) {
            LOG_ERROR("Plugin path does not exist: {}", pluginPath.string());
            return false;
        }

        SystemIO::SafePurgeDirectory(pluginPath);

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to uninstall {}: {}", pluginName, e.what());
        return false;
    }
}

nlohmann::json plugin_installer::install_plugin(const std::string& downloadUrl, size_t totalSize)
{
    try {
        Logger.Log("Requesting to install plugin -> " + downloadUrl);
        m_updater->dispatch_progress("Starting Plugin Installer...", 0, false);

        const auto downloadPath = get_plugins_path();
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::string uuidStr = GenerateUUID();
        std::filesystem::path zipPath = downloadPath / uuidStr;

        auto progressCallback = [&](size_t downloaded, size_t)
        {
            double percent = totalSize ? (double(downloaded) / totalSize) * 100.0 : 0.0;
            m_updater->dispatch_progress("Download plugin archive...", 50.0 * (percent / 100.0), false);
        };

        Http::DownloadWithProgress({ downloadUrl, totalSize }, zipPath, progressCallback);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        m_updater->dispatch_progress("Setting up installed plugin...", 50, false);
        std::filesystem::create_directories(downloadPath);
        Util::ExtractZipArchive(zipPath.string(), downloadPath.string(), [&](int current, int total, const char*)
        {
            double percent = (double(current) / total) * 100.0;
            m_updater->dispatch_progress("Extracting plugin archive...", 50.0 + (45.0 * (percent / 100.0)), false);
        });

        m_updater->dispatch_progress("Cleaning up...", 95, false);
        std::filesystem::remove(zipPath);
        std::this_thread::sleep_for(std::chrono::seconds(2));

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

std::optional<nlohmann::json> plugin_installer::read_plugin_metadata(const std::filesystem::path& pluginPath)
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

std::vector<nlohmann::json> plugin_installer::get_plugin_data()
{
    std::vector<nlohmann::json> pluginData;
    for (const auto& entry : std::filesystem::directory_iterator(get_plugins_path())) {
        if (!entry.is_directory()) continue;

        auto metadata = read_plugin_metadata(entry.path());
        if (metadata) pluginData.push_back(*metadata);
    }
    return pluginData;
}

bool plugin_installer::update_plugin(const std::string& id, const std::string& name)
{
    try {
        std::string url = "https://steambrew.app/api/v1/plugins/download?id=" + id + "&n=" + name + ".zip";
        Logger.Log("Starting plugin update for '{}'. Download URL: {}", name, url);

        std::filesystem::path tempDir = get_plugins_path() / "__tmp_extract";
        Logger.Log("Creating temporary extraction directory: {}", tempDir.string());
        std::filesystem::create_directories(tempDir);

        std::filesystem::path tempZipPath = tempDir / (name + ".zip");
        Logger.Log("Temporary zip path: {}", tempZipPath.string());

        Logger.Log("Downloading plugin archive...");
        Http::DownloadWithProgress({ url, 0 }, tempZipPath, nullptr);
        Logger.Log("Download complete. Extracting plugin '{}' into '{}'", name, tempDir.string());

        Util::ExtractZipArchive(tempZipPath.string(), tempDir.string(), [&](int current, int total, const char*)
        {
            double percent = (double(current) / total) * 100.0;
            m_updater->dispatch_progress("Extracting plugin archive...", 50.0 + (45.0 * (percent / 100.0)), false);
        });

        Logger.Log("Extraction complete.");

        std::filesystem::path extractedFolder;
        Logger.Log("Searching for extracted plugin directory...");
        for (auto& entry : std::filesystem::directory_iterator(tempDir)) {
            Logger.Log("Found entry in temp dir: {}", entry.path().string());
            if (entry.is_directory()) {
                extractedFolder = entry.path();
                Logger.Log("Extracted plugin directory found: {}", extractedFolder.string());
                break;
            }
        }

        if (extractedFolder.empty()) {
            Logger.Log("No directory found in extracted plugin '{}'. Cleaning up.", name);
            std::filesystem::remove_all(tempDir);
            return false;
        }

        std::filesystem::path targetFolder = get_plugins_path() / name;
        Logger.Log("Preparing target plugin directory: {}", targetFolder.string());
        std::filesystem::create_directories(targetFolder);

        Logger.Log("Copying plugin files to target directory...");
        for (auto& p : std::filesystem::recursive_directory_iterator(extractedFolder)) {
            auto rel = std::filesystem::relative(p.path(), extractedFolder);
            std::filesystem::path dest = targetFolder / rel;
            if (std::filesystem::is_directory(p)) {
                Logger.Log("Creating directory: {}", dest.string());
                std::filesystem::create_directories(dest);
            } else {
                Logger.Log("Copying file: {} -> {}", p.path().string(), dest.string());
                std::filesystem::copy_file(p, dest, std::filesystem::copy_options::overwrite_existing);
            }
        }

        Logger.Log("Plugin '{}' installed successfully to '{}'", name, targetFolder.string());
        Logger.Log("Cleaning up temporary files...");
        std::filesystem::remove_all(tempDir);
        Logger.Log("Update process for plugin '{}' completed successfully.", name);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to update plugin '{}': {}", name, e.what());
        return false;
    }
}

nlohmann::json plugin_installer::get_updater_request_body()
{
    auto data = get_plugin_data();
    return data.empty() ? nlohmann::json::array() : nlohmann::json(data);
}
