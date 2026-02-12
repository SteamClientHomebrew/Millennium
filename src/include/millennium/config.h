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
#include "nlohmann/json.hpp"
#include "millennium/singleton.h"
#include "millennium/types.h"
#include <functional>
#include <mutex>

class config_manager : public singleton<config_manager>
{
  public:
    friend class singleton<config_manager>;
    using listener = std::function<void(const std::string&, const json&, const json&)>;

    void register_listener(listener listener);
    void unregister_listener(listener listener);

    void load_from_disk();
    void save_to_disk();

    void set_default_config(const std::string& key, const json& value);

    json set_all(const json& newConfig, bool skipPropagation = false);
    json get_all();

    void merge_default_config(json& current, const json& incoming, const std::string& path);

    json get(const std::string& path, const json& def = nullptr);
    void set(const std::string& path, const json& value, bool skipPropagation = false);

    config_manager();
    ~config_manager();

  private:
    void notify_listeners(const std::string& key, const json& old_value, const json& new_value);

    std::recursive_mutex _mutex;
    std::mutex _save_mutex;
    json _data;
    json _defaults;
    std::vector<listener> _listeners;
    std::string _filename;
};

#define CONFIG config_manager::get_instance()
