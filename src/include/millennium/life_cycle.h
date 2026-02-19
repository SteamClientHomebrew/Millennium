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
#include "millennium/plugin_manager.h"
#include <functional>
#include <string>
#include <mutex>

class backend_event_dispatcher
{
  public:
    backend_event_dispatcher(std::shared_ptr<plugin_manager> plugin_manager) : m_plugin_manager(std::move(plugin_manager))
    {
    }

    enum class backend_ready_event
    {
        BACKEND_LOAD_SUCCESS,
        BACKEND_LOAD_FAILED,
    };

    struct plugin_t
    {
        std::string pluginName;
        backend_ready_event event;

        bool operator==(const plugin_t& other) const
        {
            return pluginName == other.pluginName && event == other.event;
        }

        plugin_t(const std::string& name) : pluginName(name), event(backend_ready_event::BACKEND_LOAD_SUCCESS)
        {
        }
        plugin_t(const std::string& name, backend_ready_event ev) : pluginName(name), event(ev)
        {
        }
    };

    using event_cb = std::function<void()>;

    void on_all_backends_ready(event_cb callback);
    void update();
    void backend_loaded_event_hdlr(plugin_t plugin);
    void backend_unloaded_event_hdlr(plugin_t plugin, bool isShuttingDown);
    void reset();

  private:
    bool are_all_backends_ready();
    std::string str_get_failed_backends();
    std::string str_get_successful_backends();

    enum backend_event
    {
        ON_BACKEND_READY_EVENT
    };

    bool is_ready_for_cb = false;
    std::vector<plugin_t> emittedPlugins;
    std::vector<backend_event> missedEvents;
    std::unordered_map<backend_event, std::vector<event_cb>> listeners;
    std::mutex listenersMutex;

    std::shared_ptr<plugin_manager> m_plugin_manager;
};
