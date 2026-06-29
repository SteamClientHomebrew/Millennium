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
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mep
{
struct patch_event
{
    std::string event_type;
    std::string plugin_name;
    std::string other_plugin;
    std::string filename;
    std::string detail;
    std::string find_pattern;
    std::string transform_patterns;
    std::string before_text;
    std::string after_text;
    uint64_t timestamp_ms = 0;
};

class patch_stream_recorder
{
  public:
    using listener_fn = std::function<void(const patch_event&)>;

    static patch_stream_recorder& instance();

    void notify(const patch_event& ev);

    std::vector<patch_event> get_recent(const std::string& plugin, std::size_t max = 200) const;

    int add_listener(const std::string& plugin, listener_fn fn);
    void remove_listener(int id);

  private:
    static constexpr std::size_t RING_SIZE = 512;

    patch_stream_recorder() = default;

    mutable std::mutex m_ring_mutex;
    std::vector<patch_event> m_ring;
    std::size_t m_write_pos = 0;

    std::mutex m_listener_mutex;
    std::unordered_map<int, std::pair<std::string, listener_fn>> m_listeners;
    std::atomic<int> m_id_counter{ 0 };
};
} // namespace mep
