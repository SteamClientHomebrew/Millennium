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

#include "millennium/http_hooks.h"
#include "millennium/filesystem.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class theme_webkit_mgr
{
  public:
    struct webkit_item
    {
        std::string match_pattern;
        std::string path;
        std::string fileType; // "TargetCss" or "TargetJs"
    };

    theme_webkit_mgr(std::shared_ptr<SettingsStore> settings_store, std::shared_ptr<network_hook_ctl> network_hook_ctl);

    void unregister_all();
    void add_conditional_data(const std::string& path, const nlohmann::json& data, const std::string& theme_name);
    unsigned long long add_browser_hook(const std::string& path, const std::string& regex = ".*", network_hook_ctl::TagTypes type = network_hook_ctl::STYLESHEET);
    bool remove_browser_hook(unsigned long long hookId);
    std::vector<webkit_item> parse_conditional_data(const nlohmann::json& conditional_patches, const std::string& theme_name);

  private:
    std::vector<int> m_registered_hooks;
    std::shared_ptr<SettingsStore> m_settings_store;
    std::shared_ptr<network_hook_ctl> m_network_hook_ctl;
};
