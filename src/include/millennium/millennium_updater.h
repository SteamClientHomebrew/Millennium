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
#include "millennium/types.h"
#include "millennium/fwd_decl.h"
#include "millennium/thread_pool.h"
#include <string>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>

class millennium_updater
{
  public:
    millennium_updater();
    void check_for_updates();
    json has_any_updates();
    bool is_pending_restart();
    void update(const std::string& downloadUrl, const size_t downloadSize, bool background);
    void win32_move_old_millennium_version(std::vector<std::string> lockedFiles);
    void win32_update_legacy_shims();
    void cleanup();
    void shutdown();
    void set_ipc_main(std::shared_ptr<ipc_main> ipc_main);

  private:
    static constexpr const char* GITHUB_API_URL = "https://api.github.com/repos/SteamClientHomebrew/Millennium/releases";

    std::string parse_version(const std::string& version);
    void update_impl(const std::string& downloadUrl, const size_t downloadSize);
    std::optional<json> find_asset(const json& release_info);
    void dispatch_progress_to_frontend(std::string status, double progress, bool is_complete);

    mutable std::mutex m_state_mutex;
    bool m_has_updates{ false };
    bool m_has_updated_millennium{ false };
    bool m_update_in_progress{ false };
    json m_latest_version;

    std::shared_ptr<ipc_main> m_ipc_main;
    std::shared_ptr<thread_pool> m_thread_pool;
};
