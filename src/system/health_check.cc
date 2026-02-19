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

#include "millennium/filesystem.h"
#include "millennium/logger.h"
#include "millennium/health_check.h"
#include <filesystem>
#include <vector>

/**
 * The following keys in "Steam.cfg" block Steam from updating, which
 * breaks Millennium as Millennium requires the latest version of Steam.
 */
static const std::vector<std::string> blackListedKeys = {
    "BootStrapperInhibitAll", "BootStrapperForceSelfUpdate", "BootStrapperInhibitClientChecksum", "BootStrapperInhibitBootstrapperChecksum", "BootStrapperInhibitUpdateOnLaunch",
};

void platform::health::check_health()
{
    const auto steam_path = get_steam_path();
    const auto cef_remote_debugger_file = steam_path / ".cef-enable-remote-debugging";

#ifdef __linux__
    if (std::filesystem::exists(cef_remote_debugger_file) && !std::filesystem::remove(cef_remote_debugger_file)) {
        LOG_ERROR("Failed to remove '{}', likely non-fatal but manual intervention recommended.", cef_remote_debugger_file.string());
    }
#elif _WIN32
    const auto steam_cfg = steamPath / "Steam.cfg";
    const auto bootstrap_error = fmt::format("Millennium is incompatible with your {} config. Remove this file to allow Steam updates.", steam_cfg.string());
    const auto cef_error = fmt::format("Failed to remove deprecated file: {}\nRemove manually and restart Steam.", cefRemoteDebugging.string());

    auto show_bootstrap_error = [&]()
    {
        Plat_ShowMessageBox("Startup Error", bootstrap_error.c_str(), MESSAGEBOX_ERROR);
        LOG_ERROR(bootstrap_error);
    };

    if (std::filesystem::exists(steam_cfg)) {
        try {
            const std::string steamConfig = platform::read_file(steam_cfg.string());
            if (std::any_of(blackListedKeys.begin(), blackListedKeys.end(), [&](const auto& key) { return steamConfig.find(key) != std::string::npos; })) {
                show_bootstrap_error();
            }
        } catch (const platform::file_exception&) {
            show_bootstrap_error();
        }
    }

    if (std::filesystem::exists(cefRemoteDebugging)) {
        try {
            std::filesystem::remove(cefRemoteDebugging);
        } catch (const std::filesystem::filesystem_error& e) {
            LOG_ERROR("Failed to remove deprecated file {}: {}", cefRemoteDebugging.string(), e.what());
            Plat_ShowMessageBox("Startup Error", cef_error.c_str(), MESSAGEBOX_ERROR);
        }
    }
#endif
}
