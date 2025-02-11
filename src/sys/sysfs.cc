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

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

namespace FileSystem = std::filesystem;

namespace SystemIO {

    std::filesystem::path GetSteamPath()
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

    std::filesystem::path GetInstallPath()
    {
        #ifdef _WIN32
        {
            return GetSteamPath();
        }
        #elif __linux__
        {
            return fmt::format("{}/.millennium", std::getenv("HOME"));
        }
        #endif
    }

    nlohmann::json ReadJsonSync(const std::string& filename, bool* success)
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

    std::string ReadFileSync(const std::string& filename)
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

    void WriteFileSync(const std::filesystem::path& filePath, std::string content)
    {
        std::ofstream outFile(filePath);

        if (outFile.is_open()) 
        {
            outFile << content;
            outFile.close();
        }
    }

    void WriteFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent)
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
}