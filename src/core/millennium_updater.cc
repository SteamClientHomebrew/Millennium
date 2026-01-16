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

#include "millennium/millennium_updater.h"
#include "millennium/semver.h"
#include "millennium/http.h"
#include "millennium/sysfs.h"
#include "millennium/logger.h"
#include "millennium/encode.h"
#include "millennium/zip.h"
#include "head/ipc_handler.h"

#include <filesystem>
#include <exception>
#include <memory>
#include <thread>
#include <chrono>

#define SHIM_LOADER_PATH "user32.dll"
#define SHIM_LOADER_QUEUED_PATH "user32.queue.dll" // The path of the recently updated shim loader waiting for an update.

#define MILLENNIUM_UPDATER_TEMP_DIR "millennium-updater-temp-files"
#define MILLENNIUM_UPDATER_LEGACY_SHIM_TEMP_DIR "millennium-updater-legacy-shim-temp"

/**
 * Uncomment to enable dry-run mode (skips actual download/extraction).
 * This is VERY helpful when debugging this shit ass updater
 */

// #define UPDATER_DEBUG_MODE

#ifdef UPDATER_DEBUG_MODE
#ifndef _DEBUG
#error "UPDATER_DEBUG_MODE can only be enabled in debug builds"
#endif
#define DEBUG_LOG(...) Logger.Log(__VA_ARGS__)
#else
#define DEBUG_LOG(...) ((void)0)
#endif

static std::atomic<bool> hasUpdatedMillennium{ false }; /** Tracks if update has completed successfully */
static std::atomic<bool> updateInProgress{ false };     /** Tracks if update is currently running */
static std::mutex update_mutex;                         /** Protects the update process */
static std::shared_ptr<std::thread> update_thread;      /** Managed background thread */

class UpdateProgressGuard
{
  public:
    UpdateProgressGuard()
    {
        updateInProgress.store(true);
    }
    ~UpdateProgressGuard()
    {
        updateInProgress.store(false);
    }
    UpdateProgressGuard(const UpdateProgressGuard&) = delete;
    UpdateProgressGuard& operator=(const UpdateProgressGuard&) = delete;
};

std::string MillenniumUpdater::ParseVersion(const std::string& version)
{
    if (!version.empty() && version[0] == 'v') return version.substr(1);
    return version;
}

void MillenniumUpdater::CheckForUpdates()
{
    #ifdef DISTRO_NIX
        Logger.Log("Skipping update check on Nix-based releases");
        return;
    #endif
    
    const bool checkForUpdates = CONFIG.GetNested("general.checkForMillenniumUpdates", true).get<bool>();
    std::string channel = CONFIG.GetNested("general.millenniumUpdateChannel", "stable").get<std::string>();

    if (!checkForUpdates) {
        Logger.Warn("User has disabled update checking for Millennium.");
        return;
    }

    Logger.Log("Checking for updates on {} channel...", channel);
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

            if (channel == "beta") {
                latest_release = release;
                break;
            }

            if (!release.value("prerelease", true)) {
                latest_release = release;
                break;
            }
        }

        Logger.Log("Latest Millennium release: {}", latest_release.value("tag_name", "unknown"));

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

        int compare = Semver::Compare(current_version, latest_version);

        {
            std::lock_guard<std::mutex> lock(_mutex);
            __latest_version = latest_release;

            if (compare == -1) {
                __has_updates = true;
                Logger.Log("New version available: " + latest_release["tag_name"].get<std::string>());
            } else if (compare == 1) {
                Logger.Warn("Current version {} is newer than latest release {}.", current_version, latest_version);
                __has_updates = false;
            } else {
                Logger.Log("Millennium is up to date.");
                __has_updates = false;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Unexpected error while checking for updates: {}. Received stream: {}", e.what(), response);

        /** Reset state on error to avoid inconsistent state */
        std::lock_guard<std::mutex> lock(_mutex);
        __has_updates = false;
        __latest_version = nlohmann::json{};
    }
}

std::optional<nlohmann::json> MillenniumUpdater::FindAsset(const nlohmann::json& release_info)
{
    if (!release_info.contains("assets")) return std::nullopt;

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
        if (asset.value("name", "") == target_name) return asset;
    }
    return std::nullopt;
}

nlohmann::json MillenniumUpdater::HasAnyUpdates()
{
    std::lock_guard<std::mutex> lock(_mutex);
    nlohmann::json result;
    result["updateInProgress"] = hasUpdatedMillennium.load();
    result["hasUpdate"] = __has_updates;

    /** Make a deep copy of the latest version to avoid race conditions */
    nlohmann::json latest_version_copy = __latest_version;

    /** Find asset while still holding the lock to ensure consistency */
    auto asset = latest_version_copy.is_null() ? std::nullopt : FindAsset(latest_version_copy);

    result["newVersion"] = latest_version_copy;
    result["platformRelease"] = asset.has_value() ? asset.value() : nullptr;

    return result;
}

void MillenniumUpdater::CleanupMillenniumUpdaterTempFiles()
{
    std::filesystem::path steam_path = SystemIO::GetSteamPath();

    std::error_code ec;
    std::filesystem::path temp_path = steam_path / MILLENNIUM_UPDATER_TEMP_DIR;

    if (std::filesystem::exists(temp_path, ec)) {
        std::filesystem::remove_all(temp_path, ec);
        if (ec) {
            Logger.Warn("Failed to clean up temporary directory {}: {}", temp_path.string(), ec.message());
        } else {
            Logger.Log("Cleaned up temporary directory: {}", temp_path.string());
        }
    }

    try {
        /** find all files in the steam directory that end with .tmp and contain uuid in the filename */
        for (const auto& entry : std::filesystem::directory_iterator(steam_path)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            std::string filename = entry.path().filename().string();
            if (filename.size() > 8 && filename.substr(filename.size() - 4) == ".tmp" && filename.find("uuid") != std::string::npos) {
                Logger.Log("Removing leftover temporary file: {}", entry.path().string());
                std::filesystem::remove(entry.path(), ec);
                if (ec) {
                    Logger.Warn("Failed to remove temporary file {}: {}", entry.path().string(), ec.message());
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& fe) {
        Logger.Warn("Filesystem error during directory iteration: {}", fe.what());
    } catch (const std::exception& e) {
        Logger.Warn("Exception during directory iteration: {}", e.what());
    }
}

void MillenniumUpdater::DeleteOldMillenniumVersion([[maybe_unused]] std::vector<std::string> lockedFiles)
{
#ifdef _WIN32
    std::filesystem::path steam_path = SystemIO::GetSteamPath();

    if (steam_path.empty()) {
        Logger.Warn("Steam path not found, skipping old Millennium cleanup.");
        return;
    }

    /** Make temporary directory for old Millennium files */
    std::filesystem::path temp_path = steam_path / MILLENNIUM_UPDATER_TEMP_DIR;
    Logger.Log("Using temporary directory for moving old Millennium files: {}", temp_path.string());

    std::error_code ec;
    std::filesystem::create_directories(temp_path, ec);
    if (ec) {
        Logger.Warn("Failed to create temporary directory for moving old Millennium files: {}", ec.message());
        return;
    }

    bool any_files_moved = false;

    for (const auto& filename : lockedFiles) {
        std::filesystem::path source_path = steam_path / filename;

        Logger.Log("Processing old file: {}", source_path.string());

        if (!std::filesystem::exists(source_path, ec)) {
            if (ec) {
                Logger.Warn("Error checking existence of {}: {}", source_path.string(), ec.message());
            }
            continue;
        }

        std::string base_filename = source_path.filename().string();
        std::string temp_file_name = fmt::format("{}.uuid{}.tmp", base_filename, GenerateUUID());

        std::filesystem::path dest_path = temp_path / temp_file_name;
        std::filesystem::path source_wide = source_path.wstring();
        std::filesystem::path dest_wide = dest_path.wstring();

        Logger.Log("Moving old Millennium file {} to temporary location {}", source_path.string(), dest_path.string());

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

void RateLimitedLogger(const std::string& message, const double& progress, bool forwardToIpc)
{
    static std::mutex rate_limit_mutex;
    static auto last_call = std::chrono::steady_clock::now() - std::chrono::seconds(2);

    std::lock_guard<std::mutex> lock(rate_limit_mutex);
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_call).count() > 1) {

        if (forwardToIpc)
            IpcForwardInstallLog({ message, progress, false });
        else
            Logger.Log("{}", message);

        last_call = now;
    }
}

void MillenniumUpdater::Co_BeginUpdate(const std::string& downloadUrl, const size_t downloadSize, bool forwardToIpc)
{
    /** RAII guard ensures progress flag is reset even on exception */
    UpdateProgressGuard progressGuard;

    std::filesystem::path tempFilePath;

    try {
#ifdef UPDATER_DEBUG_MODE
        DEBUG_LOG("[DEBUG] === DRY RUN MODE ENABLED ===");
        DEBUG_LOG("[DEBUG] Would download from: {}", downloadUrl);
        DEBUG_LOG("[DEBUG] Expected size: {} bytes", downloadSize);

        tempFilePath = std::filesystem::temp_directory_path() / fmt::format("millennium-{}.zip", GenerateUUID());
        DEBUG_LOG("[DEBUG] Would download to: {}", tempFilePath.string());

        /** Simulate download progress */
        for (int i = 0; i <= 10; ++i) {
            const double progress = (i / 10.0) * 50.0;
            RateLimitedLogger("Downloading update assets... (DRY RUN)", progress, forwardToIpc);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

#ifdef _WIN32
        DEBUG_LOG("[DEBUG] Would check for locked files on Windows");
        DEBUG_LOG("[DEBUG] Would move old Millennium files to temp location");
#endif

        DEBUG_LOG("[DEBUG] Would extract to: {}", SystemIO::GetInstallPath().generic_string());

        /** Simulate extraction progress */
        for (int i = 0; i <= 10; ++i) {
            const double progress = 50.0 + (i / 10.0) * 50.0;
            RateLimitedLogger(fmt::format("Processing update file {}/{} (DRY RUN)", i, 10), progress, forwardToIpc);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        DEBUG_LOG("[DEBUG] Dry run completed successfully - no files were actually modified");
        Logger.Log("Update completed successfully (DRY RUN - no actual changes made).");

#else
        tempFilePath = std::filesystem::temp_directory_path() / fmt::format("millennium-{}.zip", GenerateUUID());
        Logger.Log("Downloading update to temporary file: " + tempFilePath.string());

        Http::DownloadWithProgress({ downloadUrl, downloadSize }, tempFilePath, [forwardToIpc](size_t downloaded, size_t total)
        {
            const double progress = (static_cast<double>(downloaded) / total) * 50.0;
            RateLimitedLogger("Downloading update assets...", progress, forwardToIpc);
        });

#ifdef _WIN32
        /** Windows locks files when in use, so we need to use tricks to "unlock" them */
        std::vector<std::string> lockedFiles = Util::GetLockedFiles(tempFilePath.string(), SystemIO::GetInstallPath().generic_string());
        DeleteOldMillenniumVersion(lockedFiles);
#endif

        Logger.Log("Extracting and installing update to {}", SystemIO::GetInstallPath().generic_string());
        Util::ExtractZipArchive(tempFilePath.string(), SystemIO::GetInstallPath().generic_string(), [forwardToIpc](int current, int total, const char*)
        {
            const double progress = 50.0 + (static_cast<double>(current) / total) * 50.0;
            RateLimitedLogger(fmt::format("Processing update file {}/{}", current, total), progress, forwardToIpc);
        });

        /** Remove the temporary file after installation */
        std::error_code ec;
        if (std::filesystem::remove(tempFilePath, ec)) {
            Logger.Log("Removed temporary file: " + tempFilePath.string());
        } else {
            Logger.Warn("Failed to remove temporary file: {}, error: {}", tempFilePath.string(), ec.message());
        }

        Logger.Log("Update completed successfully.");
#endif

        /** Mark that update has completed successfully */
        hasUpdatedMillennium.store(true);

        if (forwardToIpc) IpcForwardInstallLog({ "Successfully updated Millennium!", 100.0f, true });

    } catch (const std::exception& e) {
        LOG_ERROR("Update failed with exception: {}", e.what());

        /** Reset update state on failure */
        hasUpdatedMillennium.store(false);

        if (forwardToIpc) IpcForwardInstallLog({ fmt::format("Update failed: {}", e.what()), 0.0f, true });

        /** Clean up temp file on failure */
#ifndef UPDATER_DEBUG_MODE
        if (!tempFilePath.empty() && std::filesystem::exists(tempFilePath)) {
            std::error_code ec;
            std::filesystem::remove(tempFilePath, ec);
            if (ec) {
                Logger.Warn("Failed to clean up temp file after error: {}", ec.message());
            }
        }
#endif
        throw;
    } catch (...) {
        LOG_ERROR("Update failed with unknown exception.");

        /** Reset update state on failure */
        hasUpdatedMillennium.store(false);

        if (forwardToIpc) IpcForwardInstallLog({ "Update failed with unknown error", 0.0f, true });

        /** Clean up temp file on failure */
#ifndef UPDATER_DEBUG_MODE
        if (!tempFilePath.empty() && std::filesystem::exists(tempFilePath)) {
            std::error_code ec;
            std::filesystem::remove(tempFilePath, ec);
            if (ec) {
                Logger.Warn("Failed to clean up temp file after error: {}", ec.message());
            }
        }
#endif
        throw;
    }

    /** UpdateProgressGuard destructor will reset updateInProgress here */
}

bool MillenniumUpdater::HasPendingRestart()
{
    return hasUpdatedMillennium.load();
}

void MillenniumUpdater::StartUpdate(const std::string& downloadUrl, const size_t downloadSize, bool background, bool forwardToIpc)
{
    /** Use mutex to ensure only one update can start at a time */
    std::lock_guard<std::mutex> lock(update_mutex);

    if (updateInProgress.load()) {
        Logger.Warn("Update already in progress, aborting new update request.");
        return;
    }

    /** Join previous background thread if it exists and has finished */
    if (update_thread && update_thread->joinable()) {
        Logger.Log("Waiting for previous background update thread to complete...");
        update_thread->join();
        update_thread.reset();
    }

    /** Reset the update completion flag for new update */
    hasUpdatedMillennium.store(false);

    if (background) {
        Logger.Log("Starting Millennium update in background thread...");

        /** Start the update process in a managed thread */
        update_thread = std::make_shared<std::thread>([downloadUrl, downloadSize, forwardToIpc]()
        {
            try {
                Co_BeginUpdate(downloadUrl, downloadSize, forwardToIpc);
            } catch (const std::exception& e) {
                LOG_ERROR("Background update thread caught exception: {}", e.what());
            } catch (...) {
                LOG_ERROR("Background update thread caught unknown exception");
            }
        });

        return;
    }

    Logger.Log("Starting Millennium update in blocking mode...");
    /** Run the update process in the current thread (blocking) */
    try {
        Co_BeginUpdate(downloadUrl, downloadSize, forwardToIpc);
        Logger.Log("Millennium update process finished.");
    } catch (const std::exception& e) {
        LOG_ERROR("Blocking update caught exception: {}", e.what());
    } catch (...) {
        LOG_ERROR("Blocking update caught unknown exception");
    }
}

void MillenniumUpdater::Shutdown()
{
    /** Wait for background update thread to complete on shutdown */
    std::lock_guard<std::mutex> lock(update_mutex);

    if (update_thread && update_thread->joinable()) {
        Logger.Log("Waiting for background update thread to complete before shutdown...");
        update_thread->join();
        update_thread.reset();
    }
}

void MillenniumUpdater::UpdateLegacyUser32Shim()
{
#ifdef _WIN32
    try {
        if (!std::filesystem::exists(SystemIO::GetInstallPath() / SHIM_LOADER_QUEUED_PATH)) {
            Logger.Log("No queued shim loader found...");
            return;
        }

        Logger.Log("Updating shim module from cache...");
        const auto oldShimPath = SystemIO::GetInstallPath() / SHIM_LOADER_PATH;

        if (std::filesystem::exists(oldShimPath)) {
            const auto tempPath = SystemIO::GetInstallPath() / MILLENNIUM_UPDATER_LEGACY_SHIM_TEMP_DIR;

            std::error_code ec;
            std::filesystem::create_directories(tempPath, ec);
            if (ec) {
                Logger.Warn("Failed to create temporary directory: {}", ec.message());
            }

            const auto destPath = tempPath / fmt::format("{}.uuid{}.tmp", SHIM_LOADER_PATH, GenerateUUID());

            if (!MoveFileExW(oldShimPath.wstring().c_str(), destPath.wstring().c_str(), MOVEFILE_REPLACE_EXISTING)) {
                const DWORD error = GetLastError();
                throw std::runtime_error(fmt::format("Failed to move old shim to temp location. Error: {}", error));
            }

            Logger.Log("Moved old shim to temporary location: {}", destPath.string());
        }

        std::filesystem::rename(SystemIO::GetInstallPath() / SHIM_LOADER_QUEUED_PATH, SystemIO::GetInstallPath() / SHIM_LOADER_PATH);
        Logger.Log("Successfully updated {}!", SHIM_LOADER_PATH);

    } catch (std::exception& e) {
        LOG_ERROR("Failed to update {}: {}", SHIM_LOADER_PATH, e.what());
        MessageBoxA(NULL, fmt::format("Failed to update {}, it's recommended that you reinstall Millennium.", SHIM_LOADER_PATH).c_str(), "Oops!", MB_ICONERROR | MB_OK);
    }
#endif
}