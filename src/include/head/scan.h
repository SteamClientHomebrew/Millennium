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
#include "millennium/browser_extension_mgr.h"
#include "millennium/plugin_manager.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include <optional>

namespace head
{
namespace Plugins
{
struct Plugin
{
    std::string path;
    bool enabled;
    nlohmann::basic_json<> data;
};

nlohmann::json FindAllPlugins(std::shared_ptr<plugin_manager> settings_store_ptr, std::shared_ptr<browser_extension_manager> extension_mgr = nullptr);
std::optional<nlohmann::json> GetPluginFromName(const std::string& plugin_name, std::shared_ptr<plugin_manager> settings_store_ptr);
} // namespace Plugins

namespace Themes
{
bool IsValid(const std::string& theme_native_name);
nlohmann::ordered_json FindAllThemes();
} // namespace Themes
} // namespace head
