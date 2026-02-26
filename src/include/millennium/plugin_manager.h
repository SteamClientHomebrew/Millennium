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

#pragma once
#include "nlohmann/json.hpp"
#include "millennium/types.h"
#include <filesystem>
#include <vector>

class plugin_manager
{
  public:
    enum class backend_t
    {
        Python,
        Lua
    };

    static constexpr const char* plugin_config_file = "plugin.json";

    struct plugin_t
    {
        std::string plugin_name;
        json plugin_json;
        std::filesystem::path plugin_base_dir;
        std::filesystem::path plugin_backend_dir;
        std::filesystem::path plugin_frontend_dir;
        std::filesystem::path plugin_webkit_path;
        backend_t backend_type;
        bool is_internal = false;
    };

    std::vector<plugin_t> get_all_plugins();
    std::vector<plugin_t> get_enabled_backends();
    std::vector<plugin_t> get_enabled_plugins();
    std::vector<std::string> get_enabled_plugin_names();

    bool is_enabled(std::string pluginName);
    bool set_plugin_enabled(std::string pluginName, bool enabled);
    int init();

    plugin_manager();

  private:
    void lint_plugin(json json, std::string pluginName);
    plugin_t get_plugin_internal_metadata(json json, std::filesystem::directory_entry entry);
};
