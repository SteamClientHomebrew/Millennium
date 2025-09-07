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
    using Listener = std::function<void(const std::string&, const nlohmann::json&, const nlohmann::json&)>;

    static ConfigManager& Instance();

    nlohmann::json Get(const std::string& key, const nlohmann::json& def = nullptr);
    void Set(const std::string& key, const nlohmann::json& value, bool skipPropagation = false);
    void Delete(const std::string& key);

    void RegisterListener(Listener listener);
    void UnregisterListener(Listener listener);

    void LoadFromFile();
    void SaveToFile();

    void SetDefault(const std::string& key, const nlohmann::json& value);
    nlohmann::json SetAll(const nlohmann::json& newConfig, bool skipPropagation = false);
    nlohmann::json GetAll();

    void MergeDefaults(nlohmann::json& current, const nlohmann::json& incoming, const std::string& path);

    nlohmann::json GetNested(const std::string& path, const nlohmann::json& def = nullptr);
    void SetNested(const std::string& path, const nlohmann::json& value, bool skipPropagation = false);

    ConfigManager();
    ~ConfigManager();

  private:
    void NotifyListeners(const std::string& key, const nlohmann::json& old_value, const nlohmann::json& new_value);

    std::recursive_mutex _mutex;
    nlohmann::json _data;
    nlohmann::json _defaults;
    std::vector<Listener> _listeners;
    std::string _filename;
};

#define CONFIG ConfigManager::Instance()