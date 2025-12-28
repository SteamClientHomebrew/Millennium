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

#include "head/theme_cfg.h"
#include "head/css_parser.h"
#include "head/scan.h"
#include "head/sys_accent_col.h"
#include "head/webkit.h"

#include "millennium/logger.h"

#include <fmt/format.h>
#include <regex>

ThemeConfig::ThemeConfig()
{
    themes_path = std::filesystem::path(SystemIO::GetSteamPath()) / "steamui" / "skins";

    UpgradeOldConfig();
    OnConfigChange();

    config_listener_ = [this](const std::string&, const nlohmann::json&, const nlohmann::json&) { this->OnConfigChange(); };
    CONFIG.RegisterListener(config_listener_);
}

ThemeConfig::~ThemeConfig()
{
    CONFIG.UnregisterListener(config_listener_);
}

void ThemeConfig::OnConfigChange()
{
    ValidateTheme();
    WebkitHookStore::Instance().UnregisterAll();
    SetupThemeHooks();
}

void ThemeConfig::UpgradeOldConfig()
{
    std::filesystem::path old_config_path = std::filesystem::path(std::getenv("MILLENNIUM__CONFIG_PATH")) / "themes.json";

    if (!std::filesystem::exists(old_config_path)) return;

    nlohmann::json old_config;
    try {
        std::ifstream file(old_config_path);
        file >> old_config;
    } catch (const std::exception& ex) {
        LOG_ERROR("Failed to read old config: " + std::string(ex.what()));
        return;
    }

    /** Migrate themes.conditions */
    if (old_config.contains("conditions") && old_config["conditions"].is_object()) {
        for (auto& [theme_name, conditions] : old_config["conditions"].items()) {
            if (!CONFIG.GetNested("themes.conditions").is_object()) CONFIG.SetNested("themes.conditions", nlohmann::json::object(), true);

            if (!CONFIG.GetNested("themes.conditions." + theme_name).is_object()) CONFIG.SetNested("themes.conditions." + theme_name, nlohmann::json::object(), true);

            for (auto& [condition_name, condition_value] : conditions.items()) {
                Logger.Log("Upgrading condition '" + condition_name + "' for theme '" + theme_name + "'");
                CONFIG.SetNested("themes.conditions." + theme_name + "." + condition_name, condition_value, true);
            }
        }
    }

    /** Migrate theme colors */
    if (old_config.contains("colors") && old_config["colors"].is_object()) {
        for (auto& [theme_name, colors_obj] : old_config["colors"].items()) {
            if (!CONFIG.GetNested("themes.themeColors").is_object()) CONFIG.SetNested("themes.themeColors", nlohmann::json::object(), true);

            if (!CONFIG.GetNested("themes.themeColors." + theme_name).is_object()) CONFIG.SetNested("themes.themeColors." + theme_name, nlohmann::json::object(), true);

            for (auto& [color_name, color_value] : colors_obj.items()) {
                Logger.Log("Upgrading color '" + color_name + "' for theme '" + theme_name + "'");
                CONFIG.SetNested("themes.themeColors." + theme_name + "." + color_name, color_value, true);
            }
        }
    }

    /** Migrate active theme */
    if (old_config.contains("active")) {
        Logger.Log("Migrating active theme from old config: " + old_config["active"].get<std::string>());
        CONFIG.SetNested("themes.activeTheme", old_config["active"], true);
    }

    /** Migrate accent color */
    if (old_config.contains("accentColor")) {
        Logger.Log("Migrating accent color from old config: " + old_config["accentColor"].get<std::string>());
        CONFIG.SetNested("general.accentColor", old_config["accentColor"], true);
    }

    /** Migrate allowed styles/scripts */
    if (old_config.contains("styles")) CONFIG.SetNested("themes.allowedStyles", old_config["styles"], true);

    if (old_config.contains("scripts")) CONFIG.SetNested("themes.allowedScripts", old_config["scripts"], true);

    /** Delete old config */
    std::error_code ec;
    std::filesystem::remove(old_config_path, ec);
    if (ec) LOG_ERROR("Failed to remove old config file: " + ec.message());
}

void ThemeConfig::ValidateTheme()
{
    if (CONFIG.GetNested("themes.activeTheme").is_null()) CONFIG.SetNested("themes.activeTheme", "default");

    std::string active = CONFIG.GetNested("themes.activeTheme").get<std::string>();
    if (active != "default" && !Millennium::Themes::IsValid(active)) {
        Logger.Log("Theme '" + active + "' is invalid. Resetting to default.");
        CONFIG.SetNested("themes.activeTheme", "default");
    }
}

nlohmann::json ThemeConfig::GetConfig()
{
    return CONFIG.GetAll();
}

void ThemeConfig::SetConfig(const std::string& path, const nlohmann::json& value)
{
    CONFIG.SetNested(path, value);
}

void ThemeConfig::ChangeTheme(const std::string& theme_name)
{
    CONFIG.SetNested("themes.activeTheme", theme_name);
}

nlohmann::json ThemeConfig::GetAccentColor()
{
    return Colors::GetAccentColor(CONFIG.GetNested("general.accentColor").get<std::string>());
}

nlohmann::json ThemeConfig::GetActiveTheme()
{
    std::string active = CONFIG.GetNested("themes.activeTheme").get<std::string>();
    std::filesystem::path path = themes_path / active / "skin.json";

    try {
        std::ifstream file(path);
        nlohmann::json data;
        file >> data;
        return {
            { "native", active },
            { "data",   data   }
        };
    } catch (...) {
        return {
            { "failed", true }
        };
    }
}

void ThemeConfig::SetupThemeHooks()
{
    theme_data = GetActiveTheme();
    active_theme_name = CONFIG.GetNested("themes.activeTheme").get<std::string>();

    SetupConditionals();
    StartWebkitHook(theme_data, active_theme_name);
    SetupColors();
}

void ThemeConfig::StartWebkitHook(const nlohmann::json& theme, const std::string& name)
{
    if (theme.contains("failed")) return;

    std::filesystem::path theme_path = themes_path / name;
    std::vector<std::string> cssItems = {
        "Steam-WebKit",
        "webkitCSS",
    };

    for (const auto& item : cssItems) {
        if (theme["data"].contains(item)) {
            AddBrowserCss((theme_path / theme["data"][item].get<std::string>()).generic_string(), ".*");
        }
    }

    if (theme["data"].contains("webkitJS")) AddBrowserJs((theme_path / theme["data"]["webkitJS"].get<std::string>()).generic_string(), ".*");

    AddConditionalData(theme_path.generic_string(), theme["data"], name);
}

void ThemeConfig::SetupColors()
{
    nlohmann::json all_themes = Millennium::Themes::FindAllThemes();

    for (auto& theme : all_themes) {
        if (theme.contains("failed") || !theme.contains("data") || !theme["data"].contains("RootColors")) {
            continue;
        }

        std::string nativeName = theme["native"].get<std::string>();
        std::string rootFile = theme["data"]["RootColors"].get<std::string>();
        const auto colorsPath = SystemIO::GetSteamPath() / "steamui" / "skins" / nativeName / rootFile;

        colors[nativeName] = Millennium::CSSParser::ParseRootColors(colorsPath.generic_string());

        if (CONFIG.GetNested("themes.themeColors", nullptr).is_null()) CONFIG.SetNested("themes.themeColors", nlohmann::json::object(), /*skipPropagation=*/false);

        if (CONFIG.GetNested("themes.themeColors." + nativeName, nullptr).is_null())
            CONFIG.SetNested("themes.themeColors." + nativeName, nlohmann::json::object(), /*skipPropagation=*/false);

        for (auto& color : colors[nativeName]) {
            std::string colorName = color["color"].get<std::string>();
            std::string defaultHex = color["defaultColor"].get<std::string>();
            int typeEnum = color["type"].get<int>();

            auto colorValue = Millennium::CSSParser::ConvertFromHex(defaultHex, static_cast<Millennium::ColorTypes>(typeEnum));

            if (CONFIG.GetNested("themes.themeColors." + nativeName + "." + colorName, nullptr).is_null())
                CONFIG.SetNested("themes.themeColors." + nativeName + "." + colorName, colorValue.value_or(""), /*skipPropagation=*/false);
        }
    }
}

nlohmann::json ThemeConfig::GetColors()
{
    std::string root = ":root {";
    std::string name = active_theme_name;

    if (!CONFIG.GetNested("themes.themeColors." + name).is_object()) return ":root {}";

    const auto themeColors = CONFIG.GetNested(fmt::format("themes.themeColors.{}", name));

    for (auto& [color, value] : themeColors.items())
        root += fmt::format("{}: {};", color, value.get<std::string>());

    root += "}";
    return root;
}

nlohmann::json ThemeConfig::GetColorOpts(const std::string& theme_name)
{
    if (colors.find(theme_name) == colors.end()) {
        Logger.Warn("No root colors found for theme: {}", theme_name);
        return nlohmann::json::array();
    }

    nlohmann::json root_colors = colors[theme_name];
    auto saved_colors = CONFIG.GetNested("themes.themeColors." + theme_name);

    for (auto& color : root_colors) {
        std::string cname = color["color"].get<std::string>();
        if (saved_colors.contains(cname)) {
            const auto hex_color = Millennium::CSSParser::ConvertToHex(saved_colors[cname].get<std::string>(), static_cast<Millennium::ColorTypes>(color["type"].get<int>()));
            if (hex_color.has_value()) color["hex"] = hex_color.value();
        }
    }

    return root_colors;
}

nlohmann::json ThemeConfig::ChangeColor(const std::string& theme, const std::string& color_name, const std::string& new_color, int color_type)
{
    std::optional<std::string> parsed_color = Millennium::CSSParser::ConvertFromHex(new_color, static_cast<Millennium::ColorTypes>(color_type));

    if (!parsed_color.has_value()) {
        LOG_ERROR("Failed to parse color: {}", new_color);
        return {};
    }

    CONFIG.SetNested(fmt::format("themes.themeColors.{}.{}", theme, color_name), parsed_color.value(), true);
    return parsed_color.value();
}

void ThemeConfig::ChangeAccentColor(const std::string& new_color)
{
    CONFIG.SetNested("general.accentColor", new_color);
}

void ThemeConfig::ResetAccentColor()
{
    CONFIG.SetNested("general.accentColor", "DEFAULT_ACCENT_COLOR");
}

void ThemeConfig::SetupConditionals()
{
    if (!CONFIG.GetNested("themes.conditions", nlohmann::json::object()).is_object()) CONFIG.SetNested("themes.conditions", nlohmann::json::object());

    nlohmann::json themes = Millennium::Themes::FindAllThemes();

    for (auto& theme : themes) {
        if (!theme.contains("data") || theme.contains("failed")) {
            Logger.Log("Skipping invalid or failed theme.");
            continue;
        }

        const auto& conditions = theme["data"].value("Conditions", nlohmann::json::object());
        if (!conditions.is_object()) continue;

        std::string theme_name = theme.value("native", "");

        for (auto& [condition_name, condition_value] : conditions.items()) {
            nlohmann::json default_value;
            if (condition_value.contains("default")) {
                default_value = condition_value["default"];
            } else if (condition_value.contains("values") && condition_value["values"].is_object() && !condition_value["values"].empty()) {
                default_value = condition_value["values"].begin().value();
            } else {
                Logger.Log("Skipping invalid condition: " + condition_name);
                continue;
            }

            nlohmann::json current_conditions = CONFIG.GetNested(fmt::format("themes.conditions.{}", theme_name), nlohmann::json::object());

            if (!current_conditions.contains(condition_name)) {
                CONFIG.SetNested(fmt::format("themes.conditions.{}.{}", theme_name, condition_name), default_value);
            } else {
                nlohmann::json value = current_conditions[condition_name];
                if (!condition_value["values"].contains(value)) {
                    CONFIG.SetNested(fmt::format("themes.conditions.{}.{}", theme_name, condition_name), default_value);
                }
            }
        }
    }

    ConfigManager::Instance().SaveToFile();
}

nlohmann::json ThemeConfig::ChangeCondition(const std::string& theme, const nlohmann::json& newData, const std::string& condition)
{
    CONFIG.SetNested(fmt::format("themes.conditions.{}.{}", theme, condition), newData, true);

    return {
        { "success", true }  /** just assume it worked, too laze to impl :) */
    };
}

std::string ThemeConfig::GetConditionals()
{
    if (!CONFIG.CONFIG.GetNested("themes.conditions").is_object()) return "{}";

    return CONFIG.CONFIG.GetNested("themes.conditions").dump(4);
}

std::set<std::string> ThemeConfig::GetAllImports(const std::filesystem::path& css_path, std::set<std::string> visited)
{
    std::string abs_path = std::filesystem::absolute(css_path).string();
    if (visited.count(abs_path)) return visited;

    visited.insert(abs_path);

    std::ifstream file(css_path);
    if (!file.is_open()) return visited;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::regex import_regex(R"(@import\s+(?:url\()?['"]?(.*?)['"]?\)?;)");
    std::smatch match;

    std::string::const_iterator search_start(content.cbegin());
    while (std::regex_search(search_start, content.cend(), match, import_regex)) {
        std::string imported_path = match[1].str();
        if (!(imported_path.rfind("http://", 0) == 0) && !(imported_path.rfind("https://", 0) == 0)) imported_path = (css_path.parent_path() / imported_path).string();

        GetAllImports(imported_path, visited);
        search_start = match.suffix().first;
    }

    return visited;
}
