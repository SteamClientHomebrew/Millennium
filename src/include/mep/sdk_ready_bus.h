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

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mep
{

struct sdk_ready_event
{
    std::string sdk_version;
    std::string millennium_version;

    int api_total = 0;
    std::vector<std::string> api_missing;
};

class sdk_ready_bus
{
  public:
    using listener_fn = std::function<void(const sdk_ready_event&)>;

    static sdk_ready_bus& instance();

    void notify(const sdk_ready_event& ev);

    /* if sdk.ready already fired before this call, the listener is invoked immediately. */
    int add_listener(listener_fn fn);
    void remove_listener(int id);

    std::optional<sdk_ready_event> get_last() const;

  private:
    sdk_ready_bus() = default;

    mutable std::mutex m_mutex;
    std::unordered_map<int, listener_fn> m_listeners;
    std::optional<sdk_ready_event> m_last;
    std::atomic<int> m_id_counter{ 0 };
};

} // namespace mep
