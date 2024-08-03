#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <util/log.h>
#include <sys/locals.h>
#include <regex>
#include <typeinfo>
#include <memory>
#include <cxxabi.h>
#include <fmt/core.h>   

bool GetThemeJson(std::string themeName, nlohmann::json& json) {
    const auto themesDirectory = SystemIO::GetSteamPath() / "steamui" / "skins";
    
    if (!std::filesystem::exists(themesDirectory / themeName)) {
        LOG_FAIL("Theme directory does not exist.");
        return false;
    }

    bool success = false;
    json = SystemIO::ReadJsonSync((themesDirectory / themeName / "skin.json").string(), &success);
    if (!success) {
        LOG_FAIL("Failed to parse theme information.");
        return false;
    }
    return true;
}

nlohmann::json GetThemeDataJson() {
    const auto themeDataPath = SystemIO::GetSteamPath() / "ext" / "themes.json";
    bool success = false;
    const auto json = SystemIO::ReadJsonSync(themeDataPath.string(), &success);
    if (!success) {
        LOG_FAIL("Failed to read themes.json");
        return {};
    }
    return json;
}

int PrintAllThemes() {
    std::vector<std::tuple<std::string, std::string>> themes;
    const auto themes_dir = SystemIO::GetSteamPath() / "steamui" / "skins";

    if (!std::filesystem::exists(themes_dir)) {
        LOG_FAIL("Themes directory does not exist");
        return 1;
    }

    for (const auto& entry : std::filesystem::directory_iterator(themes_dir)) {
        if (entry.is_directory()) {
            nlohmann::json json;
            const bool successful = GetThemeJson(entry.path().filename().string(), json);
            if (!successful) {
                LOG_WARN("Failed to parse theme information for " << entry.path().filename().string());
                themes.push_back({entry.path().filename().string(), std::string()});
            }
            else {
                themes.push_back({entry.path().filename().string(), json.value("name", std::string{})});
            }
        }
    }

    // get the longest theme name for formatting
    size_t longest_theme_name = std::get<0>(*std::max_element(themes.begin(), themes.end(), [](const auto& a, const auto& b) {
        return std::get<0>(a).size() < std::get<0>(b).size();
    })).size();

    for (const auto& [name, path] : themes) {
        std::cout << name << std::string(longest_theme_name - name.size(), ' ') << std::endl;

        // if (!path.empty()) 
        //     std::cout << GREY << "  AKA " << path << RESET << std::endl;
        // else 
        //     std::cout << std::endl;     
    }

    return 0;
}

int PrintCurrentTheme() {
    const auto activeTheme = GetThemeDataJson()["active"].get<std::string>();
    fmt::print("{}\n", activeTheme);

    if (activeTheme == "default") {
        return 0;
    }

    nlohmann::basic_json<> themeJson;
    if (!GetThemeJson(activeTheme, themeJson)) {
        LOG_WARN("Active theme could not be found on the disk, rendering it invalid.");
    }
    return 0;
}

int SetTheme(std::string themeName) {
    const auto themesDirectory = SystemIO::GetSteamPath() / "steamui" / "skins";
    const auto themeDataPath = SystemIO::GetSteamPath() / "ext" / "themes.json";

    bool success = false;
    nlohmann::json json = SystemIO::ReadJsonSync(themeDataPath.string(), &success);
    if (!success) {
        LOG_FAIL("Failed to read themes.json");
        return 1;
    }

    if (!json.contains("active") || !json["active"].is_string()) {
        LOG_FAIL("No active theme found.");
        return 1;
    }

    if (!std::filesystem::exists(themesDirectory / themeName)) {
        LOG_FAIL("Theme directory does not exist.");
        return 1;
    }

    json["active"] = themeName;
    SystemIO::WriteFileSync(themeDataPath, json.dump(4));
    LOG_INFO("Your current theme is now " << themeName << "\nTo apply changes run:\nmillennium apply");
    return 0;
}

int PrintThemeConfig(const std::string& themeName) {
    const auto themesDir = SystemIO::GetSteamPath() / "steamui" / "skins" / themeName;
    const auto themeDataPath = SystemIO::GetSteamPath() / "ext" / "themes.json";

    if (!std::filesystem::exists(themesDir)) {
        LOG_FAIL("Theme directory does not exist.");
        return 1;
    }

    bool success;
    auto themeJson = SystemIO::ReadJsonSync((themesDir / "skin.json").string(), &success);
    if (!success) {
        LOG_FAIL("Failed to read skin.json");
        return 1;
    }

    auto themeDataJson = SystemIO::ReadJsonSync((themeDataPath).string(), &success);
    if (!success) {
        LOG_FAIL("Failed to read themes.json");
        return 1;
    }

    if (!themeDataJson.value("conditions", nlohmann::json{}).contains(themeName)) {
        LOG_FAIL(themeName << " does not contain any configurable options.");
        return 1;
    }

    for (const auto& [key, value] : themeDataJson["conditions"][themeName].items()) {
        try {
            fmt::print("\n{} = {}\n", key, value.dump());

            if (themeJson.contains("Conditions") && themeJson["Conditions"].contains(key)) {
                std::string description = themeJson["Conditions"][key].value("description", std::string{});
                std::string defaultVal = themeJson["Conditions"][key].value("default", std::string{});

                fmt::print("    Values: [");
                for (const auto& [key, _] : themeJson["Conditions"][key]["values"].items()) 
                    fmt::print("{}, ", key);

                fmt::print("\b\b] Default: [{}]\n", defaultVal);
            }
        }
        catch (const std::exception& e) {
            LOG_FAIL("Failed to print theme config for " << key << " with error: " << e.what());
        }
    }
    return 0;
}

int PrintThemesWithConfig() {
    const auto themesDir = SystemIO::GetSteamPath() / "steamui" / "skins";
    const auto themeDataPath = SystemIO::GetSteamPath() / "ext" / "themes.json";

    bool success;
    auto themeDataJson = SystemIO::ReadJsonSync((themeDataPath).string(), &success);
    if (!success) {
        LOG_FAIL("Failed to read themes.json");
        return 1;
    }

    for (const auto& entry : std::filesystem::directory_iterator(themesDir)) {
        if (entry.is_directory()) {
            if (themeDataJson.value("conditions", nlohmann::json{}).contains(entry.path().filename().string())) {
                fmt::print("{}\n", entry.path().filename().string());
            }
        }
    }
    return 0;
}
