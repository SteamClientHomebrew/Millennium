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

#include "head/plugin_mgr.h"
#include "head/entry_point.h"
#include "head/library_updater.h"
#include "head/scan.h"

#include "millennium/encode.h"
#include "millennium/http.h"
#include "millennium/logger.h"
#include "millennium/zip.h"

plugin_installer::plugin_installer(std::weak_ptr<millennium_backend> millennium_backend, std::shared_ptr<SettingsStore> settings_store_ptr, std::weak_ptr<Updater> updater)
    : m_millennium_backend(std::move(millennium_backend)), settings_store_ptr(std::move(settings_store_ptr)), m_updater(std::move(updater))
{
}

std::filesystem::path plugin_installer::get_plugins_path()
{
    return std::filesystem::path(GetEnv("MILLENNIUM__PLUGINS_PATH"));
}

bool plugin_installer::is_plugin_installed(const std::string& pluginName) const
{
    return Millennium::Plugins::GetPluginFromName(pluginName, settings_store_ptr).has_value();
}

bool plugin_installer::uninstall_plugin(const std::string& pluginName) const
{
    try {
        auto pluginOpt = Millennium::Plugins::GetPluginFromName(pluginName, settings_store_ptr);
        if (!pluginOpt) {
            return false;
        }

        if (settings_store_ptr->IsEnabledPlugin(pluginName)) {
            if (const auto ptr = m_millennium_backend.lock()) {
                ptr->Millennium_TogglePluginStatus({
                    PluginStatus{ pluginName, false }
                });
            } else {
                LOG_ERROR("[plugin_installer] Failed to lock builtin_plugin_backend...");
            }
        }

        const std::filesystem::path pluginPath = pluginOpt->at("path").get<std::string>();
        Logger.Log("Attempting to remove plugin directory: {}", pluginPath.string());

        if (!exists(pluginPath)) {
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

nlohmann::json plugin_installer::install_plugin(const std::string& downloadUrl, const size_t totalSize) const
{
    try {
        Logger.Log("Requesting to install plugin -> " + downloadUrl);
        if (const auto updater = m_updater.lock()) {
            updater->dispatch_progress("Starting Plugin Installer...", 0, false);
        }
        const auto downloadPath = get_plugins_path();
        std::this_thread::sleep_for(std::chrono::seconds(2));

        const std::string uuidStr = GenerateUUID();
        const std::filesystem::path zipPath = downloadPath / uuidStr;

        auto progressCallback = [&](const size_t downloaded, size_t)
        {
            const double percent = totalSize ? (static_cast<double>(downloaded) / totalSize) * 100.0 : 0.0;
            if (const auto updater = m_updater.lock()) {
                updater->dispatch_progress("Download plugin archive...", 50.0 * (percent / 100.0), false);
            }
        };

        Http::DownloadWithProgress({ downloadUrl, totalSize }, zipPath, progressCallback);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (const auto updater = m_updater.lock()) {
            updater->dispatch_progress("Setting up installed plugin...", 50, false);
        }
        create_directories(downloadPath);
        Util::ExtractZipArchive(zipPath.string(), downloadPath.string(), [&](const int current, const int total, const char*)
        {
            const double percent = (static_cast<double>(current) / total) * 100.0;
            if (const auto updater = m_updater.lock()) {
                updater->dispatch_progress("Extracting plugin archive...", 50.0 + (45.0 * (percent / 100.0)), false);
            }
        });

        if (const auto updater = m_updater.lock()) {
            updater->dispatch_progress("Cleaning up...", 95, false);
        }

        const auto pluginsRoot = get_plugins_path();
        const auto lastSlash = downloadUrl.find_last_of('/');
        const std::string pluginName = (lastSlash != std::string::npos) ? downloadUrl.substr(lastSlash + 1) : uuidStr;
        const std::filesystem::path pluginPath = pluginsRoot / pluginName;

        create_directories(pluginPath);
        std::filesystem::rename(downloadPath, pluginPath);
        std::filesystem::remove(zipPath);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (const auto updater = m_updater.lock()) {
            updater->dispatch_progress("Done!", 100, true);
        }
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
    if (!exists(metadataPath)) {
        return std::nullopt;
    }

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
        if (!entry.is_directory()) {
            continue;
        }

        auto metadata = read_plugin_metadata(entry.path());
        if (metadata) {
            pluginData.push_back(*metadata);
        }
    }
    return pluginData;
}

bool plugin_installer::update_plugin(const std::string& id, const std::string& name) const
{
    try {
        std::string url = "https://steambrew.app/api/v1/plugins/download?id=" + id + "&n=" + name + ".zip";
        Logger.Log("Starting plugin update for '{}'. Download URL: {}", name, url);

        const std::filesystem::path tempDir = get_plugins_path() / "__tmp_extract";
        Logger.Log("Creating temporary extraction directory: {}", tempDir.string());
        create_directories(tempDir);

        const std::filesystem::path tempZipPath = tempDir / (name + ".zip");
        Logger.Log("Temporary zip path: {}", tempZipPath.string());

        Logger.Log("Downloading plugin archive...");
        Http::DownloadWithProgress({ url, 0 }, tempZipPath, nullptr);
        Logger.Log("Download complete. Extracting plugin '{}' into '{}'", name, tempDir.string());

        Util::ExtractZipArchive(tempZipPath.string(), tempDir.string(), [&](const int current, const int total, const char*)
        {
            const double percent = (static_cast<double>(current) / total) * 100.0;
            if (const auto updater = m_updater.lock()) {
                updater->dispatch_progress("Extracting plugin archive...", 50.0 + (45.0 * (percent / 100.0)), false);
            }
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
            remove_all(tempDir);
            return false;
        }

        const std::filesystem::path targetFolder = get_plugins_path() / name;
        Logger.Log("Preparing target plugin directory: {}", targetFolder.string());
        create_directories(targetFolder);

        Logger.Log("Copying plugin files to target directory...");
        for (auto& p : std::filesystem::recursive_directory_iterator(extractedFolder)) {
            auto rel = relative(p.path(), extractedFolder);
            std::filesystem::path dest = targetFolder / rel;
            if (is_directory(p)) {
                Logger.Log("Creating directory: {}", dest.string());
                create_directories(dest);
            } else {
                Logger.Log("Copying file: {} -> {}", p.path().string(), dest.string());
                copy_file(p, dest, std::filesystem::copy_options::overwrite_existing);
            }
        }

        Logger.Log("Plugin '{}' installed successfully to '{}'", name, targetFolder.string());
        Logger.Log("Cleaning up temporary files...");
        remove_all(tempDir);
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
