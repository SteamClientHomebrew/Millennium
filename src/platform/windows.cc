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
#include "millennium/cmdline_parse.h"
#include "millennium/plat_msg.h"
#include "millennium/environment.h"
#include "millennium/http_hooks.h"
#include "millennium/plugin_loader.h"
#include "millennium/steam_hooks.h"
#include "millennium/millennium_updater.h"
#include "millennium/plat_msg.h"
#include "shared/crash_handler.h"

#include <thread>

/** forward declare function */
std::thread g_millenniumThread;
extern std::mutex mtx_hasSteamUIStartedLoading;
extern std::condition_variable cv_hasSteamUIStartedLoading;

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

static VOID Win32_CreateHardlink(const std::filesystem::path& hookPath, const std::filesystem::path& targetPath, const char* description)
{
    if (!std::filesystem::exists(hookPath)) {
        platform::messagebox::show("Millennium Error", fmt::format("Millennium {} is missing: {}\nPlease reinstall Millennium.", description, hookPath.string()).c_str(),
                                   platform::messagebox::error);
        return;
    }

    if (!AreFilesIdentical(hookPath.wstring().c_str(), targetPath.wstring().c_str())) {
        DeleteFileW(targetPath.wstring().c_str());
    }

    if (std::filesystem::exists(targetPath)) {
        return;
    }

    BOOL result = CreateHardLinkA(targetPath.string().c_str(), hookPath.string().c_str(), NULL);
    if (!result) {
        platform::messagebox::show(
            "Millennium Error",
            fmt::format("Failed to create hardlink for {}.\nError Code: {}\nMake sure Steam is not running and try again.", description, GetLastError()).c_str(),
            platform::messagebox::error);
    }
}

/**
 * Initialize Millennium webhelper hook by hardlinking it into the cef bin directory.
 */
VOID Win32_AttachWebHelperHook(VOID)
{
    const auto hookPath = platform::get_millennium_data_path() / "lib" / "millennium.hhx64.dll";
    const auto targetPath = platform::get_steam_path() / "bin" / "cef" / "cef.win7x64" / "version.dll";
    Win32_CreateHardlink(hookPath, targetPath, "webhelper hook");
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
        platform::messagebox::show("Millennium Error", fmt::format("Failed to move legacy version.dll hook.\nError Code: {}", error).c_str(), platform::messagebox::error);
    }
}

/**
 * Move a single file from src to dst. Tries rename first, falls back to copy+delete.
 * No-ops if src doesn't exist or dst already exists.
 */
static void migrate_file(const std::filesystem::path& src, const std::filesystem::path& dst)
{
    std::error_code ec;
    if (!std::filesystem::exists(src, ec)) {
        return;
    }
    if (std::filesystem::exists(dst, ec)) {
        logger.log("Migration: skipping {} (already exists at {})", src.string(), dst.string());
        return;
    }

    std::filesystem::create_directories(dst.parent_path(), ec);
    logger.log("Migration: moving {} -> {}", src.string(), dst.string());

    std::filesystem::rename(src, dst, ec);
    if (!ec) return;

    // rename fails across volumes — copy + delete
    logger.log("Migration: rename failed ({}), falling back to copy", ec.message());
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        logger.log("Migration: failed to copy {} -> {}: {}", src.string(), dst.string(), ec.message());
        return;
    }
    std::filesystem::remove(src, ec);
}

/**
 * Copy a single file from src to dst (for files that can't be moved, e.g. loaded DLLs).
 * No-ops if src doesn't exist or dst already exists.
 */
static void migrate_file_copy(const std::filesystem::path& src, const std::filesystem::path& dst)
{
    std::error_code ec;
    if (!std::filesystem::exists(src, ec) || std::filesystem::exists(dst, ec)) return;

    std::filesystem::create_directories(dst.parent_path(), ec);
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) logger.log("Migration: failed to copy {} -> {}: {}", src.string(), dst.string(), ec.message());
}

/**
 * Move a directory tree from src to dst, preserving symlinks.
 * Each entry is migrated individually so partially-migrated dirs are handled.
 * Symlinks are recreated at the destination with their original target.
 */
static void migrate_directory(const std::filesystem::path& src, const std::filesystem::path& dst)
{
    std::error_code ec;
    if (!std::filesystem::exists(src, ec) || !std::filesystem::is_directory(src, ec)) return;

    std::filesystem::create_directories(dst, ec);

    for (const auto& entry : std::filesystem::directory_iterator(src, ec)) {
        const auto name = entry.path().filename();
        const auto target = dst / name;

        if (std::filesystem::exists(target, ec)) continue;

        if (entry.is_symlink(ec)) {
            auto link_target = std::filesystem::read_symlink(entry.path(), ec);
            if (ec) continue;

            // Try directory symlink, then file symlink
            std::filesystem::create_directory_symlink(link_target, target, ec);
            if (ec) {
                ec.clear();
                std::filesystem::create_symlink(link_target, target, ec);
            }

            if (!ec) {
                std::filesystem::remove(entry.path(), ec);
            } else {
                logger.log("Migration: symlink '{}' skipped (no privilege).", name.string());
                platform::messagebox::show("Millennium Migration",
                                           fmt::format("Could not migrate symlink '{}' to the new data directory.\n\n"
                                                       "Please manually move or recreate it:\n"
                                                       "  From: {}\n"
                                                       "  To:   {}\n\n"
                                                       "Target: {}",
                                                       name.string(), entry.path().string(), target.string(), link_target.string())
                                               .c_str(),
                                           platform::messagebox::warn);
                ec.clear();
            }
        } else if (entry.is_directory(ec)) {
            migrate_directory(entry.path(), target);
            // Remove the source dir if it's now empty
            std::filesystem::remove(entry.path(), ec);
        } else {
            migrate_file(entry.path(), target);
        }
    }

    // Remove the source dir if it's now empty
    std::filesystem::remove(src, ec);
}

VOID Win32_MigrateToLocalAppData(VOID)
{
    const auto dataPath = platform::get_millennium_data_path();
    const auto steamPath = platform::get_steam_path();

    std::error_code ec;

    // Libraries -> lib/
    // millennium.dll and wsock32.dll are currently loaded — copy only, can't move.
    migrate_file_copy(steamPath / "millennium.dll", dataPath / "lib" / "millennium.dll");
    migrate_file_copy(steamPath / "wsock32.dll", dataPath / "lib" / "millennium.bootstrap64.dll");
    // Prefer new bootstrap name if the updater already placed it
    migrate_file(steamPath / "millennium.bootstrap64.dll", dataPath / "lib" / "millennium.bootstrap64.dll");
    migrate_file(steamPath / "millennium.hhx64.dll", dataPath / "lib" / "millennium.hhx64.dll");

    // Executables -> bin/
    migrate_file(steamPath / "millennium.luasb64.exe", dataPath / "bin" / "millennium.luasb64.exe");
    migrate_file(steamPath / "millennium.crashhandler64.exe", dataPath / "bin" / "millennium.crashhandler64.exe");

    // Config -> config/
    migrate_file(steamPath / "ext" / "config.json", dataPath / "config" / "config.json");
    migrate_file(steamPath / "ext" / "quickcss.css", dataPath / "config" / "quickcss.css");
    migrate_file(steamPath / "ext" / "themes.json", dataPath / "config" / "themes.json");

    // Logs and crash dumps
    migrate_directory(steamPath / "ext" / "logs", dataPath / "logs");
    migrate_directory(steamPath / "ext" / "crash_dumps", dataPath / "crash_dumps");

    // Clean up ext/ if empty
    std::filesystem::remove(steamPath / "ext", ec);

    // Plugins (may contain symlinks to dev plugins)
    migrate_directory(steamPath / "plugins", dataPath / "plugins");

    // Themes (steamui/skins/ -> themes/)
    migrate_directory(steamPath / "steamui" / "skins", dataPath / "themes");
}

VOID Win32_AttachMillennium(VOID)
{
    install_millennium_crash_handler();

    /** Starts the CEF arg hook, it doesn't wait for the hook to be installed, it waits for the hook to be setup */
    if (!Plat_InitializeSteamHooks()) {
        platform::messagebox::show("Millennium Error", "Failed to initialize Steam hooks, Millennium cannot continue startup.", platform::messagebox::error);
    }

    Win32_MoveVersionHook();

    g_millennium = std::make_unique<millennium>();

    Win32_AttachWebHelperHook();
    platform::environment::setup();

    g_millennium->entry();
    logger.log("[Win32_AttachMillennium] Millennium main function has returned, proceeding with shutdown...");

    UninitializeSteamHooks();
    /** Deallocate the developer console */
    if (CommandLineArguments::has_argument("-dev")) {
        FreeConsole();
    }
}

/**
 * Cleans up resources and gracefully shuts down Millennium on Windows.
 *
 * @warning This cleanup function assumes EntryMain has returned with success, otherwise this detach function will deadlock.
 * It depends on Python being uninitialized with all threads terminated, and all frontend hooks detached.
 */
VOID Win32_DetachMillennium(VOID)
{
    logger.print(" MAIN ", "Shutting Millennium down...", COL_MAGENTA);
    g_shouldTerminateMillennium->flag.store(true);
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
 * Setup environment variables used throughout Millennium.
 * These are vital to be setup, if they aren't Millennium and its loaded plugins will likely fail to start/run
 */
CONSTRUCTOR VOID Win32_InitializeEnvironment(VOID)
{
    try {
        Win32_MigrateToLocalAppData();
        platform::environment::setup();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to set up environment variables: {}", e.what());
        std::exit(EXIT_FAILURE);
    }
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
#if defined(MILLENNIUM_32BIT)
            const char* plat = "32-bit";
#elif defined(MILLENNIUM_64BIT)
            const char* plat = "64-bit";
#else
#error "Unsupported Platform"
#endif
            logger.log("Millennium-{}@{} attached...", plat, MILLENNIUM_VERSION);

            g_millenniumThread = std::thread(Win32_AttachMillennium);
            break;
        }
        case DLL_PROCESS_DETACH:
        {
            if (lpvReserved != nullptr) {
                if (g_millenniumThread.joinable()) {
                    g_millenniumThread.detach();
                }
                g_millennium.release();
                break;
            }

            Win32_DetachMillennium();
            break;
        }
    }

    return true;
}
#endif
