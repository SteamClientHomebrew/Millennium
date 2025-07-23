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
#define MINI_CASE_SENSITIVE
#include <string>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>
#include <mini/ini.h>
#include <optional>

class SettingsStore
{
public:
    mINI::INIFile file;
    mINI::INIStructure ini;

    static constexpr const char* pluginConfigFile = "plugin.json";

    struct PluginTypeSchema
    {
        std::string pluginName;
        nlohmann::basic_json<> pluginJson;
        std::filesystem::path pluginBaseDirectory;
        std::filesystem::path backendAbsoluteDirectory;
        std::filesystem::path frontendAbsoluteDirectory;
        std::filesystem::path webkitAbsolutePath;
        bool isInternal = false;
    };

    std::vector<PluginTypeSchema> ParseAllPlugins();
    std::vector<PluginTypeSchema> GetEnabledBackends();
    std::vector<PluginTypeSchema> GetEnabledPlugins();

    bool IsEnabledPlugin(std::string pluginName);
    bool TogglePluginStatus(std::string pluginName, bool enabled);

    std::string GetSetting(std::string key, std::string defaultValue);
    void SetSetting(std::string key, std::string settingsData);

    int InitializeSettingsStore();
    std::vector<std::string> ParsePluginList();

    SettingsStore();

private:
    void LintPluginData(nlohmann::json json, std::string pluginName);
    PluginTypeSchema GetPluginInternalData(nlohmann::json json, std::filesystem::directory_entry entry);
    void InsertMillenniumModules(std::vector<SettingsStore::PluginTypeSchema>& plugins);
};

namespace SystemIO
{
    class FileException : public std::exception {
    public:
        FileException(const std::string& message) : msg(message) {}
        virtual const char* what() const noexcept override {
            return msg.c_str();
        }
    private:
        std::string msg;
    };

    std::filesystem::path GetSteamPath();

    /**
     * FIXME: This function is no longer useful and should be removed. 
     * It was intended to point to the local install path, however there isn't one anymore on unix. 
     */
    std::filesystem::path GetInstallPath();
    nlohmann::json ReadJsonSync(const std::string& filename, bool* success = nullptr);
    std::string ReadFileSync(const std::string& filename);
    std::vector<char> ReadFileBytesSync(const std::string& filePath);
    void WriteFileSync(const std::filesystem::path& filePath, std::string content);
    void WriteFileBytesSync(const std::filesystem::path& filePath, const std::vector<unsigned char>& fileContent);
    std::optional<std::string> GetMillenniumPreloadPath();
}