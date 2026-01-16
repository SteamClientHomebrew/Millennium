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

#include "millennium/fwd_decl.h"
#include "millennium/sysfs.h"

#include <filesystem>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

class plugin_installer
{
  public:
    plugin_installer(std::weak_ptr<millennium_backend> millennium_backend, std::shared_ptr<SettingsStore> settings_store_ptr, std::weak_ptr<Updater> updater);

    bool is_plugin_installed(const std::string& pluginName) const;
    bool uninstall_plugin(const std::string& pluginName) const;
    bool update_plugin(const std::string& id, const std::string& name) const;
    nlohmann::json install_plugin(const std::string& downloadUrl, size_t totalSize) const;
    static nlohmann::json get_updater_request_body();

  private:
    std::weak_ptr<millennium_backend> m_millennium_backend;
    std::shared_ptr<SettingsStore> settings_store_ptr;
    std::weak_ptr<Updater> m_updater;

    static std::filesystem::path get_plugins_path();

    static std::optional<nlohmann::json> read_plugin_metadata(const std::filesystem::path& pluginPath);
    static std::vector<nlohmann::json> get_plugin_data();
};
