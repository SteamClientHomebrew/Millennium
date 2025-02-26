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

/**
 * @file env.cc
 * 
 * @brief This file is responsible for setting up environment variables that are used throughout the application.
 */

#include "env.h"
#include <string> 
#include <locals.h>
#include <fmt/core.h>
#include <log.h>
#include <stdlib.h>

void SetEnv(const std::string& key, const std::string& value) 
{
    #ifdef _WIN32
    _putenv_s(key.c_str(), value.c_str());  // Windows
    #else
    setenv(key.c_str(), value.c_str(), 1);  // Linux/macOS (1 = overwrite existing)
    #endif
}
    
void UnsetEnv(const std::string& key) 
{
    #ifdef _WIN32
    _putenv_s(key.c_str(), "");  // Windows: Setting to empty removes it
    #else
    unsetenv(key.c_str());  // Linux/macOS
    #endif
}

std::string GetEnv(std::string key)
{
    char* szVariable = getenv(key.c_str());

    if (szVariable == nullptr)
    {
        std::cerr << fmt::format("Environment variable '{}' not found.", key) << std::endl;
        return {};
    }

    std::string strVariable(szVariable);
    return strVariable;
}

/**
 * @brief Set up environment variables used throughout the application.
 */
const void SetupEnvironmentVariables()
{
    SetEnv("MILLENNIUM_RUNTIME_PATH", "/usr/lib/millennium/libmillennium_x86.so");
    SetEnv("LIBPYTHON_RUNTIME_PATH",  LIBPYTHON_RUNTIME_PATH);
    SetEnv("MILLENNIUM__VERSION",     MILLENNIUM_VERSION);

    SetEnv("MILLENNIUM__STEAM_PATH",   SystemIO::GetSteamPath()  .string());
    SetEnv("MILLENNIUM__INSTALL_PATH", SystemIO::GetInstallPath().string());

    #ifdef _WIN32
    SetEnv("MILLENNIUM__PLUGINS_PATH", SystemIO::GetInstallPath().string() + "/plugins");
    SetEnv("MILLENNIUM__CONFIG_PATH",  SystemIO::GetInstallPath().string() + "/ext");
    SetEnv("MILLENNIUM__LOGS_PATH",    SystemIO::GetInstallPath().string() + "/ext/logs");
    SetEnv("MILLENNIUM__DATA_LIB",     SystemIO::GetInstallPath().string() + "/ext/data");
    SetEnv("MILLENNIUM__PYTHON_ENV",   SystemIO::GetInstallPath().string() + "/ext/data/cache");
    SetEnv("MILLENNIUM__SHIMS_PATH",   SystemIO::GetInstallPath().string() + "/ext/data/shims");
    SetEnv("MILLENNIUM__ASSETS_PATH",  SystemIO::GetInstallPath().string() + "/ext/data/assets");
    #elif __linux__
    /* Path to the steam exe, symlinks are parsed and accepted. */
    SetEnv("MILLENNIUM__STEAM_EXE_PATH", fmt::format("{}/.steam/steam/ubuntu12_32/steam",     std::getenv("HOME")));
    SetEnv("MILLENNIUM__PLUGINS_PATH",   fmt::format("{}/.local/share/millennium/plugins",    std::getenv("HOME")));
    SetEnv("MILLENNIUM__CONFIG_PATH",    fmt::format("{}/.config/millennium",                 std::getenv("HOME")));
    SetEnv("MILLENNIUM__LOGS_PATH",      fmt::format("{}/.local/share/millennium/logs",       std::getenv("HOME")));
    SetEnv("MILLENNIUM__DATA_LIB",       fmt::format("{}/.local/share/millennium/lib",        std::getenv("HOME")));
    SetEnv("MILLENNIUM__PYTHON_ENV",     fmt::format("{}/.local/share/millennium/lib/cache",  std::getenv("HOME")));
    SetEnv("MILLENNIUM__SHIMS_PATH",     fmt::format("{}/.local/share/millennium/lib/shims",  std::getenv("HOME")));
    SetEnv("MILLENNIUM__ASSETS_PATH",    fmt::format("{}/.local/share/millennium/lib/assets", std::getenv("HOME")));
    #endif
}