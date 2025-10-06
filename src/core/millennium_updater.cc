#include "millennium/millennium_updater.h"
#include "millennium/semver.h"
#include "millennium/http.h"
#include "millennium/sysfs.h"
#include "millennium/encode.h"
#include "millennium/zip.h"
#include "head/ipc_handler.h"

#include <filesystem>
#include <fstream>
#include <exception>

#define MILLENNIUM_UPDATER_TEMP_DIR "millennium-updater-temp-files"

// Thread-safe version with proper synchronization
static std::atomic<bool> hasUpdatedMillennium{ false };
static std::mutex update_mutex; // Protects the update process

std::string MillenniumUpdater::ParseVersion(const std::string& version)
{
    if (!version.empty() && version[0] == 'v')
        return version.substr(1);
    return version;
}

void MillenniumUpdater::CheckForUpdates()
{
    if (!CONFIG.Get("general.checkForMillenniumUpdates", true).get<bool>()) {
        Logger.Warn("User has disabled update checking for Millennium.");
        return;
    }

    Logger.Log("Checking for updates...");
    std::string response;

    try {
        response = Http::Get(GITHUB_API_URL, false);
        auto response_data = nlohmann::json::parse(response);
        if (!response_data.is_array() || response_data.empty()) {
            LOG_ERROR("Invalid response from GitHub API: expected non-empty array.");
            return;
        }

        nlohmann::json latest_release;
        for (auto& release : response_data) {
            if (!release.is_object()) {
                Logger.Warn("Skipping invalid release data in API response.");
                continue;
            }
            if (!release.value("prerelease", true)) {
                latest_release = release;
                break;
            }
        }

        if (latest_release.empty()) {
            Logger.Warn("No stable releases found in GitHub API response.");
            return;
        }

        if (!latest_release.contains("tag_name")) {
            LOG_ERROR("Latest release missing required 'tag_name' field.");
            return;
        }

        std::string current_version;
        try {
            current_version = ParseVersion(MILLENNIUM_VERSION);
        } catch (...) {
            LOG_ERROR("Failed to parse current Millennium version.");
            return;
        }

        std::string latest_version;
        try {
            latest_version = ParseVersion(latest_release["tag_name"].get<std::string>());
        } catch (...) {
            LOG_ERROR("Failed to parse latest release version.");
            return;
        }

        // Semver comparison using the extracted Semver module
        int compare = Semver::Compare(current_version, latest_version);

        // Single critical section for all shared state updates
        {
            std::lock_guard<std::mutex> lock(_mutex);
            __latest_version = latest_release;

            if (compare == -1) {
                __has_updates = true;
                Logger.Log("New version available: " + latest_release["tag_name"].get<std::string>());
            } else if (compare == 1) {
                Logger.Warn("Millennium is ahead of the latest release.");
                __has_updates = false;
            } else {
                Logger.Log("Millennium is up to date.");
                __has_updates = false;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Unexpected error while checking for updates: {}. Received stream: {}", e.what(), response);

        // Reset state on error to avoid inconsistent state
        std::lock_guard<std::mutex> lock(_mutex);
        __has_updates = false;
        __latest_version = nlohmann::json{};
    }
}

std::optional<nlohmann::json> MillenniumUpdater::FindAsset(const nlohmann::json& release_info)
{
    if (!release_info.contains("assets"))
        return std::nullopt;

    std::string platform_suffix;
#ifdef _WIN32
    platform_suffix = "windows-x86_64.zip";
#elif __linux__
    platform_suffix = "linux-x86_64.tar.gz";
#else
    Logger.Error("Invalid platform, cannot find platform-specific assets.");
    return std::nullopt;
#endif

    std::string target_name = "millennium-" + release_info.value("tag_name", "") + "-" + platform_suffix;
    for (auto& asset : release_info["assets"]) {
        if (asset.value("name", "") == target_name)
            return asset;
    }
    return std::nullopt;
}

nlohmann::json MillenniumUpdater::HasAnyUpdates()
{
    std::lock_guard<std::mutex> lock(_mutex);
    nlohmann::json result;
    result["updateInProgress"] = hasUpdatedMillennium.load();
    result["hasUpdate"] = __has_updates;

    // Make a deep copy of the latest version to avoid race conditions
    nlohmann::json latest_version_copy = __latest_version;
    result["newVersion"] = latest_version_copy;

    auto asset = latest_version_copy.is_null() ? std::nullopt : FindAsset(latest_version_copy);
    result["platformRelease"] = asset.has_value() ? asset.value() : nullptr;
    return result;
}

void MillenniumUpdater::CleanupMillenniumUpdaterTempFiles()
{
    std::error_code ec;
    const auto temp_path = std::filesystem::temp_directory_path() / MILLENNIUM_UPDATER_TEMP_DIR;

    if (std::filesystem::exists(temp_path, ec)) {
        std::filesystem::remove_all(temp_path, ec);
        if (ec) {
            Logger.Warn("Failed to clean up temporary directory {}: {}", temp_path.string(), ec.message());
        } else {
            Logger.Log("Cleaned up temporary directory: {}", temp_path.string());
        }
    }
}

void MillenniumUpdater::DeleteOldMillenniumVersion()
{
#ifdef _WIN32
    const std::vector<std::string> target_files = {
        "millennium.dll", /** Millennium main library */
        "user32.dll",     /** legacy shim */
        "version.dll",    /** new shim */
        "python311.dll"   /** embedded Python runtime */
    };

    const auto steam_path = SystemIO::GetSteamPath();

    if (steam_path.empty()) {
        Logger.Warn("Steam path not found, skipping old Millennium cleanup.");
        return;
    }

    /** Make temporary directory for old Millennium files */
    const auto temp_path = std::filesystem::temp_directory_path() / MILLENNIUM_UPDATER_TEMP_DIR;

    std::error_code ec;
    if (!std::filesystem::create_directories(temp_path, ec)) {
        Logger.Warn("Failed to create temporary directory for moving old Millennium files: {}", ec.message());
        return;
    }

    bool any_files_moved = false;

    for (const auto& filename : target_files) {
        const auto source_path = steam_path / filename;

        if (!std::filesystem::exists(source_path, ec)) {
            if (ec) {
                Logger.Warn("Error checking existence of {}: {}", source_path.string(), ec.message());
            }
            continue;
        }

        const auto dest_path = temp_path / fmt::format("{}.uuid{}.tmp", filename, GenerateUUID());
        const auto source_wide = source_path.wstring();
        const auto dest_wide = dest_path.wstring();

        /** Move file to temporary location */
        if (!MoveFileExW(source_wide.c_str(), dest_wide.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            const DWORD error = GetLastError();
            Logger.Warn("Failed to move {} to temporary location. Error: {}", source_path.string(), error);
            continue;
        }

        Logger.Log("Successfully moved old file {} to {} and queued for deletion", source_path.string(), dest_path.string());
        any_files_moved = true;
    }

    /** No files were moved, so delete the temporary directory */
    if (!any_files_moved) {
        std::filesystem::remove_all(temp_path, ec);
        if (ec) {
            Logger.Warn("Failed to clean up empty temporary directory {}: {}", temp_path.string(), ec.message());
        }
    }

    Logger.Log("Old Millennium version cleanup completed. Files moved: {}", any_files_moved);
#endif
}

void RateLimitedLogger(const std::string& message, const double& progress)
{
    static auto last_call = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_call).count() > 1) {
        IpcForwardInstallLog({ message, progress, false });
        last_call = now;
    }
}

void MillenniumUpdater::Co_BeginUpdate(const std::string& downloadUrl, const size_t downloadSize)
{
    try {
        /** Windows locks files when in use, so we need to use tricks to "unlock" them */
        DeleteOldMillenniumVersion();

        const auto tempFilePath = std::filesystem::temp_directory_path() / fmt::format("millennium-{}.zip", GenerateUUID());

        Http::DownloadWithProgress({ downloadUrl, downloadSize }, tempFilePath, [](size_t downloaded, size_t total)
        {
            const double progress = (static_cast<double>(downloaded) / total) * 50.0;
            RateLimitedLogger("Downloading update assets...", progress);
        });

        Util::ExtractZipArchive(tempFilePath.string(), SystemIO::GetInstallPath().generic_string(), [](int current, int total, const char* file)
        {
            const double progress = 50.0 + (static_cast<double>(current) / total) * 50.0;
            RateLimitedLogger(fmt::format("Processing update file {}/{}", current, total), progress);
        });

        /** Remove the temporary file after installation */
        const auto result = std::filesystem::remove(tempFilePath);
        if (result) {
            Logger.Log("Removed temporary file: " + tempFilePath.string());
        } else {
            Logger.Warn("Failed to remove temporary file: " + tempFilePath.string());
        }

        Logger.Log("Update completed successfully.");
    } catch (const std::exception& e) {
        LOG_ERROR("Update failed with exception: {}", e.what());
    } catch (...) {
        LOG_ERROR("Update failed with unknown exception.");
    }

    // Always reset the update flag when done, regardless of success or failure
    hasUpdatedMillennium.store(false);
    IpcForwardInstallLog({ "Successfully updated Millennium!", 100.0f, true });
}

void MillenniumUpdater::StartUpdate(const std::string& downloadUrl, const size_t downloadSize, bool background)
{
    // Use mutex to ensure only one update can start at a time
    std::lock_guard<std::mutex> lock(update_mutex);

    if (hasUpdatedMillennium.load()) {
        Logger.Warn("Update already in progress, aborting new update request.");
        return;
    }

    hasUpdatedMillennium.store(true);

    if (background) {
        Logger.Log("Starting Millennium update in background thread...");
        /** Start the update process in a separate thread */
        std::thread updateThread(Co_BeginUpdate, downloadUrl, downloadSize);
        updateThread.detach();
        return;
    }

    Logger.Log("Starting Millennium update in blocking mode...");
    /** Run the update process in the current thread (blocking) */
    Co_BeginUpdate(downloadUrl, downloadSize);
    Logger.Log("Millennium update process finished.");
}