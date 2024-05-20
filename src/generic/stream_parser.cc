#include "stream_parser.h"
#include <fstream>
#include <utilities/log.hpp>
#include <fmt/core.h>
#include <iostream>
#include "base.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace stream_buffer 
{
    std::filesystem::path steam_path() {
#ifdef _WIN32
        char buffer[MAX_PATH];  
        DWORD bufferSize = GetEnvironmentVariableA("SteamPath", buffer, MAX_PATH);

        return std::string(buffer, bufferSize);
#elif __linux__
        return fmt::format("{}/.steam/steam", std::getenv("HOME"));
#endif
    }

    nlohmann::json get_config() {
        const auto path = steam_path() / ".millennium" / "settings.json";

        if (!fs::exists(path)) {
            fs::create_directories(path.parent_path());
            std::ofstream outputFile(path.string());
            outputFile << "{}";
        }

        return file::readJsonSync(path.string());
    }

    void set_config(std::string file_data) {
        const auto path = steam_path() / ".millennium" / "settings.json";

        if (!fs::exists(path)) {
            fs::create_directories(path.parent_path());
            std::ofstream outputFile(path.string());
            outputFile << "{}";
        }
        file::writeFileSync(path, file_data);
    }

    void setup_config() 
    {
        auto json = get_config();

        if (json.contains("enabled") && json["enabled"].is_array()) {
            bool found = false;
            for (const auto& item : json["enabled"]) {
                if (item == "millennium__internal") {
                    found = true; break;
                }
            }
            if (!found) {
                json["enabled"].push_back("millennium__internal");
            }
        }
        else {
            json["enabled"] = nlohmann::json::array({ "millennium__internal" });
        }
        set_config(json.dump(4));
    }

    bool plugin_mgr::set_plugin_status(std::string pname, bool enabled) {

        console.log("updating plugin status");
        auto json = stream_buffer::get_config();

        if (enabled) {
            if (json.contains("enabled") && json["enabled"].is_array()) {
                console.log("pushed back plugin");
                json["enabled"].get<std::vector<std::string>>().push_back(pname);
            }
            else {
                console.log("created enabled list");
                json["enabled"] = nlohmann::json::array({pname});
            }
        }
        // disable the plugin
        else {
            if (json.contains("enabled") && json["enabled"].is_array()) {
                auto l = json["enabled"].get<std::vector<std::string>>();
                // Find and erase pname from the vector
                auto it = std::find(l.begin(), l.end(), pname);
                if (it != l.end()) { l.erase(it); }
                // Update the JSON object
                json["enabled"] = l;
                console.log("popped plugin from list");
            }
            else {
                console.err("couldn't disable [{}] as its not enabled.", pname);
            }
        }

        std::cout << "updated config -> " << json.dump(4) << std::endl;
        set_config(json.dump(4));
        return true;
    }

    std::vector<std::string> plugin_mgr::get_enabled() 
    {
        auto json = stream_buffer::get_config();
        return json["enabled"].get<std::vector<std::string>>();
    }

    bool plugin_mgr::is_enabled(std::string plugin_name) {

        for (const auto& plugin : plugin_mgr::get_enabled()) {
            if (plugin == plugin_name) {
                return true;
            }
        }
        return false;
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

            std::ifstream file(filename);
            if (!file.is_open()) {
                throw io_except(fmt::format("[{}] failed to open file -> ", __func__, filename));
            }

            std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            try {
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
            if (!fileStream) {
                console.log(fmt::format("Failed to open file for writing: {}", filePath.string()));
                return;
            }

            fileStream.write(reinterpret_cast<const char*>(fileContent.data()), fileContent.size());

            if (!fileStream) {
                console.log(fmt::format("Error writing to file: {}", filePath.string()));
            }

            fileStream.close();
        }
    }
}