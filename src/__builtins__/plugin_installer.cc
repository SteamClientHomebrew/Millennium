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

fs::path PluginInstaller::PluginsPath()
{
    return fs::path(GetEnv("MILLENNIUM__PLUGINS_PATH"));
}

bool PluginInstaller::CheckInstall(const std::string& pluginName)
{
    return Millennium::Plugins::GetPluginFromName(pluginName).has_value();
}

bool PluginInstaller::UninstallPlugin(const std::string& pluginName)
{
    try
    {
        auto pluginOpt = Millennium::Plugins::GetPluginFromName(pluginName);
        if (!pluginOpt)
            return false;

        std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();

        if (settingsStore->IsEnabledPlugin(pluginName))
        {
            Millennium_TogglePluginStatus({
                PluginStatus{pluginName, false}
            });
        }

        fs::remove_all(pluginOpt->at("path").get<std::string>());
        return true;
    }
    catch (const std::exception& e)
    {
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

void PluginInstaller::DownloadWithProgress(const std::string& url, const fs::path& destPath, std::function<void(size_t, size_t)> progressCallback)
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
        progressCallback(fs::file_size(destPath), fs::file_size(destPath));
}

void PluginInstaller::ExtractZipWithProgress(const fs::path& zipPath, const fs::path& extractTo)
{
    unzFile zip = unzOpen(zipPath.string().c_str());
    if (!zip)
        throw std::runtime_error("Failed to open zip: " + zipPath.string());

    unz_global_info globalInfo;
    unzGetGlobalInfo(zip, &globalInfo);

    for (uLong i = 0; i < globalInfo.number_entry; ++i)
    {
        char filename[512];
        unz_file_info fileInfo;
        unzGetCurrentFileInfo(zip, &fileInfo, filename, sizeof(filename), nullptr, 0, nullptr, 0);

        std::string outPath = (extractTo / filename).string();
        fs::create_directories(fs::path(outPath).parent_path());

        if (unzOpenCurrentFile(zip) == UNZ_OK)
        {
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
    json j = {
        {"status",     status    },
        {"progress",   progress  },
        {"isComplete", isComplete}
    };

    // Millennium::CallFrontendMethod("InstallerMessageEmitter", {j.dump()});
}

bool PluginInstaller::InstallPlugin(const std::string& downloadUrl, size_t totalSize)
{
    try
    {
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

        fs::path zipPath = downloadPath / uuidStr;

        auto progressCallback = [&](size_t downloaded, size_t total)
        {
            double percent = totalSize ? (double(downloaded) / totalSize) * 100.0 : 0.0;
            EmitMessage("Download plugin archive...", 50.0 * (percent / 100.0), false);
        };

        DownloadWithProgress(downloadUrl, zipPath, progressCallback);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        EmitMessage("Setting up installed plugin...", 50, false);
        fs::create_directories(downloadPath);
        ExtractZipWithProgress(zipPath, downloadPath);

        EmitMessage("Cleaning up...", 95, false);
        fs::remove(zipPath);
        std::this_thread::sleep_for(std::chrono::seconds(2));

        EmitMessage("Done!", 100, true);
        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to install plugin: {}", e.what());
        return false;
    }
}

std::optional<json> PluginInstaller::ReadMetadata(const fs::path& pluginPath)
{
    fs::path metadataPath = pluginPath / "metadata.json";
    if (!fs::exists(metadataPath))
        return std::nullopt;

    try
    {
        std::ifstream in(metadataPath);
        json metadata;
        in >> metadata;

        if (metadata.contains("id") && metadata.contains("commit"))
        {
            return json{
                {"id",     metadata["id"]                },
                {"commit", metadata["commit"]            },
                {"name",   pluginPath.filename().string()}
            };
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to read metadata.json for plugin {}: {}", pluginPath.filename().string(), e.what());
    }
    return std::nullopt;
}

std::vector<json> PluginInstaller::GetPluginData()
{
    std::vector<json> pluginData;
    for (const auto& entry : fs::directory_iterator(PluginsPath()))
    {
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
    try
    {
        std::string url = "https://steambrew.app/api/v1/plugins/download?id=" + id + "&n=" + name + ".zip";
        Logger.Log("Starting plugin update for '{}'. Download URL: {}", name, url);

        fs::path tempDir = PluginsPath() / "__tmp_extract";
        Logger.Log("Creating temporary extraction directory: {}", tempDir.string());
        fs::create_directories(tempDir);

        fs::path tempZipPath = tempDir / (name + ".zip");
        Logger.Log("Temporary zip path: {}", tempZipPath.string());

        Logger.Log("Downloading plugin archive...");
        DownloadWithProgress(url, tempZipPath, nullptr);
        Logger.Log("Download complete. Extracting plugin '{}' into '{}'", name, tempDir.string());

        ExtractZipWithProgress(tempZipPath, tempDir);
        Logger.Log("Extraction complete.");

        fs::path extractedFolder;
        Logger.Log("Searching for extracted plugin directory...");
        for (auto& entry : fs::directory_iterator(tempDir))
        {
            Logger.Log("Found entry in temp dir: {}", entry.path().string());
            if (entry.is_directory())
            {
                extractedFolder = entry.path();
                Logger.Log("Extracted plugin directory found: {}", extractedFolder.string());
                break;
            }
        }

        if (extractedFolder.empty())
        {
            Logger.Log("No directory found in extracted plugin '{}'. Cleaning up.", name);
            fs::remove_all(tempDir);
            return false;
        }

        fs::path targetFolder = PluginsPath() / name;
        Logger.Log("Preparing target plugin directory: {}", targetFolder.string());
        fs::create_directories(targetFolder);

        Logger.Log("Copying plugin files to target directory...");
        for (auto& p : fs::recursive_directory_iterator(extractedFolder))
        {
            auto rel = fs::relative(p.path(), extractedFolder);
            fs::path dest = targetFolder / rel;
            if (fs::is_directory(p))
            {
                Logger.Log("Creating directory: {}", dest.string());
                fs::create_directories(dest);
            }
            else
            {
                Logger.Log("Copying file: {} -> {}", p.path().string(), dest.string());
                fs::copy_file(p, dest, fs::copy_options::overwrite_existing);
            }
        }

        Logger.Log("Plugin '{}' installed successfully to '{}'", name, targetFolder.string());
        Logger.Log("Cleaning up temporary files...");
        fs::remove_all(tempDir);
        Logger.Log("Update process for plugin '{}' completed successfully.", name);
        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to update plugin '{}': {}", name, e.what());
        return false;
    }
}

json PluginInstaller::GetRequestBody()
{
    auto data = GetPluginData();
    return data.empty() ? json::array() : json(data);
}