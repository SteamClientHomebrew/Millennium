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

#include <filesystem>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

class PluginInstaller
{
  public:
    static bool CheckInstall(const std::string& pluginName);
    static bool UninstallPlugin(const std::string& pluginName);
    static bool DownloadPluginUpdate(const std::string& id, const std::string& name);
    static nlohmann::json InstallPlugin(const std::string& downloadUrl, size_t totalSize);
    [[nodiscard]] static nlohmann::json GetRequestBody();

  private:
    static void RPCLogMessage(const std::string& status, double progress, bool isComplete);
    static std::filesystem::path PluginsPath();

    static std::optional<nlohmann::json> ReadMetadata(const std::filesystem::path& pluginPath);
    [[nodiscard]] static std::vector<nlohmann::json> GetPluginData();
};
