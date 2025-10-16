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

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include "millennium/sysfs.h"
#include "millennium/logger.h"
#include "millennium/auth.h"
#include "millennium/env.h"

#include <fmt/core.h>
#include <fstream>
#include <iostream>

namespace SystemIO
{

std::filesystem::path GetSteamPath()
{
#ifdef _WIN32
    HKEY hKey;
    LONG result;

    /** try to open the Steam registry key */
    result = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        LOG_ERROR("GetSteamPath: Failed to open Steam registry key (error: {}), defaulting to C:/Program Files (x86)/Steam", result);
        return "C:/Program Files (x86)/Steam";
    }

    DWORD dataType;
    DWORD dataSize = 0;

    /** first call to get the required buffer size */
    result = RegQueryValueExA(hKey, "SteamPath", nullptr, &dataType, nullptr, &dataSize);
    if (result != ERROR_SUCCESS || dataType != REG_SZ) {
        RegCloseKey(hKey);
        LOG_ERROR("GetSteamPath: Failed to query SteamPath registry value (error: {}), defaulting to C:/Program Files (x86)/Steam", result);
        return "C:/Program Files (x86)/Steam";
    }

    /** now allocate space using that buffer length */
    std::vector<char> buffer(dataSize);
    result = RegQueryValueExA(hKey, "SteamPath", nullptr, &dataType, reinterpret_cast<LPBYTE>(buffer.data()), &dataSize);

    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        LOG_ERROR("GetSteamPath: Failed to read SteamPath registry value (error: {}), defaulting to C:/Program Files (x86)/Steam", result);
        return "C:/Program Files (x86)/Steam";
    }

    /** convert to string, removing null terminator if present */
    std::string steamPath(buffer.data(), dataSize > 0 && buffer[dataSize - 1] == '\0' ? dataSize - 1 : dataSize);
    return steamPath;
#elif __linux__
    return fmt::format("{}/.steam/steam/", std::getenv("HOME"));
#elif __APPLE__
    return fmt::format("{}/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS", std::getenv("HOME"));
#endif
}

std::filesystem::path GetInstallPath()
{
#if defined(__linux__) || defined(__APPLE__)
    return GetEnv("MILLENNIUM__CONFIG_PATH");
#elif defined(_WIN32)
    return GetSteamPath();
#endif
}

nlohmann::json ReadJsonSync(const std::string& filename, bool* success)
{
    std::ifstream outputLogStream(filename);

    if (!outputLogStream.is_open()) {
        if (success != nullptr) {
            *success = false;
        }
    }

    std::string fileContent((std::istreambuf_iterator<char>(outputLogStream)), std::istreambuf_iterator<char>());

    if (nlohmann::json::accept(fileContent)) {
        if (success != nullptr) {
            *success = true;
        }
        return nlohmann::json::parse(fileContent);
    } else {
        LOG_ERROR("error reading [{}]", filename);

        if (success != nullptr) {
            *success = false;
        }
        return {};
    }
}

std::string ReadFileSync(const std::string& filename)
{
    std::ifstream outputLogStream(filename);

    if (!outputLogStream.is_open()) {
        LOG_ERROR("failed to open file -> {}", filename);
        return {};
    }

    std::string fileContent((std::istreambuf_iterator<char>(outputLogStream)), std::istreambuf_iterator<char>());
    return fileContent;
}

std::vector<char> ReadFileBytesSync(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filePath);
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(fileSize);
    if (!file.read(buffer.data(), fileSize)) {
        throw std::runtime_error("Failed to read file: " + filePath);
    }

    return buffer;
}

void WriteFileSync(const std::filesystem::path& filePath, std::string content)
{
    std::ofstream outFile(filePath);

    if (outFile.is_open()) {
        outFile << content;
        outFile.close();
    }
}

void WriteFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent)
{
    Logger.Log(fmt::format("writing file to: {}", filePath.string()));

    std::ofstream fileStream(filePath, std::ios::binary);
    if (!fileStream) {
        Logger.Log(fmt::format("Failed to open file for writing: {}", filePath.string()));
        return;
    }

    fileStream.write(reinterpret_cast<const char*>(fileContent.data()), fileContent.size());

    if (!fileStream) {
        Logger.Log(fmt::format("Error writing to file: {}", filePath.string()));
    }

    fileStream.close();
}

std::string GetMillenniumPreloadPath()
{
    return fmt::format("{}/millennium.js", GetScrambledApiPathToken());
}

void SafePurgeDirectory(const std::filesystem::path& root)
{
    size_t files = 0, dirs = 0;
    auto start = std::chrono::steady_clock::now();

    std::vector<std::filesystem::path> all_files;
    std::vector<std::filesystem::path> all_directories;

    std::function<void(const std::filesystem::path&)> collectPaths = [&](const std::filesystem::path& current)
    {
        std::error_code ec;

        for (auto& entry : std::filesystem::directory_iterator(current, ec)) {
            if (ec) {
                Logger.Log("Failed to iterate directory {}: {}", current.string(), ec.message());
                continue;
            }

            try {
                if (std::filesystem::is_directory(entry, ec) && !ec) {
                    collectPaths(entry.path());
                    all_directories.push_back(entry.path());
                } else if (!ec) {
                    all_files.push_back(entry.path());
                }
            } catch (const std::exception& e) {
                Logger.Log("Error processing {}: {}", entry.path().string(), e.what());
            }
        }
    };

    Logger.Log("Collecting all files and directories...");
    collectPaths(root);

    std::sort(all_directories.begin(), all_directories.end(),
              [](const std::filesystem::path& a, const std::filesystem::path& b) { return std::distance(a.begin(), a.end()) > std::distance(b.begin(), b.end()); });

    Logger.Log("Found {} files and {} directories to delete", all_files.size(), all_directories.size());

    for (const auto& file : all_files) {
        try {
            std::error_code ec;
            std::filesystem::permissions(file, std::filesystem::perms::owner_all | std::filesystem::perms::group_all, std::filesystem::perm_options::replace, ec);

            Logger.Log("Deleting file: {}", file.string());
            if (std::filesystem::remove(file, ec) && !ec) {
                ++files;
            } else {
                Logger.Log("Failed to delete file {}: {}", file.string(), ec ? ec.message() : "unknown error");
            }
        } catch (const std::exception& e) {
            Logger.Log("Failed to delete file {}: {}", file.string(), e.what());
        }
    }

    for (const auto& dir : all_directories) {
        try {
            std::error_code ec;
            std::filesystem::permissions(dir, std::filesystem::perms::owner_all | std::filesystem::perms::group_all, std::filesystem::perm_options::replace, ec);

            Logger.Log("Deleting directory: {}", dir.string());
            if (std::filesystem::remove(dir, ec) && !ec) {
                ++dirs;
            } else {
                Logger.Log("Failed to delete directory {}: {}", dir.string(), ec ? ec.message() : "unknown error");
            }
        } catch (const std::exception& e) {
            Logger.Log("Failed to delete directory {}: {}", dir.string(), e.what());
        }
    }

    try {
        std::error_code ec;
        std::filesystem::permissions(root, std::filesystem::perms::owner_all | std::filesystem::perms::group_all, std::filesystem::perm_options::replace, ec);

        Logger.Log("Deleting root directory: {}", root.string());
        if (std::filesystem::remove(root, ec) && !ec) {
            ++dirs;
        } else {
            Logger.Log("Failed to delete root directory {}: {}", root.string(), ec ? ec.message() : "unknown error");
        }
    } catch (const std::exception& e) {
        Logger.Log("Failed to delete root directory {}: {}", root.string(), e.what());
    }

    auto end = std::chrono::steady_clock::now();
    Logger.Log("Deleted {} files and {} directories in {} ms.", files, dirs, std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
}

void MakeWritable(const std::filesystem::path& p)
{
    std::error_code ec;
    auto perms = std::filesystem::status(p, ec).permissions();
    if (!ec)
        std::filesystem::permissions(p, perms | std::filesystem::perms::owner_write, ec);
}

bool DeleteFolder(const std::filesystem::path& p)
{
    if (!std::filesystem::exists(p))
        return true;

    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    if (!ec)
        return true;

    Logger.Log("DeleteFolder failed initially: {} — retrying with writable perms.", ec.message());
    for (auto it = std::filesystem::recursive_directory_iterator(p, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::recursive_directory_iterator(); ++it)
        MakeWritable(it->path());

    ec.clear();
    std::filesystem::remove_all(p, ec);
    if (ec) {
        LOG_ERROR("Failed to delete folder {}: {}", p.string(), ec.message());
        return false;
    }
    return true;
}
} // namespace SystemIO
