/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|_|___|_|_|_|
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
#include "millennium/http_hooks.h"
#include "millennium/cdp_api.h"
#include "millennium/types.h"
#include "millennium/filesystem.h"
#include "millennium/plugin_webkit_store.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>

/**
 * manages isolated javascript contexts for webviews using cdp.
 *
 * creates isolated worlds in each webkit target and exposes a cdp binding
 * so js code in those contexts can make cdp calls back to the backend.
 *
 * automatically discovers new targets and cleans up destroyed ones.
 */
class webkit_world_mgr
{
  public:
    static inline std::string m_context_name = "millennium";

    explicit webkit_world_mgr(std::shared_ptr<cdp_client> client, std::shared_ptr<SettingsStore> settings_store, std::shared_ptr<network_hook_ctl> network_hook_ctl,
                              std::shared_ptr<plugin_webkit_store> plugin_webkit_store);
    ~webkit_world_mgr();

    webkit_world_mgr(const webkit_world_mgr&) = delete;
    webkit_world_mgr& operator=(const webkit_world_mgr&) = delete;

  private:
    struct target_context
    {
        std::string session_id;
        int execution_context_id;
        bool attaching{ false };
    };

    std::shared_ptr<cdp_client> m_client;
    std::shared_ptr<SettingsStore> m_settings_store;
    std::shared_ptr<network_hook_ctl> m_network_hook_ctl;
    std::shared_ptr<plugin_webkit_store> m_plugin_webkit_store;

    std::unordered_map<std::string, target_context> m_attached_targets;
    std::mutex m_targets_mutex;

    /** map execution context id's to their session id's for reliable binding */
    std::unordered_map<int, std::string> m_context_to_session;

    /** track targets with queued/in-progress attachments to prevent duplicates */
    std::unordered_set<std::string> m_attachments_in_flight;
    std::mutex m_inflight_mutex;

    std::atomic<bool> m_shutdown{ false };

    /** separate thread pool for attachment operations to avoid deadlock */
    std::shared_ptr<thread_pool> m_attachment_pool;

    /** kick off discovery and attach to existing targets */
    void initialize();
    void attach_to_existing_targets();
    void attach_to_target(const std::string& target_id);
    void expose_millennium_to_ctx(int context_id, const std::string& session_id, bool can_reload);
    void setup_event_listeners();
    bool is_valid_target_url(const std::string& url) const;
    std::string compile_api_shim();

    void target_create_hdlr(const json& params);
    void target_destroy_hdlr(const json& params);
    void target_change_hdlr(const json& params);
};
