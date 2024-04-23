#include "stream_parser.h"
#include <fstream>
#include <utilities/log.hpp>
#include <fmt/core.h>
#include <iostream>
#include <windows.h>

namespace fs = std::filesystem;

namespace stream_buffer 
{
    std::filesystem::path steam_path() {

        char buffer[MAX_PATH];  
        DWORD bufferSize = GetEnvironmentVariableA("SteamPath", buffer, MAX_PATH);

        return std::string(buffer, bufferSize);
    }

    std::vector<plugin_mgr::plugin_t> plugin_mgr::parse_all() 
    {
        const auto plugin_path = steam_path() / "steamui" / "plugins";
        std::vector<plugin_mgr::plugin_t> plugins;

        try {
            for (const auto& entry : std::filesystem::directory_iterator(plugin_path)) 
            {
                if (entry.is_directory()) 
                {
                    const auto skin_json = entry.path() / "plugin.json";

                    if (!fs::exists(skin_json)) {
                        continue;
                    }

                    try {
                        const auto json = file::readJsonSync(skin_json.string());
                        
                        plugins.push_back({
                            entry.path(),
                            entry.path() / "backend" / "main.py",
                            (json.contains("name") && json["name"].is_string() ? json["name"].get<std::string>() : entry.path().filename().string()),
                            fmt::format("plugins/{}/dist/index.js", entry.path().filename().string()),
                            json
                        });
                    }
                    catch (file::io_except& ex) {
                        console.err(ex.what());
                    }
                }
            }
        } catch (const std::exception& ex) {
            std::cout << "Error: " << ex.what() << std::endl;
        }

        return plugins;
    }
    
    namespace file {

        nlohmann::json readJsonSync(const std::string& filename) {
            // Open the file
            std::ifstream file(filename);
            if (!file.is_open()) {
                throw io_except(fmt::format("[{}] failed to open file -> ", __func__, filename));
            }

            // Read the content of the file into a string
            std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            try {
                // Parse the string content as JSON
                nlohmann::json jsonData = nlohmann::json::parse(fileContent);
                return jsonData;
            }
            catch (const std::exception&) {
                throw io_except(fmt::format("[{}] failed parse file object -> ", __func__, filename));
            }
        }

        std::string readFileSync(const std::string& filename) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                console.err("failed to open file [readJsonSync]");
                return {};
            }

            std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return fileContent;
        }

        void writeFileSync(const std::filesystem::path& filePath, std::string content) {
            std::ofstream outFile(filePath);

            if (outFile.is_open()) {
                outFile << content;
                outFile.close();
            }
        }

        void writeFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent) {
            console.log(fmt::format("writing file to: {}", filePath.string()));

            std::ofstream fileStream(filePath, std::ios::binary);
            if (!fileStream)
            {
                console.log(fmt::format("Failed to open file for writing: {}", filePath.string()));
                return;
            }

            fileStream.write(reinterpret_cast<const char*>(fileContent.data()), fileContent.size());

            if (!fileStream)
            {
                console.log(fmt::format("Error writing to file: {}", filePath.string()));
            }

            fileStream.close();
        }
    }
}