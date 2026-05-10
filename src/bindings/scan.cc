/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

#include <fstream>
#include "head/scan.h"
#include "mep/crash_event_bus.h"

#include "millennium/logger.h"
#include "millennium/filesystem.h"

nlohmann::json head::Plugins::FindAllPlugins(std::shared_ptr<plugin_manager> plugin_manager)
{
    nlohmann::json result = nlohmann::json::array();
    const auto foundPlugins = plugin_manager->get_all_plugins();
    const auto pending_crashes = mep::crash_event_bus::instance().get_pending_crashes();

    for (const auto& plugin : foundPlugins) {
        const bool enabled = plugin_manager->is_enabled(plugin.plugin_name);

        /* Determine runtime status: disabled / running / crashed */
        std::string status = enabled ? "running" : "disabled";
        nlohmann::json crash_info = nullptr;

        for (const auto& crash : pending_crashes) {
            if (crash.plugin_name == plugin.plugin_name) {
                status = "crashed";

                std::string display = crash.display_name;
                if (display.empty()) display = plugin.plugin_json.value("common_name", plugin.plugin_name);

                crash_info = {
                    { "plugin",       crash.plugin_name         },
                    { "displayName",  display                   },
                    { "exitCode",     (uint32_t)crash.exit_code },
                    { "crashDumpDir", crash.crash_dump_dir      }
                };
                break;
            }
        }

        result.push_back({
            { "path",              plugin.plugin_base_dir.generic_string() },
            { "enabled",           enabled                                 },
            { "status",            status                                  },
            { "crash",             crash_info                              },
            { "isChromeExtension", false                                   },
            { "data",              plugin.plugin_json                      }
        });
    }

    return result;
}

std::optional<nlohmann::json> head::Plugins::GetPluginFromName(const std::string& plugin_name, std::shared_ptr<plugin_manager> plugin_manager)
{
    for (const auto& plugin : FindAllPlugins(plugin_manager)) {
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
bool head::Themes::IsValid(const std::string& theme_native_name)
{
    std::filesystem::path file_path = platform::get_millennium_path() / "themes" / theme_native_name / "skin.json";

    if (!std::filesystem::is_regular_file(file_path)) return false;

    std::ifstream file(file_path);
    return nlohmann::json::accept(file);
}

/**
 * Find all themes in the skins directory.
 * @return A JSON array of themes, each containing "native" (the theme folder name) and "data" (and skin data) fields.
 */
nlohmann::ordered_json head::Themes::FindAllThemes()
{
    auto path = platform::get_millennium_path() / "themes";
    std::filesystem::create_directories(path);

    nlohmann::ordered_json themes = nlohmann::ordered_json::array();

    try {
        std::vector<std::filesystem::directory_entry> dirs;
        std::copy_if(std::filesystem::directory_iterator(path), std::filesystem::directory_iterator{}, std::back_inserter(dirs), [](const auto& entry)
        {
            return entry.is_directory();
        });

        std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b)
        {
            return a.path().filename() < b.path().filename();
        });

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
        logger.log("Filesystem error: " + std::string(e.what()));
    }

    return themes;
}
