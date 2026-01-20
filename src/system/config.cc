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

#include "millennium/logger.h"
#include "millennium/config.h"
#include "millennium/environment.h"
#include "millennium/plat_msg.h"

#include <fmt/core.h>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif

#include "head/default_cfg.h"

nlohmann::json config_manager::get(const std::string& path, const nlohmann::json& def)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    const nlohmann::json* current = &_data;
    size_t start = 0, end = 0;

    while ((end = path.find('.', start)) != std::string::npos) {
        std::string key = path.substr(start, end - start);
        if (!current->contains(key)) return def;
        current = &(*current)[key];
        start = end + 1;
    }

    std::string last_key = path.substr(start);
    if (!current->contains(last_key)) return def;
    return (*current)[last_key];
}

void config_manager::set(const std::string& path, const nlohmann::json& value, bool skipPropagation)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    nlohmann::json* current = &_data;
    size_t start = 0, end = 0;

    while ((end = path.find('.', start)) != std::string::npos) {
        std::string key = path.substr(start, end - start);
        if (!current->contains(key) || !(*current)[key].is_object()) (*current)[key] = nlohmann::json::object();

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
        notify_listeners(path, old_value, value);
        if (!skipPropagation) save_to_disk();
    }
}

void config_manager::register_listener(listener listener)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _listeners.push_back(listener);
}

void config_manager::unregister_listener(listener _listener)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _listeners.erase(std::remove_if(_listeners.begin(), _listeners.end(),
                                    [&](const listener& l)
    {
        return l.target_type() == _listener.target_type() && l.target<void(const std::string&, const nlohmann::json&, const nlohmann::json&)>() ==
                                                                 _listener.target<void(const std::string&, const nlohmann::json&, const nlohmann::json&)>();
    }),
                     _listeners.end());
}

void config_manager::merge_default_config(nlohmann::json& current, const nlohmann::json& incoming, const std::string& path)
{
    for (auto& [key, value] : incoming.items()) {
        std::string fullKey = path.empty() ? key : path + "." + key;
        if (value.is_object()) {
            if (!current.contains(key) || !current[key].is_object()) {
                current[key] = nlohmann::json::object();
            }
            merge_default_config(current[key], value, fullKey);
        } else {
            if (!current.contains(key)) {
                current[key] = value;
                notify_listeners(fullKey, nullptr, value);
            }
        }
    }
}

void config_manager::load_from_disk()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    /** Clean up any stale temp file from interrupted writes */
    std::string tempFilename = _filename + ".tmp";
    if (std::filesystem::exists(tempFilename)) {
        logger.warn("Found stale temp config file, removing: {}", tempFilename);
        std::filesystem::remove(tempFilename);
    }

    {
        std::ifstream file(_filename);

        if (file.is_open()) {
            try {
                file >> _data;
            } catch (...) {
                logger.warn("Invalid JSON in config file: {}", _filename);
                Plat_ShowMessageBox("Millennium", fmt::format("The config file at '{}' contains invalid JSON and will be reset to defaults.", _filename).c_str(),
                                    MESSAGEBOX_WARNING);
                _data = nlohmann::json::object();
            }
        } else {
            logger.warn("Failed to open config file: {}", _filename);
            Plat_ShowMessageBox("Millennium", fmt::format("The config file at '{}' could not be opened and will be reset to defaults.", _filename).c_str(), MESSAGEBOX_WARNING);
            _data = nlohmann::json::object();
        }
    }

    merge_default_config(_data, _defaults, "");
    save_to_disk();
}

void config_manager::save_to_disk()
{
    // Serialize all file I/O operations to prevent concurrent writes
    std::lock_guard<std::mutex> save_lock(_save_mutex);
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    /**
     * Use atomic write pattern: write to temp file, then rename
     * This prevents corruption if the app is terminated mid-write
     */
    std::string tempFilename = _filename + ".tmp";

    {
        std::ofstream file(tempFilename);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open temp config file for writing: {}", tempFilename);
            return;
        }

        file << _data.dump(2);
        file.flush();

        if (file.fail()) {
            LOG_ERROR("Failed to write config data to temp file: {}", tempFilename);
            file.close();
            std::filesystem::remove(tempFilename);
            return;
        }
    }

    /** atomic rename - either the old file exists or the new one, never partial */
    std::error_code ec;
    std::filesystem::rename(tempFilename, _filename, ec);

    if (ec) {
        LOG_ERROR("Failed to rename temp config file: {} -> {}: {}", tempFilename, _filename, ec.message());
        std::filesystem::remove(tempFilename);
    }
}

void config_manager::set_default_config(const std::string& key, const nlohmann::json& value)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    _defaults[key] = value;
}

nlohmann::json config_manager::set_all(const nlohmann::json& newConfig, bool skipPropagation)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    try {
        nlohmann::json old_data = _data;
        _data = newConfig;

        if (!skipPropagation) {
            for (auto& [k, v] : newConfig.items()) {
                notify_listeners(k, old_data.value(k, nlohmann::json(nullptr)), v);
            }
        }

        save_to_disk();
        return _data;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to set entire config: {}", e.what());
        return nlohmann::json::object();
    }
}

nlohmann::json config_manager::get_all()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    nlohmann::json result = _defaults;
    for (auto& [k, v] : _data.items())
        result[k] = v;
    return result;
}

config_manager::config_manager()
{
    _filename = platform::environment::get("MILLENNIUM__CONFIG_PATH") + "/config.json";
    _defaults = GetDefaultConfig();

    if (!std::filesystem::exists(_filename)) {
        logger.warn("Could not find config file at: {}, attempting to create it...", _filename);
        std::filesystem::create_directories(std::filesystem::path(_filename).parent_path());

        {
            /** create the config file itself */
            std::ofstream file(_filename);
            file << _defaults.dump(4);
        }
    }

    load_from_disk();
}

config_manager::~config_manager()
{
    save_to_disk();
}

void config_manager::notify_listeners(const std::string& key, const nlohmann::json& old_value, const nlohmann::json& new_value)
{
    std::string key_copy = key;
    nlohmann::json old_value_copy = old_value;
    nlohmann::json new_value_copy = new_value;
    std::vector<listener> listeners_copy;

    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        listeners_copy = _listeners;
    }

    for (auto& listener : listeners_copy) {
        try {
            listener(key_copy, old_value_copy, new_value_copy);
        } catch (const nlohmann::json::exception& e) {
            LOG_ERROR("Listener JSON exception for key: {}: {}", key_copy, e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Listener exception for key: {}: {}", key_copy, e.what());
        } catch (...) {
            LOG_ERROR("Listener exception for key: {}", key_copy);
        }
    }
}
