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

#pragma once
#ifdef _WIN32
#include <winbase.h>
#include <io.h>
#include "internal_logger.h"
#include <fcntl.h>
#include "plugin_logger.h"

static const char* SHIM_LOADER_PATH        = "user32.dll";
static const char* SHIM_LOADER_QUEUED_PATH = "user32.queue.dll"; // The path of the recently updated shim loader waiting for an update.

namespace WinUtils {

    enum ShimLoaderProps {
        VALID,
        INVALID,
        FAILED
    };

    const static ShimLoaderProps CheckShimLoaderVersion(std::filesystem::path shimPath)
    {
        /** Load the shim loader with DONT_RESOLVE_DLL_REFERENCES to prevent DllMain from causing a boot loop/realloc on deallocated memory */
        HMODULE hModule = LoadLibraryEx(shimPath.wstring().c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);

        if (!hModule) 
        {
            LOG_ERROR("Failed to load {}: {}", shimPath.string(), GetLastError());
            return ShimLoaderProps::FAILED;
        }

        // Call __get_shim_version() from the shim module
        typedef const char* (*GetShimVersion)();
        GetShimVersion getShimVersion = (GetShimVersion)GetProcAddress(hModule, "__get_shim_version");

        if (!getShimVersion) 
        {
            LOG_ERROR("Failed to get __get_shim_version: {}", GetLastError());
            FreeLibrary(hModule);
            return ShimLoaderProps::FAILED;
        }

        std::string shimVersion = getShimVersion();
        Logger.Log("Shim version: {}", shimVersion);

        if (shimVersion != MILLENNIUM_VERSION) 
        {
            LOG_ERROR("Shim version mismatch: {} != {}", shimVersion, MILLENNIUM_VERSION);
            FreeLibrary(hModule);
            return ShimLoaderProps::INVALID;
        }

        FreeLibrary(hModule);
        return ShimLoaderProps::VALID;
    }

    /**
     * @brief Redirects the standard output to a pipe to be processed by the plugin logger, and verifies the shim loader.
     */
    const void SetupWin32Environment()
    {
        const auto startupParams = std::make_unique<StartupParameters>();

        try 
        {
            if (!std::filesystem::exists(SystemIO::GetInstallPath() / SHIM_LOADER_QUEUED_PATH))
            {
                Logger.Log("No queued shim loader found...");
                return;
            }

            if (CheckShimLoaderVersion(SystemIO::GetInstallPath() / SHIM_LOADER_QUEUED_PATH) == ShimLoaderProps::INVALID)
            {
                MessageBoxA(NULL, "There is a version mismatch between two of Millenniums core assets 'user32.queue.dll' and 'millennium.dll' in the Steam directory. Try removing 'user32.queue.dll', and if that doesn't work reinstall Millennium.", "Oops!", MB_ICONERROR | MB_OK);
                return;
            }

            Logger.Log("Updating shim module from cache...");

            const int TIMEOUT_IN_SECONDS = 10;
            auto startTime = std::chrono::steady_clock::now();

            // Wait for the preloader to be removed, as sometimes it's still in use for a few milliseconds.
            while (true) 
            {
                try 
                {
                    std::filesystem::remove(SystemIO::GetInstallPath() / SHIM_LOADER_PATH);
                    break;
                }
                catch (std::filesystem::filesystem_error& e)
                {
                    if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(TIMEOUT_IN_SECONDS))
                    {
                        throw std::runtime_error("Timed out while waiting for the preloader to be removed.");
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }

            Logger.Log("Removed old inject shim...");

            std::filesystem::rename(SystemIO::GetInstallPath() / SHIM_LOADER_QUEUED_PATH, SystemIO::GetInstallPath() / SHIM_LOADER_PATH); 
            Logger.Log("Successfully updated {}!", SHIM_LOADER_PATH);
        }
        catch (std::exception& e) 
        {
            LOG_ERROR("Failed to update {}: {}", SHIM_LOADER_PATH, e.what());
            MessageBoxA(NULL, fmt::format("Failed to update {}, it's recommended that you reinstall Millennium.", SHIM_LOADER_PATH).c_str(), "Oops!", MB_ICONERROR | MB_OK);
        }
    }
}
#endif
