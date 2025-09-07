/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
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

#pragma once
#include "__builtins__/config.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>

class ThemeConfig
{
  public:
    ThemeConfig();

    void OnConfigChange();
    void UpgradeOldConfig();
    void ValidateTheme();

    nlohmann::json GetConfig();
    void SetConfig(const std::string& path, const nlohmann::json& value);

    void ChangeTheme(const std::string& theme_name);
    nlohmann::json GetAccentColor();
    nlohmann::json GetActiveTheme();

    void SetupThemeHooks();
    void StartWebkitHook(const nlohmann::json& theme, const std::string& name);

    void SetupColors();
    std::string GetColors();
    nlohmann::json GetColorOpts(const std::string& theme_name);
    void ChangeColor(const std::string& theme, const std::string& color_name, const std::string& new_color, int color_type);
    void ChangeAccentColor(const std::string& new_color);
    void ResetAccentColor();

    bool DoesThemeUseAccentColor();
    std::string GetConditionals();
    nlohmann::json ChangeCondition(const std::string& theme, const nlohmann::json& newData, const std::string& condition);

  private:
    void SetupConditionals(const nlohmann::json& theme_data);
    std::set<std::string> GetAllImports(const std::filesystem::path& css_path, std::set<std::string> visited = {});

    std::filesystem::path themes_path;
    std::string active_theme_name;
    nlohmann::json theme_data;
    std::map<std::string, nlohmann::json> colors;
};