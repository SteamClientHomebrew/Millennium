/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
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

#ifdef _WIN32
#include "millennium/millennium.h"
#include "millennium/filesystem.h"
#include "millennium/cmdline_api.h"
#include "millennium/plat_msg.h"
#include "millennium/environment.h"
#include "millennium/encoding.h"
#include "millennium/steam_hooks.h"
#include "millennium/plat_msg.h"
#include "shared/crash_handler.h"

#include <thread>

namespace fs = std::filesystem;

/** forward declare function */
std::thread g_millenniumThread;

/**
 * Setup environment variables used throughout Millennium.
 * These are vital to be setup, if they aren't Millennium and its loaded plugins will likely fail to start/run
 */
CONSTRUCTOR VOID Win32_InitializeEnvironment(VOID)
{
    try {
        platform::environment::setup();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to set up environment variables: {}", e.what());
        std::exit(EXIT_FAILURE);
    }
}

#include "millennium/millennium_lifecycle.h"

BOOL AreFilesIdentical(LPCWSTR path1, LPCWSTR path2)
{
    HANDLE h1 = CreateFileW(path1, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h1 == INVALID_HANDLE_VALUE) return FALSE;

    HANDLE h2 = CreateFileW(path2, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h2 == INVALID_HANDLE_VALUE) {
        CloseHandle(h1);
        return FALSE;
    }

    BY_HANDLE_FILE_INFORMATION info1, info2;
    BOOL same = FALSE;

    if (GetFileInformationByHandle(h1, &info1) && GetFileInformationByHandle(h2, &info2)) {
        same = (info1.dwVolumeSerialNumber == info2.dwVolumeSerialNumber) && (info1.nFileIndexHigh == info2.nFileIndexHigh) && (info1.nFileIndexLow == info2.nFileIndexLow);
    }

    CloseHandle(h1);
    CloseHandle(h2);
    return same;
}

/**
 * Initialize Millennium webhelper hook by hardlinking it into the cef bin directories.
 */
VOID Win32_AttachWebHelperHook(VOID)
{
    const auto hookPath = platform::get_millennium_path() / "lib" / "millennium.hhx64.dll";

    if (!std::filesystem::exists(hookPath)) {
        platform::messagebox::show("Millennium Error", "Millennium webhelper hook is missing. Please reinstall Millennium.", platform::messagebox::error);
        return;
    }

    const fs::path cefDirs[] = {
        platform::get_steam_path() / "bin" / "cef" / "cef.win7x64",
        platform::get_steam_path() / "bin" / "cef" / "cef.win64",
    };

    for (const auto& cefDir : cefDirs) {
        if (!std::filesystem::exists(cefDir)) {
            continue;
        }

        const auto targetPath = cefDir / "version.dll";

        if (!AreFilesIdentical(hookPath.wstring().c_str(), targetPath.wstring().c_str())) {
            DeleteFileW(targetPath.wstring().c_str());
        }

        if (std::filesystem::exists(targetPath)) {
            continue;
        }

        BOOL result = CreateHardLinkW(targetPath.wstring().c_str(), hookPath.wstring().c_str(), NULL);
        if (!result) {
            platform::messagebox::show(
                "Millennium Error",
                std::format("Failed to create hardlink for Millennium webhelper hook.\nTarget: {}\nError Code: {}\nMake sure Steam is not running and try again.",
                            targetPath.string(), GetLastError())
                    .c_str(),
                platform::messagebox::error);
        }
    }
}

VOID Win32_MoveVersionHook(VOID)
{
    const auto versionHookPath = platform::get_steam_path() / "version.dll";
    const auto targetPath = platform::get_steam_path() / "millennium-legacy.version.dll";

    if (!std::filesystem::exists(versionHookPath)) {
        return;
    }

    if (!MoveFileExW(versionHookPath.wstring().c_str(), targetPath.wstring().c_str(), MOVEFILE_REPLACE_EXISTING)) {
        const DWORD error = GetLastError();
        platform::messagebox::show("Millennium Error", std::format("Failed to move legacy version.dll hook.\nError Code: {}", error).c_str(), platform::messagebox::error);
    }
}

/* move a single file to a temp directory. Uses MoveFileExW so it can handle in-use files.*/
static void move_to_temp(const fs::path& file, const fs::path& temp_dir)
{
    std::error_code ec;
    if (!fs::exists(file, ec)) return;

    fs::path dest = temp_dir / std::format("{}.{}.tmp", file.filename().string(), GenerateUUID());
    if (!MoveFileExW(file.wstring().c_str(), dest.wstring().c_str(), MOVEFILE_REPLACE_EXISTING)) {
        logger.warn("Migration: could not move {} (error: {})", file.string(), GetLastError());
    } else {
        logger.log("Migration: moved {} -> temp", file.filename().string());
    }
}

/**
 * move each entry (file, dir, or symlink) from src_dir into dst_dir individually.
 * moves symlinks/junctions as-is (no admin rights needed).
 */
static void move_directory_entries(const fs::path& src_dir, const fs::path& dst_dir)
{
    std::error_code ec;
    if (!fs::exists(src_dir, ec) || !fs::is_directory(src_dir, ec)) return;

    for (const auto& entry : fs::directory_iterator(src_dir, ec)) {
        if (ec) break;

        fs::path dest = dst_dir / entry.path().filename();
        if (fs::exists(dest, ec)) continue;

        fs::rename(entry.path(), dest, ec);
        if (ec) {
            logger.warn("Migration: failed to move {}: {}", entry.path().filename().string(), ec.message());
            ec.clear();
        }
    }
}

/**
 * migrate the old flat <steam>/ext/ layout to the new <steam>/millennium/ structure.
 * runs once, skipped if ext/ is already gone or millennium/ is already populated.
 */
static VOID Win32_MigrateLegacyLayout(VOID)
{
    try {
        const auto steam = platform::get_steam_path();
        const auto millennium = platform::get_millennium_path();

        std::error_code ec;
        const bool hasLegacyExt = fs::exists(steam / "ext", ec);
        const bool hasLegacyPlugins = fs::exists(steam / "plugins", ec);
        const bool hasLegacySkins = fs::exists(steam / "steamui" / "skins", ec);

        if (!hasLegacyExt && !hasLegacyPlugins && !hasLegacySkins) return;

        logger.log("Migration: legacy layout detected, migrating...");

        /** create directory skeleton */
        for (const auto& dir : { "ext/data/assets", "ext/data/shims", "plugins", "logs", "themes", "config", "crashes" }) {
            fs::create_directories(millennium / dir, ec);
        }

        /** move data directories */
        auto safe_rename = [&](const fs::path& src, const fs::path& dst)
        {
            if (fs::exists(src, ec) && !fs::exists(dst, ec)) {
                fs::rename(src, dst, ec);
                if (ec) {
                    logger.warn("Migration: rename {} failed: {}", src.string(), ec.message());
                    ec.clear();
                }
            }
        };

        safe_rename(steam / "ext" / "data" / "assets", millennium / "ext" / "data" / "assets");
        safe_rename(steam / "ext" / "data" / "shims", millennium / "ext" / "data" / "shims");

        /** move plugins and themes (entry-by-entry so symlinks are preserved) */
        move_directory_entries(steam / "plugins", millennium / "plugins");
        move_directory_entries(steam / "steamui" / "skins", millennium / "themes");

        /** move logs */
        move_directory_entries(steam / "ext" / "logs", millennium / "logs");

        /** move config files */
        safe_rename(steam / "ext" / "config.json", millennium / "config" / "config.json");
        safe_rename(steam / "ext" / "quickcss.css", millennium / "config" / "quick.css");

        /** move legacy DLLs to temp (they may be loaded, so move rather than delete) */
        fs::path temp_dir = steam / "millennium-migration-temp";
        fs::create_directories(temp_dir, ec);

        const fs::path legacy_files[] = {
            steam / "user32.dll",
            steam / "version.dll",
            steam / "ext" / "compat32" / "millennium_x86.dll",
            steam / "ext" / "compat32" / "python311.dll",
            steam / "ext" / "compat64" / "millennium_x64.dll",
            steam / "ext" / "compat64" / "python311.dll",
            steam / "millennium.hhx64.dll",
            steam / "millennium.dll",
            steam / "python311.dll",
            steam / "millennium-legacy.version.dll",
        };

        for (const auto& file : legacy_files) {
            move_to_temp(file, temp_dir);
        }

        /** clean up legacy directories */
        if (fs::exists(steam / "ext", ec)) {
            fs::rename(steam / "ext", temp_dir / "ext", ec);
            if (ec) {
                logger.warn("Migration: could not move ext/ to temp: {}", ec.message());
                ec.clear();
            }
        }
        fs::remove_all(steam / "plugins", ec);
        fs::remove_all(steam / "steamui" / "skins", ec);

        logger.log("Migration: completed.");
    } catch (const std::exception& e) {
        LOG_ERROR("Migration failed: {}", e.what());
    }
}

VOID Win32_AttachMillennium(VOID)
{
    if (!platform::initialize_steam_hooks()) {
        platform::messagebox::show("Millennium Error", "Failed to initialize Steam hooks, Millennium cannot continue startup.", platform::messagebox::error);
    }

    install_millennium_crash_handler();
    Win32_MigrateLegacyLayout();
    Win32_MoveVersionHook();

    g_millennium = std::make_unique<millennium>();

    Win32_AttachWebHelperHook();
    platform::environment::setup();

    g_millennium->entry();
    logger.log("[Win32_AttachMillennium] Millennium main function has returned, proceeding with shutdown...");

    uninitialize_steam_hooks();
    /** Deallocate the developer console */
    if (CommandLineArguments::has_argument("-dev")) {
        FreeConsole();
    }
}

/**
 * Cleans up resources and gracefully shuts down Millennium on Windows.
 *
 * @warning This cleanup function assumes EntryMain has returned with success, otherwise this detach function will deadlock.
 * It depends on Lua being uninitialized with all threads terminated, and all frontend hooks detached.
 */
VOID Win32_DetachMillennium(VOID)
{
    logger.print(" MAIN ", "Shutting Millennium down...", COL_MAGENTA);
    millennium_lifecycle::get().terminate.notify();
    logger.log("Waiting for Millennium thread to exit...");

    if (!g_millenniumThread.joinable()) {
        platform::messagebox::show("Warning", "Millennium thread is not joinable, skipping join. This is likely because Millennium failed to start properly.",
                                   platform::messagebox::warn);
        return;
    }

    g_millenniumThread.join();
    g_millennium.reset();
    logger.log("Millennium thread has exited.");
}

/**
 * @brief Entry point for Millennium on Windows.
 * @param fdwReason The reason for calling the DLL.
 * @return True if the DLL was successfully loaded, false otherwise.
 */
DLL_EXPORT INT WINAPI DllMain([[maybe_unused]] HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
        {
            logger.log("Millennium-x86_64@{} attached...", MILLENNIUM_VERSION);
            register_dll_notifications();

            g_millenniumThread = std::thread(Win32_AttachMillennium);
            break;
        }
        case DLL_PROCESS_DETACH:
        {
            if (lpvReserved != nullptr) {
                if (g_millenniumThread.joinable()) {
                    g_millenniumThread.detach();
                }
                void(g_millennium.release());
                break;
            }

            Win32_DetachMillennium();
            break;
        }
    }

    return true;
}
#endif
