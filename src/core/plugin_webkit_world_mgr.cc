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

#include "millennium/ffi_binder.h"
#include "millennium/http_hooks.h"
#include "millennium/plugin_webkit_world_mgr.h"
#include "millennium/plugin_webkit_store.h"
#include "millennium/logger.h"
#include "millennium/auth.h"
#include "millennium/urlp.h"
#include <fmt/format.h>

webkit_world_mgr::webkit_world_mgr(std::shared_ptr<cdp_client> client, std::shared_ptr<SettingsStore> settings_store, std::shared_ptr<network_hook_ctl> network_hook_ctl,
                                   std::shared_ptr<plugin_webkit_store> plugin_webkit_store)
    : m_client(std::move(client)), m_settings_store(std::move(settings_store)), m_network_hook_ctl(std::move(network_hook_ctl)),
      m_plugin_webkit_store(std::move(plugin_webkit_store))
{
    initialize();
}

webkit_world_mgr::~webkit_world_mgr()
{
    m_shutdown.store(true, std::memory_order_release);

    /** unregister all event listeners to prevent new callbacks */
    m_client->off("Target.targetCreated");
    m_client->off("Target.targetDestroyed");
    m_client->off("Target.targetInfoChanged");
    m_client->off("Runtime.executionContextCreated");

    Logger.Log("Successfully shut down webkit_world_mgr...");
}

void webkit_world_mgr::initialize()
{
    /** setup event listeners FIRST before any cdp commands to avoid race conditions */
    setup_event_listeners();

    const json discover_params = {
        { "discover", true }
    };

    try {
        m_client->send_host("Target.setDiscoverTargets", discover_params).get();
    } catch (const std::exception& e) {
        LOG_ERROR("webkit_world_mgr: failed to enable target discovery: {}", e.what());
        throw;
    }

    attach_to_existing_targets();
}

bool webkit_world_mgr::is_valid_target_url(const std::string& url) const
{
    if (url.empty()) {
        return false;
    }

    if (url.find("http://") != 0 && url.find("https://") != 0) {
        return false;
    }

    if (url.find("https://steamloopback.host/index.html") == 0) {
        return false;
    }

    return true;
}

void webkit_world_mgr::attach_to_existing_targets()
{
    try {
        auto result = m_client->send_host("Target.getTargets", json::object()).get();

        if (!result.contains("targetInfos") || !result["targetInfos"].is_array()) {
            LOG_ERROR("webkit_world_mgr: invalid response from Target.getTargets - missing or invalid targetInfos");
            return;
        }

        const auto& target_infos = result["targetInfos"];

        for (const auto& target_info : target_infos) {
            if (!target_info.contains("canAccessOpener") || !target_info.contains("url") || !target_info.contains("targetId")) {
                continue;
            }

            /** skip targets that can access their opener, they are Steam UI windows spawned by the SharedJS context */
            if (target_info["canAccessOpener"].get<bool>()) {
                continue;
            }

            std::string url = target_info["url"].get<std::string>();
            if (!is_valid_target_url(url)) {
                continue;
            }

            std::string target_id = target_info["targetId"].get<std::string>();

            attach_to_target(target_id);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("webkit_world_mgr: exception while attaching to existing targets: {}", e.what());
    }
}

void webkit_world_mgr::attach_to_target(const std::string& target_id)
{
    if (m_shutdown.load(std::memory_order_acquire)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_targets_mutex);
        auto [it, inserted] = m_attached_targets.try_emplace(target_id, target_context{ "", -1, true });

        if (!inserted) {
            if (it->second.attaching || !it->second.session_id.empty()) {
                return;
            }
            it->second.attaching = true;
        }
    }

    try {
        const json attach_params = {
            { "targetId", target_id },
            { "flatten",  true      }
        };

        auto attach_result = m_client->send_host("Target.attachToTarget", attach_params).get();

        if (!attach_result.contains("sessionId") || !attach_result["sessionId"].is_string()) {
            LOG_ERROR("webkit_world_mgr: no valid sessionId in attachToTarget response for {}", target_id);
            std::lock_guard<std::mutex> lock(m_targets_mutex);
            auto it = m_attached_targets.find(target_id);
            if (it != m_attached_targets.end()) {
                it->second.attaching = false;
            }
            return;
        }

        std::string session_id = attach_result["sessionId"].get<std::string>();

        m_client->send_host("Runtime.enable", json::object(), session_id).get();
        m_client->send_host("Page.enable", json::object(), session_id).get();

        /** bypass CSP to allow dynamic imports from FTP server */
        const json disable_csp_params = {
            { "enabled", true }
        };
        m_client->send_host("Page.setBypassCSP", disable_csp_params, session_id).get();

        auto frame_tree_result = m_client->send_host("Page.getFrameTree", json::object(), session_id).get();

        if (!frame_tree_result.contains("frameTree") || !frame_tree_result["frameTree"].contains("frame") || !frame_tree_result["frameTree"]["frame"].contains("id")) {
            LOG_ERROR("webkit_world_mgr: invalid frameTree response for target {}", target_id);
            std::lock_guard<std::mutex> lock(m_targets_mutex);
            auto it = m_attached_targets.find(target_id);
            if (it != m_attached_targets.end()) {
                it->second.attaching = false;
            }
            return;
        }

        std::string frame_id = frame_tree_result["frameTree"]["frame"]["id"].get<std::string>();

        const json create_world_params = {
            { "frameId",             frame_id       },
            { "worldName",           m_context_name },
            { "grantUniveralAccess", true           }
        };
        auto isolated_world_result = m_client->send_host("Page.createIsolatedWorld", create_world_params, session_id).get();

        if (!isolated_world_result.contains("executionContextId") || !isolated_world_result["executionContextId"].is_number()) {
            LOG_ERROR("webkit_world_mgr: no valid executionContextId in createIsolatedWorld response for target {}", target_id);
            std::lock_guard<std::mutex> lock(m_targets_mutex);
            auto it = m_attached_targets.find(target_id);
            if (it != m_attached_targets.end()) {
                it->second.attaching = false;
            }
            return;
        }

        int execution_context_id = isolated_world_result["executionContextId"].get<int>();

        /** check if this is a top-level target (can reload) by checking the target info */
        bool is_top_level = true;
        if (frame_tree_result["frameTree"]["frame"].contains("parentId")) {
            is_top_level = false; /** has parent frame = nested target */
        }

        {
            std::lock_guard<std::mutex> lock(m_targets_mutex);
            m_attached_targets[target_id] = target_context{ session_id, execution_context_id, false };
        }
        expose_millennium_to_ctx(execution_context_id, session_id, is_top_level);

    } catch (const std::exception& e) {
        LOG_ERROR("webkit_world_mgr: exception while attaching to target {}: {}", target_id, e.what());
        std::lock_guard<std::mutex> lock(m_targets_mutex);
        auto it = m_attached_targets.find(target_id);
        if (it != m_attached_targets.end()) {
            it->second.attaching = false;
        }
    }
}

std::string webkit_world_mgr::compile_api_shim()
{
    std::string m_api_shim_script = R"(
/**
 * This file is auto-generated by Millennium
 * {location}
 */
console.log("%cMillennium%c loading webkit modules...", "background: white; color: black; padding: 2px 6px; border-radius: 6px;", "");

import('{ftpPath}')
.then(module => (new module.default).startWebkitPreloader('{token}', [{plugins}], [{legacy_shims}], [{ctx_shims}], '{ftp_base}'))
.catch((error) => console.error("%cMillennium%c failed to load webkit modules.", "background: black; color: red; padding: 2px 6px; border-radius: 2px;", "", error));
)";

    const std::string m_ftp_url = m_network_hook_ctl->get_ftp_url();
    const std::vector<plugin_webkit_store::item> webkit_items = m_plugin_webkit_store->get();
    const std::vector<SettingsStore::plugin_t> plist = m_settings_store->GetEnabledPlugins();

    /**
     * Separate isolated and non-isolated webkit modules
     * Isolated modules (webkitApiVersion 2.0.0) are loaded into their own isolated world
     *
     * I decided to maintain backwards compatibility for non-isolated modules as there are unintended
     * side-effects to changing the execution context of existing plugins without notice.
     */
    auto [isolated, non_isolated] = std::accumulate(webkit_items.begin(), webkit_items.end(), std::pair<std::string, std::string>{}, [m_ftp_url](auto acc, const auto& item)
    {
        auto& [iso, non_iso] = acc;
        auto& target = item.should_isolate ? iso : non_iso;

        if (!target.empty()) target += ",";
        target += fmt::format("\"{}\"", UrlFromPath(m_ftp_url, item.abs_webkit_path.generic_string()));

        return acc;
    });

    const std::string plugins = std::accumulate(plist.begin(), plist.end(), std::string{}, [](auto acc, auto& p) { return acc + fmt::format("'{}',", p.pluginName); });
    const std::string location = GET_GITHUB_URL_FROM_HERE();
    const std::string token = GetAuthToken();
    const std::string ftp_path = m_ftp_url + SystemIO::GetMillenniumPreloadPath();
    const std::string ftp_base = m_ftp_url + GetScrambledApiPathToken();

    return fmt::format(                         //
        m_api_shim_script,                      //
        fmt::arg("location", location),         //
        fmt::arg("ftpPath", ftp_path),          //
        fmt::arg("token", token),               //
        fmt::arg("plugins", plugins),           //
        fmt::arg("legacy_shims", non_isolated), //
        fmt::arg("ctx_shims", isolated),        //
        fmt::arg("ftp_base", ftp_base)          //
    );
}

void webkit_world_mgr::expose_millennium_to_ctx(int context_id, const std::string& session_id, bool can_reload)
{
    try {
        /** add binding for initial context */
        const json add_binding_params = {
            { "name",               ffi_constants::binding_name },
            { "executionContextId", context_id                  }
        };
        m_client->send_host("Runtime.addBinding", add_binding_params, session_id).get();

        /** register script to run on every navigation */
        const json add_script_params = {
            { "source",    this->compile_api_shim() },
            { "worldName", m_context_name           }
        };
        m_client->send_host("Page.addScriptToEvaluateOnNewDocument", add_script_params, session_id).get();

        if (can_reload) {
            /** reload page to apply CSP bypass and run script */
            m_client->send_host("Page.reload", json::object(), session_id).get();
            Logger.Log("webkit_world_mgr: reloaded page for context {} (top-level target)", context_id);
        } else {
            /** nested target - can't reload, just evaluate immediately (CSP bypass already applied) */
            const json evaluate_params = {
                { "contextId",  context_id               },
                { "expression", this->compile_api_shim() }
            };
            m_client->send_host("Runtime.evaluate", evaluate_params, session_id).get();
            Logger.Log("webkit_world_mgr: evaluated script in context {} (nested target, no reload)", context_id);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("webkit_world_mgr: failed to expose millennium to context {}: {}", context_id, e.what());
    }
}

void webkit_world_mgr::target_create_hdlr(const json& params)
{
    if (!params.contains("targetInfo")) {
        LOG_ERROR("webkit_world_mgr: Target.targetCreated missing targetInfo");
        return;
    }

    const auto& target_info = params["targetInfo"];
    if (!target_info.contains("canAccessOpener") || !target_info.contains("url") || !target_info.contains("targetId")) return;
    if (target_info["canAccessOpener"].get<bool>()) return;

    std::string url = target_info["url"].get<std::string>();
    std::string target_id = target_info["targetId"].get<std::string>();

    if (!is_valid_target_url(url)) return;
    if (m_shutdown.load(std::memory_order_acquire)) return;
    {
        std::lock_guard<std::mutex> lock(m_inflight_mutex);
        if (m_attachments_in_flight.count(target_id)) {
            return;
        }
        m_attachments_in_flight.insert(target_id);
    }
    {
        if (m_shutdown.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(m_inflight_mutex);
            m_attachments_in_flight.erase(target_id);
            return;
        }
        attach_to_target(target_id);

        std::lock_guard<std::mutex> lock(m_inflight_mutex);
        m_attachments_in_flight.erase(target_id);
    }
}

void webkit_world_mgr::target_destroy_hdlr(const json& params)
{
    if (!params.contains("targetId")) {
        LOG_ERROR("webkit_world_mgr: Target.targetDestroyed missing targetId");
        return;
    }

    std::string target_id = params["targetId"].get<std::string>();

    std::lock_guard<std::mutex> lock(m_targets_mutex);
    auto it = m_attached_targets.find(target_id);

    if (it != m_attached_targets.end()) {
        m_attached_targets.erase(it);
    }
}

void webkit_world_mgr::target_change_hdlr(const json& params)
{
    if (!params.contains("targetInfo") || !params["targetInfo"].contains("targetId")) {
        return;
    }

    const auto& target_info = params["targetInfo"];
    std::string target_id = target_info["targetId"].get<std::string>();
    bool should_attach = false;

    if (target_info.contains("canAccessOpener") && target_info["canAccessOpener"].get<bool>()) {
        return;
    }

    if (target_info.contains("url") && target_info["url"].is_string()) {
        std::string url = target_info["url"].get<std::string>();
        should_attach = is_valid_target_url(url);
    }

    if (!should_attach) return;
    {
        std::lock_guard<std::mutex> lock(m_targets_mutex);
        auto it = m_attached_targets.find(target_id);
        if (it != m_attached_targets.end() && !it->second.session_id.empty()) {
            return;
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_inflight_mutex);
        if (m_attachments_in_flight.count(target_id)) return;
        m_attachments_in_flight.insert(target_id);
    }

    if (m_shutdown.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(m_inflight_mutex);
        m_attachments_in_flight.erase(target_id);
        return;
    }
    {
        if (m_shutdown.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(m_inflight_mutex);
            m_attachments_in_flight.erase(target_id);
            return;
        }
        attach_to_target(target_id);

        std::lock_guard<std::mutex> lock(m_inflight_mutex);
        m_attachments_in_flight.erase(target_id);
    }
}

void webkit_world_mgr::setup_event_listeners()
{
    m_client->on("Target.targetCreated", std::bind(&webkit_world_mgr::target_create_hdlr, this, std::placeholders::_1));
    m_client->on("Target.targetDestroyed", std::bind(&webkit_world_mgr::target_destroy_hdlr, this, std::placeholders::_1));
    m_client->on("Target.targetInfoChanged", std::bind(&webkit_world_mgr::target_change_hdlr, this, std::placeholders::_1));
}
