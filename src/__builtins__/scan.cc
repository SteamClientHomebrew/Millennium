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

#include "__builtins__/scan.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

std::string Millennium::Config::GetConfigPath()
{
    const char* env = std::getenv("MILLENNIUM__CONFIG_PATH");
    return env ? env : "";
}

bool Millennium::Config::IsEnabled(const std::string& plugin_name)
{
    std::ifstream file(GetConfigPath() + "/millennium.ini");
    if (!file.is_open())
        return false;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.find("enabled_plugins") != std::string::npos)
        {
            auto pos = line.find("=");
            if (pos != std::string::npos)
            {
                std::string plugins_str = line.substr(pos + 1);
                std::stringstream ss(plugins_str);
                std::string item;
                while (std::getline(ss, item, '|'))
                {
                    if (item == plugin_name)
                        return true;
                }
            }
        }
    }
    return false;
}

void Millennium::Plugins::SearchDirectories(const std::string& path, std::vector<Plugin>& plugins)
{
    for (const auto& entry : fs::directory_iterator(path))
    {
        if (!entry.is_directory())
            continue;

        std::string plugin_json_path = entry.path().string() + "/plugin.json";
        if (!fs::exists(plugin_json_path))
            continue;

        try
        {
            std::ifstream json_file(plugin_json_path);
            json skin_data;
            json_file >> skin_data;

            std::string plugin_name = skin_data.value("name", "undefined_plugin_name");
            plugins.push_back({entry.path().string(), Config::IsEnabled(plugin_name), skin_data});
        }
        catch (const json::parse_error&)
        {
            std::cerr << "Error parsing " << plugin_json_path << ". Invalid JSON format.\n";
        }
    }
}

nlohmann::json Millennium::Plugins::FindAllPlugins()
{
    std::vector<Plugin> plugins;

    const char* internal_path = std::getenv("MILLENNIUM__DATA_LIB");
    const char* user_path = std::getenv("MILLENNIUM__PLUGINS_PATH");

    for (const char* dir : {internal_path, user_path})
    {
        if (dir && fs::exists(dir))
        {
            SearchDirectories(dir, plugins);
        }
    }

    json result = json::array();
    for (const auto& p : plugins)
    {
        result.push_back({
            {"path",    p.path   },
            {"enabled", p.enabled},
            {"data",    p.data   }
        });
    }
    return result;
}

std::optional<json> Millennium::Plugins::GetPluginFromName(const std::string& plugin_name)
{
    json plugins = FindAllPlugins();
    for (const auto& plugin : plugins)
    {
        if (plugin.contains("data") && plugin["data"].contains("name") && plugin["data"]["name"] == plugin_name)
        {
            return plugin;
        }
    }
    return std::nullopt;
}

bool Millennium::Themes::IsValid(const std::string& theme_native_name)
{
    fs::path file_path = fs::path(SystemIO::GetSteamPath()) / "steamui" / "skins" / theme_native_name / "skin.json";

    if (!fs::is_regular_file(file_path))
        return false;

    try
    {
        std::ifstream file(file_path);
        json j;
        file >> j;
        return true;
    }
    catch (const json::exception&)
    {
        return false;
    }
}

nlohmann::json Millennium::Themes::FindAllThemes()
{
    fs::path path = fs::path(SystemIO::GetSteamPath()) / "steamui" / "skins";
    fs::create_directories(path);

    json themes = json::array();

    try
    {
        std::vector<std::string> theme_dirs;
        for (const auto& entry : fs::directory_iterator(path))
        {
            if (entry.is_directory())
                theme_dirs.push_back(entry.path().filename().string());
        }

        std::sort(theme_dirs.begin(), theme_dirs.end());

        for (const auto& theme : theme_dirs)
        {
            fs::path skin_json_path = path / theme / "skin.json";

            if (!fs::exists(skin_json_path))
                continue;

            try
            {
                std::ifstream json_file(skin_json_path);
                json skin_data;
                json_file >> skin_data;

                json theme_entry = {
                    {"native", theme    },
                    {"data",   skin_data}
                };

                themes.push_back(theme_entry);
            }
            catch (const json::exception&)
            {
                Logger.Log("Error parsing " + skin_json_path.string() + ". Invalid JSON format.");
            }
        }
    }
    catch (const fs::filesystem_error& e)
    {
        Logger.Log("Filesystem error: " + std::string(e.what()));
    }

    return themes;
}