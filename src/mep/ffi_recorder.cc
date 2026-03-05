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

#include "mep/ffi_recorder.h"
#include <algorithm>

namespace mep
{
ffi_recorder& ffi_recorder::instance()
{
    static ffi_recorder inst;
    return inst;
}

void ffi_recorder::record(ffi_call_entry entry)
{
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

std::vector<ffi_call_entry> ffi_recorder::get_recent(const std::string& plugin, std::size_t max) const
{
    std::lock_guard<std::mutex> lock(m_ring_mutex);

    std::vector<ffi_call_entry> out;
    out.reserve(std::min(max, m_ring.size()));

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

int ffi_recorder::add_listener(const std::string& plugin, listener_fn fn)
{
    int id = ++m_id_counter;
    std::lock_guard<std::mutex> lock(m_listener_mutex);
    m_listeners[id] = { plugin, std::move(fn) };
    return id;
}

void ffi_recorder::remove_listener(int id)
{
    std::lock_guard<std::mutex> lock(m_listener_mutex);
    m_listeners.erase(id);
}
} // namespace mep
