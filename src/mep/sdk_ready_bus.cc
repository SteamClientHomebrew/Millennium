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

#include "mep/sdk_ready_bus.h"

namespace mep
{

sdk_ready_bus& sdk_ready_bus::instance()
{
    static sdk_ready_bus inst;
    return inst;
}

void sdk_ready_bus::notify(const sdk_ready_event& ev)
{
    std::vector<listener_fn> targets;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_last = ev;
        for (const auto& [id, fn] : m_listeners) {
            targets.push_back(fn);
        }
    }
    for (const auto& fn : targets) {
        try {
            fn(ev);
        } catch (...) {
        }
    }
}

int sdk_ready_bus::add_listener(listener_fn fn)
{
    std::optional<sdk_ready_event> replay;
    int id;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        id = ++m_id_counter;
        m_listeners[id] = fn;
        replay = m_last;
    }
    /* replay immediately if the event already fired before this listener registered */
    if (replay) {
        try {
            fn(*replay);
        } catch (...) {
        }
    }
    return id;
}

void sdk_ready_bus::remove_listener(int id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_listeners.erase(id);
}

std::optional<sdk_ready_event> sdk_ready_bus::get_last() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_last;
}

} // namespace mep
