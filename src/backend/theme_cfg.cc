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

#include "millennium/environment.h"
#include "millennium/filesystem.h"
#include "millennium/logger.h"

#include <fmt/format.h>
#include <regex>

head::theme_config_store::theme_config_store(std::shared_ptr<plugin_manager> plugin_manager, std::shared_ptr<theme_webkit_mgr> theme_webkit_mgr)
    : m_plugin_manager(std::move(plugin_manager)), m_theme_webkit_mgr(std::move(theme_webkit_mgr))
{
    themes_path = std::filesystem::path(platform::get_steam_path()) / "steamui" / "skins";

    migrate_old_config();
    on_config_change_hdlr();

    config_listener_ = [this](const std::string&, const nlohmann::json&, const nlohmann::json&) { this->on_config_change_hdlr(); };
    CONFIG.register_listener(config_listener_);
}

head::theme_config_store::~theme_config_store()
{
    // CONFIG.UnregisterListener(config_listener_);
}

void head::theme_config_store::on_config_change_hdlr()
{
    validate_theme();
    m_theme_webkit_mgr->unregister_all();
    setup_theme_hooks();
}

void head::theme_config_store::migrate_old_config()
{
    std::filesystem::path old_config_path = std::filesystem::path(platform::environment::get("MILLENNIUM__CONFIG_PATH")) / "themes.json";

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
            if (!CONFIG.get("themes.conditions").is_object()) CONFIG.set("themes.conditions", nlohmann::json::object(), true);

            if (!CONFIG.get("themes.conditions." + theme_name).is_object()) CONFIG.set("themes.conditions." + theme_name, nlohmann::json::object(), true);

            for (auto& [condition_name, condition_value] : conditions.items()) {
                logger.log("Upgrading condition '" + condition_name + "' for theme '" + theme_name + "'");
                CONFIG.set("themes.conditions." + theme_name + "." + condition_name, condition_value, true);
            }
        }
    }

    /** Migrate theme colors */
    if (old_config.contains("colors") && old_config["colors"].is_object()) {
        for (auto& [theme_name, colors_obj] : old_config["colors"].items()) {
            if (!CONFIG.get("themes.themeColors").is_object()) CONFIG.set("themes.themeColors", nlohmann::json::object(), true);

            if (!CONFIG.get("themes.themeColors." + theme_name).is_object()) CONFIG.set("themes.themeColors." + theme_name, nlohmann::json::object(), true);

            for (auto& [color_name, color_value] : colors_obj.items()) {
                logger.log("Upgrading color '" + color_name + "' for theme '" + theme_name + "'");
                CONFIG.set("themes.themeColors." + theme_name + "." + color_name, color_value, true);
            }
        }
    }

    /** Migrate active theme */
    if (old_config.contains("active")) {
        logger.log("Migrating active theme from old config: " + old_config["active"].get<std::string>());
        CONFIG.set("themes.activeTheme", old_config["active"], true);
    }

    /** Migrate accent color */
    if (old_config.contains("accentColor")) {
        logger.log("Migrating accent color from old config: " + old_config["accentColor"].get<std::string>());
        CONFIG.set("general.accentColor", old_config["accentColor"], true);
    }

    /** Migrate allowed styles/scripts */
    if (old_config.contains("styles")) CONFIG.set("themes.allowedStyles", old_config["styles"], true);

    if (old_config.contains("scripts")) CONFIG.set("themes.allowedScripts", old_config["scripts"], true);

    /** Delete old config */
    std::error_code ec;
    std::filesystem::remove(old_config_path, ec);
    if (ec) LOG_ERROR("Failed to remove old config file: " + ec.message());
}

void head::theme_config_store::validate_theme()
{
    if (CONFIG.get("themes.activeTheme").is_null()) CONFIG.set("themes.activeTheme", "default");

    std::string active = CONFIG.get("themes.activeTheme").get<std::string>();
    if (active != "default" && !head::Themes::IsValid(active)) {
        logger.log("Theme '" + active + "' is invalid. Resetting to default.");
        CONFIG.set("themes.activeTheme", "default");
    }
}

nlohmann::json head::theme_config_store::get_config()
{
    return CONFIG.get_all();
}

void head::theme_config_store::set_config(const std::string& path, const nlohmann::json& value)
{
    CONFIG.set(path, value);
}

void head::theme_config_store::change_theme(const std::string& theme_name)
{
    CONFIG.set("themes.activeTheme", theme_name);
}

nlohmann::json head::theme_config_store::get_accent_color()
{
    return head::system_accent_color::plat_get_accent_color(CONFIG.get("general.accentColor").get<std::string>());
}

nlohmann::json head::theme_config_store::get_active_theme()
{
    std::string active = CONFIG.get("themes.activeTheme").get<std::string>();
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

void head::theme_config_store::setup_theme_hooks()
{
    theme_data = get_active_theme();
    active_theme_name = CONFIG.get("themes.activeTheme").get<std::string>();

    setup_conditionals();
    start_webkit_hook(theme_data, active_theme_name);
    setup_colors();
}

void head::theme_config_store::start_webkit_hook(const nlohmann::json& theme, const std::string& name)
{
    if (theme.contains("failed")) return;

    std::filesystem::path theme_path = themes_path / name;
    std::vector<std::string> cssItems = {
        "Steam-WebKit",
        "webkitCSS",
    };

    for (const auto& item : cssItems) {
        if (theme["data"].contains(item)) {
            m_theme_webkit_mgr->add_browser_hook((theme_path / theme["data"][item].get<std::string>()).generic_string(), ".*", network_hook_ctl::TagTypes::STYLESHEET);
        }
    }

    if (theme["data"].contains("webkitJS")) {
        m_theme_webkit_mgr->add_browser_hook((theme_path / theme["data"]["webkitJS"].get<std::string>()).generic_string(), ".*", network_hook_ctl::TagTypes::JAVASCRIPT);
    }

    m_theme_webkit_mgr->add_conditional_data(theme_path.generic_string(), theme["data"], name);
}

void head::theme_config_store::setup_colors()
{
    nlohmann::json all_themes = head::Themes::FindAllThemes();

    for (auto& theme : all_themes) {
        if (theme.contains("failed") || !theme.contains("data") || !theme["data"].contains("RootColors")) {
            continue;
        }

        std::string nativeName = theme["native"].get<std::string>();
        std::string rootFile = theme["data"]["RootColors"].get<std::string>();
        const auto colorsPath = platform::get_steam_path() / "steamui" / "skins" / nativeName / rootFile;

        colors[nativeName] = head::css_parser::parse_root_colors(colorsPath.generic_string());

        if (CONFIG.get("themes.themeColors", nullptr).is_null()) CONFIG.set("themes.themeColors", nlohmann::json::object(), /*skipPropagation=*/false);

        if (CONFIG.get("themes.themeColors." + nativeName, nullptr).is_null()) CONFIG.set("themes.themeColors." + nativeName, nlohmann::json::object(), /*skipPropagation=*/false);

        for (auto& color : colors[nativeName]) {
            std::string colorName = color["color"].get<std::string>();
            std::string defaultHex = color["defaultColor"].get<std::string>();
            int typeEnum = color["type"].get<int>();

            auto colorValue = head::css_parser::convert_from_hex(defaultHex, static_cast<head::color_type>(typeEnum));

            if (CONFIG.get("themes.themeColors." + nativeName + "." + colorName, nullptr).is_null())
                CONFIG.set("themes.themeColors." + nativeName + "." + colorName, colorValue.value_or(""), /*skipPropagation=*/false);
        }
    }
}

nlohmann::json head::theme_config_store::get_colors()
{
    std::string root = ":root {";
    std::string name = active_theme_name;

    if (!CONFIG.get("themes.themeColors." + name).is_object()) return ":root {}";

    const auto themeColors = CONFIG.get(fmt::format("themes.themeColors.{}", name));

    for (auto& [color, value] : themeColors.items())
        root += fmt::format("{}: {};", color, value.get<std::string>());

    root += "}";
    return root;
}

nlohmann::json head::theme_config_store::get_color_options(const std::string& theme_name)
{
    if (colors.find(theme_name) == colors.end()) {
        logger.warn("No root colors found for theme: {}", theme_name);
        return nlohmann::json::array();
    }

    nlohmann::json root_colors = colors[theme_name];
    auto saved_colors = CONFIG.get("themes.themeColors." + theme_name);

    for (auto& color : root_colors) {
        std::string cname = color["color"].get<std::string>();
        if (saved_colors.contains(cname)) {
            const auto hex_color = head::css_parser::convert_to_hex(saved_colors[cname].get<std::string>(), static_cast<head::color_type>(color["type"].get<int>()));
            if (hex_color.has_value()) color["hex"] = hex_color.value();
        }
    }

    return root_colors;
}

nlohmann::json head::theme_config_store::set_theme_color(const std::string& theme, const std::string& color_name, const std::string& new_color, int color_type)
{
    std::optional<std::string> parsed_color = head::css_parser::convert_from_hex(new_color, static_cast<head::color_type>(color_type));

    if (!parsed_color.has_value()) {
        LOG_ERROR("Failed to parse color: {}", new_color);
        return {};
    }

    CONFIG.set(fmt::format("themes.themeColors.{}.{}", theme, color_name), parsed_color.value(), true);
    return parsed_color.value();
}

void head::theme_config_store::set_accent_color(const std::string& new_color)
{
    CONFIG.set("general.accentColor", new_color);
}

void head::theme_config_store::reset_accent_color()
{
    CONFIG.set("general.accentColor", "DEFAULT_ACCENT_COLOR");
}

void head::theme_config_store::setup_conditionals()
{
    if (!CONFIG.get("themes.conditions", nlohmann::json::object()).is_object()) CONFIG.set("themes.conditions", nlohmann::json::object());

    nlohmann::json themes = head::Themes::FindAllThemes();

    for (auto& theme : themes) {
        if (!theme.contains("data") || theme.contains("failed")) {
            logger.log("Skipping invalid or failed theme.");
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
                logger.log("Skipping invalid condition: " + condition_name);
                continue;
            }

            nlohmann::json current_conditions = CONFIG.get(fmt::format("themes.conditions.{}", theme_name), nlohmann::json::object());

            if (!current_conditions.contains(condition_name)) {
                CONFIG.set(fmt::format("themes.conditions.{}.{}", theme_name, condition_name), default_value);
            } else {
                nlohmann::json value = current_conditions[condition_name];
                if (!condition_value["values"].contains(value)) {
                    CONFIG.set(fmt::format("themes.conditions.{}.{}", theme_name, condition_name), default_value);
                }
            }
        }
    }

    CONFIG.save_to_disk();
}

nlohmann::json head::theme_config_store::set_condition(const std::string& theme, const nlohmann::json& newData, const std::string& condition)
{
    CONFIG.set(fmt::format("themes.conditions.{}.{}", theme, condition), newData, true);

    return {
        { "success", true }  /** just assume it worked, too laze to impl :) */
    };
}

std::string head::theme_config_store::get_theme_conditionals()
{
    if (!CONFIG.CONFIG.get("themes.conditions").is_object()) return "{}";

    return CONFIG.CONFIG.get("themes.conditions").dump(4);
}

std::set<std::string> head::theme_config_store::get_all_imports(const std::filesystem::path& css_path, std::set<std::string> visited)
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

        get_all_imports(imported_path, visited);
        search_start = match.suffix().first;
    }

    return visited;
}
