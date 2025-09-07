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

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

class ConfigManager
{
  public:
    /**
     * A listener function that gets called when a config value changes.
     * @param key The key that changed.
     * @param old_value The old value of the key.
     * @param new_value The new value of the key.
     */
    using Listener = std::function<void(const std::string&, const nlohmann::json&, const nlohmann::json&)>;

    static ConfigManager& Instance();

    /**
     * Set a nested configuration key.
     * @param key The path to the nested key. Ex: "parent.child.key"
     * @param def The default value to return if the key is not found.
     *
     * @note This does not set nested keys. Use SetNested for that. This only works for top-level keys.
     */
    nlohmann::json Get(const std::string& key, const nlohmann::json& def = nullptr);

    /**
     * Set a configuration key.
     * @param key The key to set.
     * @param value The value to set.
     * @param skipPropagation If true, listeners will not be notified.
     *
     * @note This does not set nested keys. Use SetNested for that. This only works for top-level keys.
     */
    void Set(const std::string& key, const nlohmann::json& value, bool skipPropagation = false);

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
    ~ConfigManager();

  private:
    /**
     * Notify all registered listeners of a configuration change.
     */
    void NotifyListeners(const std::string& key, const nlohmann::json& old_value, const nlohmann::json& new_value);

    std::recursive_mutex _mutex;
    nlohmann::json _data;
    nlohmann::json _defaults;
    std::vector<Listener> _listeners;
    std::string _filename;
};

/**
 * Global config manager instance.
 */
#define CONFIG ConfigManager::Instance()