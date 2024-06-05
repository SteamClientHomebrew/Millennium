#include "locals.h"
#include <fstream>
#include <utilities/log.hpp>
#include <fmt/core.h>
#include <iostream>
#include "base.h"

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
            return fmt::format("{}/.steam/steam", std::getenv("HOME"));
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
            Logger.Error("error reading [{}]", filename);

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
            Logger.Error("failed to open file [readJsonSync]");
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