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
#include "singleton.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

class SettingsStore
{
  public:
    enum PluginBackendType
    {
        Python,
        Lua
    };

    static constexpr const char* pluginConfigFile = "plugin.json";

    struct PluginTypeSchema
    {
        std::string pluginName;
        nlohmann::basic_json<> pluginJson;
        std::filesystem::path pluginBaseDirectory;
        std::filesystem::path backendAbsoluteDirectory;
        std::filesystem::path frontendAbsoluteDirectory;
        std::filesystem::path webkitAbsolutePath;
        PluginBackendType backendType;
        bool isInternal = false;
    };

    static std::vector<PluginTypeSchema> ParseAllPlugins();
    static std::vector<PluginTypeSchema> GetEnabledBackends();
    static std::vector<PluginTypeSchema> GetEnabledPlugins();
    static std::vector<std::string> GetEnabledPluginNames();

    static bool IsEnabledPlugin(std::string pluginName);
    static bool TogglePluginStatus(std::string pluginName, bool enabled);

    static int InitializeSettingsStore();
    SettingsStore();

  private:
    static void LintPluginData(nlohmann::json json, std::string pluginName);
    static PluginTypeSchema GetPluginInternalData(nlohmann::json json, std::filesystem::directory_entry entry);
};

namespace SystemIO
{
class FileException : public std::exception
{
  public:
    explicit FileException(std::string message) : msg(std::move(message))
    {
    }
    [[nodiscard]] virtual const char* what() const noexcept override
    {
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
std::string GetMillenniumPreloadPath();
void SafePurgeDirectory(const std::filesystem::path& root);
void MakeWritable(const std::filesystem::path& p);
bool DeleteFolder(const std::filesystem::path& p);
} // namespace SystemIO

class ConfigManager : public Singleton<ConfigManager>
{
    friend class Singleton;
  public:
    /**
     * A listener function that gets called when a config value changes.
     * @param key The key that changed.
     * @param old_value The old value of the key.
     * @param new_value The new value of the key.
     */
    using Listener = std::function<void(const std::string&, const nlohmann::json&, const nlohmann::json&)>;

    /**
     * Delete a configuration key.
     * @note This does not delete nested keys. Use SetNested with a null value instead.
     */
    void Delete(const std::string& key);

    /**
     * Register a listener to be notified of configuration changes.
     * @note Listeners are called with the key, old value, and new value.
     */
    void RegisterListener(Listener listener);

    /**
     * Unregister a previously registered listener.
     */
    void UnregisterListener(Listener listener);

    /**
     * Load the configuration from the file specified in _filename.
     */
    void LoadFromFile();

    /**
     * Save the current configuration to the file specified in _filename.
     */
    void SaveToFile();

    /**
     * Set the default value for a configuration key.
     * @note this does not overwrite existing values, only sets the default if it doesn't exist.
     */
    void SetDefault(const std::string& key, const nlohmann::json& value);

    /**
     * Overwrite the entire config with a new json object. no merging is done, its entirely replace.
     * @note No checks are done to ensure the new config is valid. If you fuck it up, you fucked it up.
     */
    nlohmann::json SetAll(const nlohmann::json& newConfig, bool skipPropagation = false);

    /**
     * Get the entire config as its stored on disk.
     * @note keep in mind subclasses and overrides aren't the same class instance and may not reflect the current state.
     */
    nlohmann::json GetAll();

    /**
     * Used to set the default config values.
     * @note this merges without replacing.
     */
    void MergeDefaults(nlohmann::json& current, const nlohmann::json& incoming, const std::string& path);

    /**
     * Get a nested configuration key.
     * @param path The path to the nested key. Ex: "parent.child.key"
     * @param def The default value to return if the key is not found.
     */
    nlohmann::json GetNested(const std::string& path, const nlohmann::json& def = nullptr);

    /**
     * Set a nested configuration key.
     * @param path The path to the nested key. Ex: "parent.child.key"
     * @param value The value to set.
     * @param skipPropagation If true, listeners will not be notified.
     */
    void SetNested(const std::string& path, const nlohmann::json& value, bool skipPropagation = false);

    ConfigManager();
    virtual ~ConfigManager() override;

  private:
    /**
     * Notify all registered listeners of a configuration change.
     */
    void NotifyListeners(const std::string& key, const nlohmann::json& old_value, const nlohmann::json& new_value);

    std::recursive_mutex _mutex;
    std::mutex _save_mutex;
    nlohmann::json _data;
    nlohmann::json _defaults;
    std::vector<Listener> _listeners;
    std::string _filename;
};

/**
 * Global config manager instance.
 */
#define CONFIG ConfigManager::GetInstance()
