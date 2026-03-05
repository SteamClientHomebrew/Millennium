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

#pragma once

#include "millennium/cdp_api.h"
#include "millennium/plugin_manager.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace mep
{

struct console_entry
{
    std::string plugin; /* attributed plugin name, or "millennium" for core */
    nlohmann::json raw; /* full CDP Runtime.consoleAPICalled params */
};

class console_capture
{
  public:
    using listener_fn = std::function<void(const console_entry&)>;
    static console_capture& instance();

    /* start listening to console events from the given CDP client. */
    void start(std::shared_ptr<cdp_client> cdp, std::shared_ptr<plugin_manager> pm);

    std::vector<console_entry> get_recent(const std::string& plugin, std::size_t max = 200) const;

    /* get an object's properties via CDP Runtime.getProperties. */
    nlohmann::json get_properties(const std::string& objectId, const std::string& sessionId);

    int add_listener(const std::string& plugin, listener_fn fn);
    void remove_listener(int id);

  private:
    console_capture() = default;

    void on_console_event(const nlohmann::json& params);
    std::string attribute_plugin(const nlohmann::json& params) const;

    static constexpr std::size_t RING_SIZE = 512;

    mutable std::mutex m_ring_mutex;
    std::vector<console_entry> m_ring;
    std::size_t m_write_pos = 0;

    std::mutex m_listener_mutex;
    std::unordered_map<int, std::pair<std::string, listener_fn>> m_listeners;
    std::atomic<int> m_id_counter{ 0 };

    std::shared_ptr<plugin_manager> m_pm;
    std::shared_ptr<cdp_client> m_cdp;
    std::atomic<bool> m_started{ false };
};
} // namespace mep
