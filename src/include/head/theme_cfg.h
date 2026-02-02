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
#include "filesystem"
#include <set>

#include "head/webkit.h"
#include "millennium/config.h"

namespace head
{
class theme_config_store
{
  public:
    theme_config_store(std::shared_ptr<plugin_manager> settings_store, std::shared_ptr<theme_webkit_mgr> theme_webkit_mgr);
    ~theme_config_store();

    void on_config_change_hdlr();
    void migrate_old_config();
    void validate_theme();

    json get_config();
    void set_config(const std::string& path, const json& value);

    void change_theme(const std::string& theme_name);
    json get_accent_color();
    json get_active_theme();

    void setup_theme_hooks();
    void start_webkit_hook(const json& theme, const std::string& name);

    void setup_colors();
    json get_colors();
    json get_color_options(const std::string& theme_name);
    json set_theme_color(const std::string& theme, const std::string& color_name, const std::string& new_color, int color_type);
    void set_accent_color(const std::string& new_color);
    void reset_accent_color();

    bool does_theme_use_accent_color();
    std::string get_theme_conditionals();
    json set_condition(const std::string& theme, const json& newData, const std::string& condition);

  private:
    void setup_conditionals();
    std::set<std::string> get_all_imports(const std::filesystem::path& css_path, std::set<std::string> visited = {});

    config_manager::listener config_listener_;
    std::filesystem::path themes_path;
    std::string active_theme_name;
    json theme_data;
    std::map<std::string, json> colors;
    std::shared_ptr<plugin_manager> m_settings_store;
    std::shared_ptr<theme_webkit_mgr> m_theme_webkit_mgr;
};
} // namespace head
