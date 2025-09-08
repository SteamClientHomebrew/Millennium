#include "__builtins__/plugin_installer.h"
#include <__builtins__/scan.h>
#include <_millennium_api.h>
#include <chrono>
#include <cstdio>
#include <curl/curl.h>
#include <env.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <minizip/unzip.h>
#include <random>
#include <sstream>
#include <thread>

std::filesystem::path PluginInstaller::PluginsPath()
{
    return std::filesystem::path(GetEnv("MILLENNIUM__PLUGINS_PATH"));
}

bool PluginInstaller::CheckInstall(const std::string& pluginName)
{
    return Millennium::Plugins::GetPluginFromName(pluginName).has_value();
}

bool PluginInstaller::UninstallPlugin(const std::string& pluginName)
{
    try {
        auto pluginOpt = Millennium::Plugins::GetPluginFromName(pluginName);
        if (!pluginOpt)
            return false;

        std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();

        if (settingsStore->IsEnabledPlugin(pluginName)) {
            Millennium_TogglePluginStatus({
                PluginStatus{ pluginName, false }
            });
        }

        std::filesystem::remove_all(pluginOpt->at("path").get<std::string>());
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to uninstall {}: {}", pluginName, e.what());
        return false;
    }
}

size_t WriteCallback(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* downloaded = static_cast<size_t*>(userdata);
    size_t written = size * nmemb;
    *downloaded += written;
    return written;
}

void PluginInstaller::DownloadWithProgress(const std::string& url, const std::filesystem::path& destPath, std::function<void(size_t, size_t)> progressCallback)
{
    CURL* curl = curl_easy_init();
    if (!curl)
        throw std::runtime_error("Failed to initialize curl");

    FILE* fp = fopen(destPath.string().c_str(), "wb");
    if (!fp)
        throw std::runtime_error("Failed to open file: " + destPath.string());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);

    if (progressCallback)
        progressCallback(std::filesystem::file_size(destPath), std::filesystem::file_size(destPath));
}

void PluginInstaller::ExtractZipWithProgress(const std::filesystem::path& zipPath, const std::filesystem::path& extractTo)
{
    unzFile zip = unzOpen(zipPath.string().c_str());
    if (!zip)
        throw std::runtime_error("Failed to open zip: " + zipPath.string());

    unz_global_info globalInfo;
    unzGetGlobalInfo(zip, &globalInfo);

    for (uLong i = 0; i < globalInfo.number_entry; ++i) {
        char filename[512];
        unz_file_info fileInfo;
        unzGetCurrentFileInfo(zip, &fileInfo, filename, sizeof(filename), nullptr, 0, nullptr, 0);

        std::string outPath = (extractTo / filename).string();
        std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());

        if (unzOpenCurrentFile(zip) == UNZ_OK) {
            std::ofstream outFile(outPath, std::ios::binary);
            char buffer[8192];
            int bytesRead = 0;
            while ((bytesRead = unzReadCurrentFile(zip, buffer, sizeof(buffer))) > 0)
                outFile.write(buffer, bytesRead);
            outFile.close();
            unzCloseCurrentFile(zip);
        }

        EmitMessage("Extracting plugin archive...", 50.0 + (45.0 * (i / (double)globalInfo.number_entry)), false);
        if ((i + 1) < globalInfo.number_entry)
            unzGoToNextFile(zip);
    }

    unzClose(zip);
}

void PluginInstaller::EmitMessage(const std::string& status, double progress, bool isComplete)
{
    nlohmann::json message = {
        { "status",     status     },
        { "progress",   progress   },
        { "isComplete", isComplete }
    };

    // Millennium::CallFrontendMethod("InstallerMessageEmitter", {j.dump()});
}

bool PluginInstaller::InstallPlugin(const std::string& downloadUrl, size_t totalSize)
{
    try {
        Logger.Log("Requesting to install plugin -> " + downloadUrl);
        EmitMessage("Starting Plugin Installer...", 0, false);

        const auto downloadPath = PluginsPath();
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Generate UUID
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

        uint32_t data[4];
        for (int i = 0; i < 4; ++i)
            data[i] = dis(gen);

        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(8) << data[0] << "-" << std::setw(4) << ((data[1] >> 16) & 0xFFFF) << "-" << std::setw(4)
           << (((data[1] >> 0) & 0x0FFF) | 0x4000) << "-" << std::setw(4) << (((data[2] >> 16) & 0x3FFF) | 0x8000) << "-" << std::setw(4) << (data[2] & 0xFFFF) << std::setw(8)
           << data[3];

        std::string uuidStr = ss.str();

        std::filesystem::path zipPath = downloadPath / uuidStr;

        auto progressCallback = [&](size_t downloaded, size_t total)
        {
            double percent = totalSize ? (double(downloaded) / totalSize) * 100.0 : 0.0;
            EmitMessage("Download plugin archive...", 50.0 * (percent / 100.0), false);
        };

        DownloadWithProgress(downloadUrl, zipPath, progressCallback);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        EmitMessage("Setting up installed plugin...", 50, false);
        std::filesystem::create_directories(downloadPath);
        ExtractZipWithProgress(zipPath, downloadPath);

        EmitMessage("Cleaning up...", 95, false);
        std::filesystem::remove(zipPath);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        EmitMessage("Done!", 100, true);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to install plugin: {}", e.what());
        return false;
    }
}

std::optional<nlohmann::json> PluginInstaller::ReadMetadata(const std::filesystem::path& pluginPath)
{
    std::filesystem::path metadataPath = pluginPath / "metadata.json";
    if (!std::filesystem::exists(metadataPath))
        return std::nullopt;

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

std::vector<nlohmann::json> PluginInstaller::GetPluginData()
{
    std::vector<nlohmann::json> pluginData;
    for (const auto& entry : std::filesystem::directory_iterator(PluginsPath())) {
        if (!entry.is_directory())
            continue;

        auto metadata = ReadMetadata(entry.path());
        if (metadata)
            pluginData.push_back(*metadata);
    }
    return pluginData;
}

bool PluginInstaller::DownloadPluginUpdate(const std::string& id, const std::string& name)
{
    try {
        std::string url = "https://steambrew.app/api/v1/plugins/download?id=" + id + "&n=" + name + ".zip";
        Logger.Log("Starting plugin update for '{}'. Download URL: {}", name, url);

        std::filesystem::path tempDir = PluginsPath() / "__tmp_extract";
        Logger.Log("Creating temporary extraction directory: {}", tempDir.string());
        std::filesystem::create_directories(tempDir);

        std::filesystem::path tempZipPath = tempDir / (name + ".zip");
        Logger.Log("Temporary zip path: {}", tempZipPath.string());

        Logger.Log("Downloading plugin archive...");
        DownloadWithProgress(url, tempZipPath, nullptr);
        Logger.Log("Download complete. Extracting plugin '{}' into '{}'", name, tempDir.string());

        ExtractZipWithProgress(tempZipPath, tempDir);
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

        std::filesystem::path targetFolder = PluginsPath() / name;
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

nlohmann::json PluginInstaller::GetRequestBody()
{
    auto data = GetPluginData();
    return data.empty() ? nlohmann::json::array() : nlohmann::json(data);
}