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
#include "millennium/core_ipc.h"
#include "millennium/semver.h"
#include "millennium/http.h"
#include "millennium/sysfs.h"
#include "millennium/logger.h"
#include "millennium/encode.h"
#include "millennium/zip.h"

#include <filesystem>
#include <exception>
#include <memory>

#define SHIM_LOADER_PATH "user32.dll"
#define SHIM_LOADER_QUEUED_PATH "user32.queue.dll"

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

millennium_updater::millennium_updater() : m_thread_pool(std::make_shared<thread_pool>(1))
{
}

millennium_updater::~millennium_updater()
{
    Logger.Log("millennium_updater destructor, m_ipc_main use_count: {}", m_ipc_main ? m_ipc_main.use_count() : 0);
}

std::string millennium_updater::parse_version(const std::string& version)
{
    if (!version.empty() && version[0] == 'v') return version.substr(1);
    return version;
}

void millennium_updater::check_for_updates()
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
            current_version = parse_version(MILLENNIUM_VERSION);
        } catch (...) {
            LOG_ERROR("Failed to parse current Millennium version.");
            return;
        }

        std::string latest_version;
        try {
            latest_version = parse_version(latest_release["tag_name"].get<std::string>());
        } catch (...) {
            LOG_ERROR("Failed to parse latest release version.");
            return;
        }

        int compare = Semver::Compare(current_version, latest_version);
        {
            std::lock_guard<std::mutex> lock(m_state_mutex);
            m_latest_version = std::move(latest_release);

            if (compare == -1) {
                m_has_updates = true;
                Logger.Log("New version available: " + m_latest_version["tag_name"].get<std::string>());
            } else if (compare == 1) {
                Logger.Warn("Current version {} is newer than latest release {}.", current_version, latest_version);
                m_has_updates = false;
            } else {
                Logger.Log("Millennium is up to date.");
                m_has_updates = false;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Unexpected error while checking for updates: {}. Received stream: {}", e.what(), response);

        // Reset state on error
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_has_updates = false;
        m_latest_version = nlohmann::json{};
    }
}

std::optional<nlohmann::json> millennium_updater::find_asset(const nlohmann::json& release_info)
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

void millennium_updater::dispatch_progress_to_frontend(std::string status, double progress, bool is_complete)
{
    if (!m_ipc_main) {
        return; /** ipc hasn't connected to Steam yet */
    }

    std::vector<ipc_main::javascript_parameter> params = { status, progress, is_complete };
    m_ipc_main->evaluate_javascript_expression(m_ipc_main->compile_javascript_expression("core", "InstallerMessageEmitter", params));
}

json millennium_updater::has_any_updates()
{
    std::lock_guard<std::mutex> lock(m_state_mutex);

    json result;
    result["updateInProgress"] = m_has_updated_millennium;
    result["hasUpdate"] = m_has_updates;
    result["newVersion"] = m_latest_version;

    auto asset = m_latest_version.is_null() ? std::nullopt : find_asset(m_latest_version);
    result["platformRelease"] = asset.has_value() ? asset.value() : nullptr;

    return result;
}

void millennium_updater::cleanup()
{
#ifdef _WIN32
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
        // Find all temp files with uuid in filename
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
#endif
}

void millennium_updater::win32_move_old_millennium_version([[maybe_unused]] std::vector<std::string> lockedFiles)
{
#ifdef _WIN32
    std::filesystem::path steam_path = SystemIO::GetSteamPath();

    if (steam_path.empty()) {
        Logger.Warn("Steam path not found, skipping old Millennium cleanup.");
        return;
    }

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

        if (!MoveFileExW(source_wide.c_str(), dest_wide.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            const DWORD error = GetLastError();
            Logger.Warn("Failed to move {} to temporary location. Error: {}", source_path.string(), error);
            continue;
        }

        Logger.Log("Successfully moved old file {} to {} and queued for deletion", source_path.string(), dest_path.string());
        any_files_moved = true;
    }

    // No files were moved, so delete the temporary directory
    if (!any_files_moved) {
        std::filesystem::remove_all(temp_path, ec);
        if (ec) {
            Logger.Warn("Failed to clean up empty temporary directory {}: {}", temp_path.string(), ec.message());
        }
    }

    Logger.Log("Old Millennium version cleanup completed. Files moved: {}", any_files_moved);
#endif
}

void millennium_updater::update_impl(const std::string& downloadUrl, const size_t downloadSize)
{
    std::filesystem::path tempFilePath;

    auto cleanup_temp_file = [&tempFilePath]()
    {
#ifndef UPDATER_DEBUG_MODE
        if (!tempFilePath.empty() && std::filesystem::exists(tempFilePath)) {
            std::error_code ec;
            std::filesystem::remove(tempFilePath, ec);
            if (ec) {
                Logger.Warn("Failed to clean up temp file: {}", ec.message());
            }
        }
#endif
    };

    auto update_state = [this](bool success)
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_has_updated_millennium = success;
        m_update_in_progress = false;
    };

    try {
        tempFilePath = std::filesystem::temp_directory_path() / fmt::format("millennium-{}.zip", GenerateUUID());

#ifdef UPDATER_DEBUG_MODE
        DEBUG_LOG("[DEBUG] === DRY RUN MODE ENABLED ===");
        DEBUG_LOG("[DEBUG] Would download from: {}", downloadUrl);
        DEBUG_LOG("[DEBUG] Would download to: {}", tempFilePath.string());

        for (int i = 0; i <= 10; ++i) {
            const double progress = (i / 10.0) * 100.0;
            const char* msg = i <= 5 ? "Downloading update assets... (DRY RUN)" : "Processing update files (DRY RUN)";
            this->dispatch_progress_to_frontend(msg, progress, false);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        DEBUG_LOG("[DEBUG] Dry run completed - no files modified");
#else
        Logger.Log("Downloading update to: {}", tempFilePath.string());
        Http::DownloadWithProgress({ downloadUrl, downloadSize }, tempFilePath, [this](size_t downloaded, size_t total)
        {
            const double progress = (static_cast<double>(downloaded) / total) * 50.0;
            this->dispatch_progress_to_frontend("Downloading update assets...", progress, false);
        });

#ifdef _WIN32
        auto lockedFiles = Util::GetLockedFiles(tempFilePath.string(), SystemIO::GetInstallPath().generic_string());
        win32_move_old_millennium_version(lockedFiles);
#endif
        Logger.Log("Extracting to {}", SystemIO::GetInstallPath().generic_string());
        Util::ExtractZipArchive(tempFilePath.string(), SystemIO::GetInstallPath().generic_string(), [this](int current, int total, const char*)
        {
            const double progress = 50.0 + (static_cast<double>(current) / total) * 50.0;
            this->dispatch_progress_to_frontend(fmt::format("Processing file {}/{}", current, total), progress, false);
        });

        std::error_code ec;
        if (!std::filesystem::remove(tempFilePath, ec)) {
            Logger.Warn("Failed to remove temp file: {}", ec.message());
        }
#endif
        Logger.Log("Update completed successfully.");
        this->dispatch_progress_to_frontend("Successfully updated Millennium!", 100.0f, true);
        update_state(true);

    } catch (const std::exception& e) {
        LOG_ERROR("Update failed: {}", e.what());
        this->dispatch_progress_to_frontend(fmt::format("Update failed: {}", e.what()), 0.0f, true);
        cleanup_temp_file();
        update_state(false);
        throw;
    } catch (...) {
        LOG_ERROR("Update failed with unknown exception.");
        this->dispatch_progress_to_frontend("Update failed with unknown error", 0.0f, true);
        cleanup_temp_file();
        update_state(false);
        throw;
    }
}

bool millennium_updater::is_pending_restart()
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_has_updated_millennium;
}

void millennium_updater::update(const std::string& downloadUrl, const size_t downloadSize, bool background)
{
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);

        if (m_update_in_progress) {
            Logger.Warn("Update already in progress, aborting new update request.");
            return;
        }

        if (m_has_updated_millennium) {
            Logger.Warn("Update already completed, restart pending. Aborting new update request.");
            return;
        }

        m_update_in_progress = true;
    }

    const auto start_update = [this, downloadUrl, downloadSize]()
    {
        try {
            update_impl(downloadUrl, downloadSize);
        } catch (const std::exception& e) {
            LOG_ERROR("Background update thread caught exception: {}", e.what());
        } catch (...) {
            LOG_ERROR("Background update thread caught unknown exception");
        }
    };

    if (background) {
        Logger.Log("Starting Millennium update in background thread...");
        m_thread_pool->enqueue(start_update);
        return;
    }
    start_update();
}

void millennium_updater::shutdown()
{
    Logger.Log("Successfully shut down millennium_updater...");
}

void millennium_updater::set_ipc_main(std::shared_ptr<ipc_main> ipc_main)
{
    this->m_ipc_main = std::move(ipc_main);
}

void millennium_updater::win32_update_legacy_shims()
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
