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

#include "__builtins__/config.h"
#include <__builtins__/default_settings.h>
#include <env.h>
#include <internal_logger.h>

ConfigManager& ConfigManager::Instance()
{
    static ConfigManager instance;
    return instance;
}

nlohmann::json ConfigManager::GetNested(const std::string& path, const nlohmann::json& def)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    const nlohmann::json* current = &_data;
    size_t start = 0, end = 0;

    while ((end = path.find('.', start)) != std::string::npos) {
        std::string key = path.substr(start, end - start);
        if (!current->contains(key))
            return def;
        current = &(*current)[key];
        start = end + 1;
    }

    std::string last_key = path.substr(start);
    if (!current->contains(last_key))
        return def;
    return (*current)[last_key];
}

void ConfigManager::SetNested(const std::string& path, const nlohmann::json& value, bool skipPropagation)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    nlohmann::json* current = &_data;
    size_t start = 0, end = 0;

    while ((end = path.find('.', start)) != std::string::npos) {
        std::string key = path.substr(start, end - start);
        if (!current->contains(key) || !(*current)[key].is_object())
            (*current)[key] = nlohmann::json::object();

        current = &(*current)[key];
        start = end + 1;
    }

    std::string last_key = path.substr(start);
    if (!current->is_object()) {
        *current = nlohmann::json::object();
    }

    nlohmann::json old_value = nullptr;
    if (current->contains(last_key)) {
        old_value = (*current)[last_key];
    }

    if (old_value != value) {
        (*current)[last_key] = value;
        NotifyListeners(path, old_value, value);
        if (!skipPropagation)
            SaveToFile();
    }
}

nlohmann::json ConfigManager::Get(const std::string& key, const nlohmann::json& def)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (_data.contains(key))
        return _data[key];
    if (_defaults.contains(key))
        return _defaults[key];
    return def;
}

void ConfigManager::Set(const std::string& key, const nlohmann::json& value, bool skipPropagation)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    nlohmann::json old_value = _data.value(key, nullptr);
    if (old_value != value) {
        _data[key] = value;
        NotifyListeners(key, old_value, value);
        if (!skipPropagation)
            SaveToFile();
    }
}

void ConfigManager::Delete(const std::string& key)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    if (_data.contains(key)) {
        nlohmann::json old_value = _data[key];
        _data.erase(key);
        NotifyListeners(key, old_value, nullptr);
        SaveToFile();
    }
}

void ConfigManager::RegisterListener(Listener listener)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _listeners.push_back(listener);
}

void ConfigManager::UnregisterListener(Listener listener)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _listeners.erase(std::remove_if(_listeners.begin(), _listeners.end(),
                                    [&](const Listener& l)
    {
        return l.target_type() == listener.target_type() && l.target<void(const std::string&, const nlohmann::json&, const nlohmann::json&)>() ==
                                                                listener.target<void(const std::string&, const nlohmann::json&, const nlohmann::json&)>();
    }),
                     _listeners.end());
}

void ConfigManager::MergeDefaults(nlohmann::json& current, const nlohmann::json& incoming, const std::string& path)
{
    for (auto& [key, value] : incoming.items()) {
        std::string fullKey = path.empty() ? key : path + "." + key;
        if (value.is_object()) {
            if (!current.contains(key) || !current[key].is_object()) {
                current[key] = nlohmann::json::object();
            }
            MergeDefaults(current[key], value, fullKey);
        } else {
            if (!current.contains(key)) {
                current[key] = value;
                NotifyListeners(fullKey, nullptr, value);
            }
        }
    }
}

void ConfigManager::LoadFromFile()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    std::ifstream file(_filename);
    if (file.is_open()) {
        try {
            file >> _data;
        } catch (...) {
            std::cerr << "Invalid JSON in config file. Creating empty config.\n";
            _data = nlohmann::json::object();
        }
    } else {
        _data = nlohmann::json::object();
    }

    MergeDefaults(_data, _defaults, "");
    SaveToFile();
}

void ConfigManager::SaveToFile()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    std::ofstream file(_filename);

    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file for writing: {}", _filename);
        return;
    }

    Logger.Log("Saving config");
    file << _data.dump(2);
}

void ConfigManager::SetDefault(const std::string& key, const nlohmann::json& value)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _defaults[key] = value;
}

nlohmann::json ConfigManager::SetAll(const nlohmann::json& newConfig, bool skipPropagation)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    try {
        nlohmann::json old_data = _data;
        _data = newConfig;

        Logger.Log("Config updated via SetAll: {}", _data.dump(4));

        if (!skipPropagation) {
            for (auto& [k, v] : newConfig.items()) {
                NotifyListeners(k, old_data.value(k, nlohmann::json(nullptr)), v);
            }
        }

        SaveToFile();
        return _data;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to set entire config: {}", e.what());
        return nlohmann::json::object();
    }
}

nlohmann::json ConfigManager::GetAll()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    nlohmann::json result = _defaults;
    for (auto& [k, v] : _data.items())
        result[k] = v;
    return result;
}

ConfigManager::ConfigManager()
{
    _filename = GetEnv("MILLENNIUM__CONFIG_PATH") + "/config.json";
    _defaults = GetDefaultConfig();

    LoadFromFile();
}

ConfigManager::~ConfigManager()
{
    SaveToFile();
}

void ConfigManager::NotifyListeners(const std::string& key, const nlohmann::json& old_value, const nlohmann::json& new_value)
{
    for (auto& listener : _listeners) {
        try {
            listener(key, old_value, new_value);
        } catch (...) {
            LOG_ERROR("Listener exception for key: {}", key);
        }
    }
}