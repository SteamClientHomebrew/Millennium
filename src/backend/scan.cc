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

#include "head/scan.h"
#include "millennium/env.h"
#include "millennium/logger.h"
#include "millennium/sysfs.h"

#include <fstream>
#include <iostream>

nlohmann::json Millennium::Plugins::FindAllPlugins(std::shared_ptr<SettingsStore> settings_store_ptr)
{
    nlohmann::json result = nlohmann::json::array();

    const auto foundPlugins = settings_store_ptr->ParseAllPlugins();

    for (const auto& plugin : foundPlugins) {
        result.push_back({
            { "path",    plugin.pluginBaseDirectory.generic_string()            },
            { "enabled", settings_store_ptr->IsEnabledPlugin(plugin.pluginName) },
            { "data",    plugin.pluginJson                                      }
        });
    }

    return result;
}

std::optional<nlohmann::json> Millennium::Plugins::GetPluginFromName(const std::string& plugin_name, std::shared_ptr<SettingsStore> settings_store_ptr)
{
    for (const auto& plugin : FindAllPlugins(settings_store_ptr)) {
        if (plugin.contains("data") && plugin["data"].contains("name") && plugin["data"]["name"] == plugin_name) {
            return plugin;
        }
    }
    return std::nullopt;
}

/**
 * Check if a theme is valid by verifying the existence and validity of skin.json
 * @param theme_native_name The native name of the theme (folder name).
 */
bool Millennium::Themes::IsValid(const std::string& theme_native_name)
{
    std::filesystem::path file_path = std::filesystem::path(SystemIO::GetSteamPath()) / "steamui" / "skins" / theme_native_name / "skin.json";

    if (!std::filesystem::is_regular_file(file_path)) return false;

    std::ifstream file(file_path);
    return nlohmann::json::accept(file);
}

/**
 * Find all themes in the skins directory.
 * @return A JSON array of themes, each containing "native" (the theme folder name) and "data" (and skin data) fields.
 */
nlohmann::ordered_json Millennium::Themes::FindAllThemes()
{
    auto path = std::filesystem::path(SystemIO::GetSteamPath()) / "steamui" / "skins";
    std::filesystem::create_directories(path);

    nlohmann::ordered_json themes = nlohmann::ordered_json::array();

    try {
        std::vector<std::filesystem::directory_entry> dirs;
        std::copy_if(std::filesystem::directory_iterator(path), std::filesystem::directory_iterator{}, std::back_inserter(dirs),
                     [](const auto& entry) { return entry.is_directory(); });

        std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) { return a.path().filename() < b.path().filename(); });

        for (const auto& dir : dirs) {
            auto skinJsonPath = dir.path() / "skin.json";
            if (!std::filesystem::exists(skinJsonPath)) continue;

            std::ifstream file(skinJsonPath);
            if (!file.is_open()) continue;

            nlohmann::ordered_json skinData;
            if (!(file >> skinData)) continue; /** invalid json from stream */

            themes.push_back({
                { "native", dir.path().filename().string() },
                { "data",   std::move(skinData)            }
            });
        }
    } catch (const std::filesystem::filesystem_error& e) {
        Logger.Log("Filesystem error: " + std::string(e.what()));
    }

    return themes;
}
