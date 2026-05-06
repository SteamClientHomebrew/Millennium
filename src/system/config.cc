/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

#include <format>
#include <fstream>
#include <chrono>
#include <ctime>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

#include "head/default_cfg.h"

nlohmann::json config_manager::get(std::initializer_list<std::string> segments, const nlohmann::json& def)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    const nlohmann::json* current = &_data;

    auto it = segments.begin();
    for (; it != segments.end(); ++it) {
        if (!current->contains(*it)) return def;
        if (std::next(it) != segments.end()) {
            current = &(*current)[*it];
        } else {
            return (*current)[*it];
        }
    }

    return def;
}

static std::string join_segments(std::initializer_list<std::string> segments)
{
    std::string result;
    for (auto it = segments.begin(); it != segments.end(); ++it) {
        if (it != segments.begin()) result += '.';
        result += *it;
    }
    return result;
}

void config_manager::set(std::initializer_list<std::string> segments, const nlohmann::json& value, bool skipPropagation)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    nlohmann::json* current = &_data;

    auto it = segments.begin();
    auto last = std::prev(segments.end());

    for (; it != last; ++it) {
        if (!current->contains(*it) || !(*current)[*it].is_object()) (*current)[*it] = nlohmann::json::object();
        current = &(*current)[*it];
    }

    if (!current->is_object()) {
        *current = nlohmann::json::object();
    }

    nlohmann::json old_value = nullptr;
    if (current->contains(*last)) {
        old_value = (*current)[*last];
    }

    if (old_value != value) {
        (*current)[*last] = value;
        if (!skipPropagation) {
            notify_listeners(join_segments(segments), old_value, value);
            save_to_disk();
        }
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

    const bool file_exists = std::filesystem::exists(_filename);

    {
        std::ifstream file(_filename);

        if (file.is_open()) {
            try {
                file >> _data;
            } catch (...) {
                logger.warn("Invalid JSON in config file: {}", _filename);
                file.close();

                /**
                 * Preserve the corrupted file before we reset — once save_to_disk
                 * runs it will overwrite the original, and the user has no other
                 * recovery path.
                 */
                std::error_code bec;
                const std::string backup = _filename + ".corrupted." + std::to_string(static_cast<long long>(std::time(nullptr)));
                std::filesystem::copy_file(_filename, backup, std::filesystem::copy_options::overwrite_existing, bec);
                const std::string backup_note = bec ? std::string("backup failed: ") + bec.message() : std::string("a backup has been saved to ") + backup;

                platform::messagebox::show("Millennium", std::format("The config file at '{}' contains invalid JSON and will be reset to defaults ({}).", _filename, backup_note),
                                           platform::messagebox::level::warn);
                _data = nlohmann::json::object();
            }
        } else if (file_exists) {
            logger.warn("Could not open existing config file (likely transient lock): {}. Disabling persistence for this session.", _filename);
            platform::messagebox::show("Millennium",
                                       std::format("Could not open the config file at '{}' (it may be temporarily locked by another process). Your settings on disk are preserved; "
                                                   "defaults will be used until the next launch.",
                                                   _filename),
                                       platform::messagebox::level::warn);
            _data = nlohmann::json::object();
            _save_disabled = true;
        } else {
            /** No file at all — fresh install path; constructor normally creates it first. */
            logger.warn("Config file does not exist: {}", _filename);
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

    if (_save_disabled) return;

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

    /**
     * Atomic rename. either the old file exists or the new one, never partial.
     * On Windows the destination can be momentarily locked by AV/backup/indexer
     * software, so retry a few times with a short backoff before giving up.
     */
    std::error_code ec;
    for (int attempt = 0; attempt < 5; ++attempt) {
        ec.clear();
        std::filesystem::rename(tempFilename, _filename, ec);
        if (!ec) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    LOG_ERROR("Failed to rename temp config file after retries: {} -> {}: {}", tempFilename, _filename, ec.message());
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
    _defaults = head::get_default_config();

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
