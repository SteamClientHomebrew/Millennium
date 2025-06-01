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

#include "locals.h"
#include <fstream>
#include "log.h"
#include <fmt/core.h>
#include <iostream>
#include "fvisible.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif
#include <env.h>
#include <optional>
#include <regex>

namespace FileSystem = std::filesystem;

namespace SystemIO {

    MILLENNIUM std::filesystem::path GetSteamPath()
    {
        #ifdef _WIN32
        {
            char buffer[MAX_PATH];
            DWORD bufferSize = GetEnvironmentVariableA("SteamPath", buffer, MAX_PATH);

            const std::string steamPath = std::string(buffer, bufferSize);

            if (steamPath.empty())
            {
                return "C:/Program Files (x86)/Steam";
            }
            else
            {
                return steamPath;
            }
        }
        #elif __linux__
        {
            return fmt::format("{}/.steam/steam/", std::getenv("HOME"));
        }
        #endif
    }

    MILLENNIUM std::filesystem::path GetInstallPath()
    {
        #if defined(__linux__)
            return GetEnv("MILLENNIUM__CONFIG_PATH");
        #elif defined(_WIN32)
            return GetSteamPath();
        #endif
    }

    MILLENNIUM nlohmann::json ReadJsonSync(const std::string& filename, bool* success)
    {
        std::ifstream outputLogStream(filename);

        if (!outputLogStream.is_open())
        {
            if (success != nullptr)
            {
                *success = false;
            }
        }

        std::string fileContent((std::istreambuf_iterator<char>(outputLogStream)), std::istreambuf_iterator<char>());

        if (nlohmann::json::accept(fileContent)) 
        {
            if (success != nullptr)
            {
                *success = true;
            }
            return nlohmann::json::parse(fileContent);
        }
        else 
        {
            LOG_ERROR("error reading [{}]", filename);

            if (success != nullptr) 
            {
                *success = false;
            }
            return {};
        }
    }

    MILLENNIUM std::string ReadFileSync(const std::string& filename)
    {
        std::ifstream outputLogStream(filename);

        if (!outputLogStream.is_open())
        {
            LOG_ERROR("failed to open file -> {}", filename);
            return {};
        }

        std::string fileContent((std::istreambuf_iterator<char>(outputLogStream)), std::istreambuf_iterator<char>());
        return fileContent;
    }

    MILLENNIUM std::vector<char> ReadFileBytesSync(const std::string& filePath) 
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file) 
        {
            throw std::runtime_error("Failed to open file: " + filePath);
        }
    
        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
    
        std::vector<char> buffer(fileSize);
        if (!file.read(buffer.data(), fileSize)) 
        {
            throw std::runtime_error("Failed to read file: " + filePath);
        }
    
        return buffer;
    }

    MILLENNIUM void WriteFileSync(const std::filesystem::path& filePath, std::string content)
    {
        std::ofstream outFile(filePath);

        if (outFile.is_open()) 
        {
            outFile << content;
            outFile.close();
        }
    }

    MILLENNIUM void WriteFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent)
    {
        Logger.Log(fmt::format("writing file to: {}", filePath.string()));

        std::ofstream fileStream(filePath, std::ios::binary);
        if (!fileStream)
        {
            Logger.Log(fmt::format("Failed to open file for writing: {}", filePath.string()));
            return;
        }

        fileStream.write(reinterpret_cast<const char*>(fileContent.data()), fileContent.size());

        if (!fileStream)
        {
            Logger.Log(fmt::format("Error writing to file: {}", filePath.string()));
        }

        fileStream.close();
    }

    MILLENNIUM std::optional<std::string> GetMillenniumPreloadPath()
    {
        const std::filesystem::path dirPath = std::filesystem::path(GetEnv("MILLENNIUM__SHIMS_PATH"));

        // Regex for files starting with "millennium" and ending with ".js"
        const std::regex pattern("^millennium.*\\.js$", std::regex_constants::icase);

        for (const auto& entry : std::filesystem::directory_iterator(dirPath))
        {
            if (entry.is_regular_file())
            {
                const std::string filename = entry.path().filename().string();
                if (std::regex_match(filename, pattern))
                {
                    // Ensure forward slashes in the path
                    return entry.path().generic_string();
                }
            }
        }

        return std::nullopt;
    }
}