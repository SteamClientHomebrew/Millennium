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

#include "mep/patch_stream_recorder.h"
#include <vector>

namespace mep
{

patch_stream_recorder& patch_stream_recorder::instance()
{
    static patch_stream_recorder inst;
    return inst;
}

void patch_stream_recorder::notify(const patch_event& ev)
{
    {
        std::lock_guard<std::mutex> lock(m_ring_mutex);
        if (m_ring.size() < RING_SIZE)
            m_ring.push_back(ev);
        else
            m_ring[m_write_pos % RING_SIZE] = ev;
        ++m_write_pos;
    }

    std::vector<listener_fn> targets;
    {
        std::lock_guard<std::mutex> lock(m_listener_mutex);
        for (const auto& [id, pair] : m_listeners) {
            const std::string& listener_plugin = pair.first;
            bool deliver = false;

            if (ev.event_type == "url_seen" || ev.event_type == "syntax_error") {
                deliver = true;
            } else if (ev.event_type == "conflict") {
                deliver = (listener_plugin == ev.plugin_name || listener_plugin == ev.other_plugin);
            } else {
                deliver = (listener_plugin == ev.plugin_name);
            }

            if (deliver) targets.push_back(pair.second);
        }
    }

    for (const auto& fn : targets) {
        try {
            fn(ev);
        } catch (...) {
        }
    }
}

std::vector<patch_event> patch_stream_recorder::get_recent(const std::string& plugin, std::size_t max) const
{
    std::lock_guard<std::mutex> lock(m_ring_mutex);

    std::vector<patch_event> out;
    out.reserve(std::min(max, m_ring.size()));

    const std::size_t total = m_ring.size();
    const std::size_t count = std::min(max, total);
    const std::size_t start = (total < RING_SIZE) ? 0 : (m_write_pos % RING_SIZE);

    for (std::size_t i = 0; i < total; ++i) {
        const auto& e = m_ring[(start + i) % total];

        bool include = false;
        if (e.event_type == "url_seen" || e.event_type == "syntax_error")
            include = true;
        else if (e.event_type == "conflict")
            include = (e.plugin_name == plugin || e.other_plugin == plugin);
        else
            include = (e.plugin_name == plugin);

        if (include) {
            out.push_back(e);
            if (out.size() >= count) break;
        }
    }

    return out;
}

int patch_stream_recorder::add_listener(const std::string& plugin, listener_fn fn)
{
    int id = ++m_id_counter;
    std::lock_guard<std::mutex> lock(m_listener_mutex);
    m_listeners[id] = { plugin, std::move(fn) };
    return id;
}

void patch_stream_recorder::remove_listener(int id)
{
    std::lock_guard<std::mutex> lock(m_listener_mutex);
    m_listeners.erase(id);
}

} // namespace mep
