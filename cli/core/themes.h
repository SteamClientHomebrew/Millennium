#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <logger/log.h>
#include <sys/locals.h>
#include <regex>
#include <typeinfo>
#include <memory>
#include <cxxabi.h>

void PrintAllThemes() {
    std::vector<std::tuple<std::string, std::string>> themes;
    const auto themes_dir = SystemIO::GetSteamPath() / "steamui" / "skins";

    if (!std::filesystem::exists(themes_dir)) {
        LOG_FAIL("Themes directory does not exist");
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(themes_dir)) {
        if (entry.is_directory()) {
            const auto theme_json = entry.path() / "skin.json";
            if (!std::filesystem::exists(theme_json)) 
                continue; 

            bool success = false;
            const auto json = SystemIO::ReadJsonSync(theme_json, &success);
            if (!success) 
                continue;

            themes.push_back({entry.path().filename().string(), json.contains("name") ? json["name"].get<std::string>() : std::string()});
        }
    }

    // get the longest theme name for formatting
    size_t longest_theme_name = std::get<0>(*std::max_element(themes.begin(), themes.end(), [](const auto& a, const auto& b) {
        return std::get<0>(a).size() < std::get<0>(b).size();
    })).size();

    for (const auto& [name, path] : themes) {
        if (path.empty()) 
            std::cout << name << std::string(longest_theme_name - name.size(), ' ') << std::endl;
        else
            std::cout << name << std::string(longest_theme_name - name.size(), ' ') << GREY << " AKA " << path << RESET << std::endl;
    }
}

void PrintCurrentTheme() {
    const auto themes_dir = SystemIO::GetSteamPath() / "steamui" / "skins";
    const auto themes_info = SystemIO::GetSteamPath() / "ext" / "themes.json";

    bool success = false;
    const auto json = SystemIO::ReadJsonSync(themes_info, &success);
    if (!success) {
        LOG_FAIL("Failed to read themes.json");
        return;
    }

    if (!json.contains("active") || !json["active"].is_string()) {
        LOG_FAIL("No active theme found.");
        return;
    }

    const auto theme_dir = themes_dir / json["active"].get<std::string>();
    std::cout << json["active"].get<std::string>() << std::endl;

    if (!std::filesystem::exists(theme_dir)) {
        LOG_WARN("Active theme directory does not exist.");
    }
}