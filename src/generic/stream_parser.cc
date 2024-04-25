#include "stream_parser.h"
#include <fstream>
#include <utilities/log.hpp>
#include <fmt/core.h>
#include <iostream>
#include <windows.h>
#include "base.h"

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
            const auto json = file::readJsonSync(fmt::format("{}/plugin.json", millennium_modules_path));
            
            stream_buffer::plugin_mgr::plugin_t plugin;
            plugin.base_dir     = millennium_modules_path;
            plugin.backend_abs  = fmt::format("{}/backend/main.py", millennium_modules_path);
            plugin.name         = "millennium__internal";
            plugin.frontend_abs = fmt::format(".millennium_modules/dist/index.js");
            plugin.pjson        = json;

            plugins.push_back(plugin);
        }
        catch (file::io_except& ex) {
            console.err(ex.what());
        }

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
                        
                        stream_buffer::plugin_mgr::plugin_t plugin;
                        plugin.base_dir     = entry.path();
                        plugin.backend_abs  = entry.path() / "backend" / "main.py";
                        plugin.name         = (json.contains("name") && json["name"].is_string() ? json["name"].get<std::string>() : entry.path().filename().string());
                        plugin.frontend_abs = fmt::format("plugins/{}/dist/index.js", entry.path().filename().string());
                        plugin.pjson        = json;

                        plugins.push_back(plugin);
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