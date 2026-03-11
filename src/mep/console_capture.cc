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

#include "mep/console_capture.h"
#include <algorithm>

namespace mep
{
console_capture& console_capture::instance()
{
    static console_capture inst;
    return inst;
}

void console_capture::start(std::shared_ptr<cdp_client> cdp, std::shared_ptr<plugin_manager> pm)
{
    m_pm = std::move(pm);
    m_cdp = cdp;
    cdp->on("Runtime.consoleAPICalled", [this](const nlohmann::json& params) { on_console_event(params); });
    m_started.store(true);
}

nlohmann::json console_capture::get_properties(const std::string& objectId, const std::string& sessionId)
{
    if (!m_cdp || !m_cdp->is_active()) {
        return {
            { "error", "CDP client not available" }
        };
    }

    nlohmann::json params = {
        { "objectId",        objectId },
        { "ownProperties",   true     },
        { "generatePreview", true     },
    };

    try {
        return m_cdp->send_host("Runtime.getProperties", params, sessionId).get();
    } catch (const std::exception& e) {
        return {
            { "error", e.what() }
        };
    }
}

void console_capture::on_console_event(const nlohmann::json& params)
{
    console_entry entry;
    entry.plugin = attribute_plugin(params);
    entry.raw = params;

    {
        std::lock_guard<std::mutex> lock(m_ring_mutex);
        if (m_ring.size() < RING_SIZE) {
            m_ring.push_back(entry);
        } else {
            m_ring[m_write_pos % RING_SIZE] = entry;
        }
        ++m_write_pos;
    }

    std::vector<listener_fn> targets;
    {
        std::lock_guard<std::mutex> lock(m_listener_mutex);
        for (const auto& [id, pair] : m_listeners) {
            if (pair.first == entry.plugin) {
                targets.push_back(pair.second);
            }
        }
    }

    for (const auto& fn : targets) {
        try {
            fn(entry);
        } catch (...) {
        }
    }
}

std::string console_capture::attribute_plugin(const nlohmann::json& params) const
{
    if (!m_pm) return "millennium";

    if (params.contains("stackTrace") && params["stackTrace"].contains("callFrames")) {
        const auto plugins = m_pm->get_all_plugins();

        for (const auto& frame : params["stackTrace"]["callFrames"]) {
            if (!frame.contains("url") || !frame["url"].is_string()) continue;
            const std::string url = frame["url"].get<std::string>();

            for (const auto& p : plugins) {
                if (p.is_internal) continue;
                const std::string base = p.plugin_base_dir.generic_string();
                if (!base.empty() && url.find(base) != std::string::npos) {
                    return p.plugin_name;
                }
            }
        }
    }

    return "millennium";
}

std::vector<console_entry> console_capture::get_recent(const std::string& plugin, std::size_t max) const
{
    std::lock_guard<std::mutex> lock(m_ring_mutex);

    std::vector<console_entry> out;
    out.reserve((std::min)(max, m_ring.size()));

    const std::size_t total = m_ring.size();
    const std::size_t start = (m_write_pos > 0) ? m_write_pos - 1 : 0;

    for (std::size_t i = 0; i < total && out.size() < max; ++i) {
        std::size_t idx = (total < RING_SIZE) ? (total - 1 - i) : ((start - i + RING_SIZE) % RING_SIZE);
        const auto& e = m_ring[idx];
        if (e.plugin == plugin) {
            out.push_back(e);
        }
    }

    std::reverse(out.begin(), out.end());
    return out;
}

int console_capture::add_listener(const std::string& plugin, listener_fn fn)
{
    int id = ++m_id_counter;
    std::lock_guard<std::mutex> lock(m_listener_mutex);
    m_listeners[id] = { plugin, std::move(fn) };
    return id;
}

void console_capture::remove_listener(int id)
{
    std::lock_guard<std::mutex> lock(m_listener_mutex);
    m_listeners.erase(id);
}
} // namespace mep
