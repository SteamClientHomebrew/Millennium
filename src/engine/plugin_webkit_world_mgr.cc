/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|_|___|_|_|_|
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

#include "millennium/ffi_binder.h"
#include "millennium/filesystem.h"
#include "millennium/http_hooks.h"
#include "millennium/plugin_webkit_world_mgr.h"
#include "millennium/plugin_webkit_store.h"
#include "millennium/logger.h"
#include "millennium/auth.h"
#include "millennium/url_parser.h"
#include <format>

webkit_world_mgr::webkit_world_mgr(std::shared_ptr<cdp_client> client, std::shared_ptr<plugin_manager> plugin_manager, std::shared_ptr<network_hook_ctl> network_hook_ctl,
                                   std::shared_ptr<plugin_webkit_store> plugin_webkit_store)
    : m_client(std::move(client)), m_plugin_manager(std::move(plugin_manager)), m_network_hook_ctl(std::move(network_hook_ctl)),
      m_plugin_webkit_store(std::move(plugin_webkit_store))
{
    initialize();
}

webkit_world_mgr::~webkit_world_mgr()
{
    m_shutdown.store(true, std::memory_order_release);

    /** unregister all event listeners to prevent new callbacks */
    for (int token : m_listener_tokens) {
        m_client->off(token);
    }

    logger.log("Successfully shut down webkit_world_mgr...");
}

void webkit_world_mgr::initialize()
{
    setup_event_listeners();

    const json discover_params = {
        { "discover", true }
    };

    try {
        m_client->send_host("Target.setDiscoverTargets", discover_params);
    } catch (const std::exception& e) {
        LOG_ERROR("webkit_world_mgr: failed to send setDiscoverTargets: {}", e.what());
    }
}

static bool is_steam_owned_url(const std::string& url)
{
    /* extract just the hostname: skip scheme, stop at port / path / query */
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) return false;
    auto host_start = scheme_end + 3;
    auto host_end = url.find_first_of(":/?#", host_start);
    std::string host = url.substr(host_start, host_end == std::string::npos ? std::string::npos : host_end - host_start);

    if (host == k_steam_loopback) return true;
    for (const auto* tld : k_steam_tlds) {
        const size_t tld_len = std::strlen(tld);
        if (host == tld) return true;
        if (host.size() > tld_len + 1 && host[host.size() - tld_len - 1] == '.' && host.compare(host.size() - tld_len, tld_len, tld) == 0)
            return true;
    }
    return false;
}

bool webkit_world_mgr::is_valid_target_url(const std::string& url) const
{
    if (url.empty()) {
        return false;
    }

    // only web protocols are supported
    if (url.find("http://") != 0 && url.find("https://") != 0) {
        return false;
    }

    // steamloopback is the main UI, handled natively by SharedJSContext
    if (url.find("https://steamloopback.host/") == 0) {
        return false;
    }

    // forbid URLs matching the global blacklist (e.g. checkout pages)
    for (const auto& pattern : g_js_hook_blacklist) {
        if (std::regex_match(url, pattern)) {
            return false;
        }
    }

    // allow all steam-owned popups/frames (e.g. store, community)
    if (is_steam_owned_url(url)) {
        return true;
    }

    // allow external URLs if explicitly hooked by a plugin or theme (e.g. via add_browser_js / add_browser_css in the backend)
    const auto hookList = m_network_hook_ctl->get_hook_list();
    return std::any_of(hookList.begin(), hookList.end(), [&url](const auto& hook)
    {
        return std::regex_match(url, hook.hook.url_pattern);
    });
}

void webkit_world_mgr::attach_to_target(const std::string& target_id, const std::string& url)
{
    if (m_shutdown.load(std::memory_order_acquire)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_targets_mutex);
        auto [it, inserted] = m_attached_targets.try_emplace(target_id, target_context{ "", "", true });

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

        if (!frame_tree_result.contains("frameTree") || !frame_tree_result["frameTree"].contains("frame")) {
            LOG_ERROR("webkit_world_mgr: invalid frameTree response for target {}", target_id);
            std::lock_guard<std::mutex> lock(m_targets_mutex);
            auto it = m_attached_targets.find(target_id);
            if (it != m_attached_targets.end()) {
                it->second.attaching = false;
            }
            return;
        }

        /**
         * reload only if top level and steam owned. extern pages: steamdb, csstats, etc, may break on reloads.
         * it seems this reload interferes with some versions of CF turnstile.
         */
        bool is_top_level = !frame_tree_result["frameTree"]["frame"].contains("parentId");
        bool can_reload = is_top_level && is_steam_owned_url(url);

        {
            std::lock_guard<std::mutex> lock(m_targets_mutex);
            m_attached_targets[target_id] = target_context{ session_id, "", false };
        }
        expose_millennium_to_ctx(session_id, can_reload);

    } catch (const std::exception& e) {
        /** if the target died before we could attach, that's totally fine. */
        if (std::string(e.what()) != "No target with given id found") {
            LOG_ERROR("webkit_world_mgr: exception while attaching to target {}: {}", target_id, e.what());
        }

        std::lock_guard<std::mutex> lock(m_targets_mutex);
        auto it = m_attached_targets.find(target_id);
        if (it != m_attached_targets.end()) {
            it->second.attaching = false;
        }
    }
}

std::string webkit_world_mgr::compile_api_shim()
{
    constexpr const char* m_api_shim_script = R"(
/**
 * This file is auto-generated by Millennium
 * {0}
 */
console.log("%cMillennium%c loading webkit modules...", "background: white; color: black; padding: 2px 6px; border-radius: 6px;", "");

import('{1}')
.then(module => (new module.default).startBrowser([{2}], [{3}], [{4}], '{5}'))
.catch((error) => console.error("%cMillennium%c failed to load webkit modules.", "background: black; color: red; padding: 2px 6px; border-radius: 2px;", "", error));
)";

    const std::string m_ftp_url = m_network_hook_ctl->get_ftp_url();
    const std::vector<plugin_webkit_store::item> webkit_items = m_plugin_webkit_store->get();
    const std::vector<plugin_manager::plugin_t> plist = m_plugin_manager->get_enabled_plugins();

    const std::string modules = std::accumulate(webkit_items.begin(), webkit_items.end(), std::string{}, [m_ftp_url](auto acc, const auto& item)
    {
        if (!acc.empty()) acc += ",";
        return acc + std::format("\"{}\"", utils::url::get_url_from_path(m_ftp_url, item.abs_webkit_path.generic_string()));
    });

    const std::string plugins = std::accumulate(plist.begin(), plist.end(), std::string{}, [](auto acc, auto& p)
    {
        return acc + std::format("'{}',", p.plugin_name);
    });
    const std::string location = GET_GITHUB_URL_FROM_HERE();
    const std::string ftp_path = m_ftp_url + platform::get_millennium_preload_path();
    const std::string ftp_base = m_ftp_url + GetScrambledApiPathToken();

    return std::format(m_api_shim_script, location, ftp_path, plugins, modules, "", ftp_base);
}

void webkit_world_mgr::expose_millennium_to_ctx(const std::string& session_id, bool can_reload)
{
    try {
        /**
         * Expose binding for the whole target session so legacy/public-context
         * webkit shims can still reach backend FFI.
         */
        const json add_binding_params = {
            { "name", ffi_constants::binding_name }
        };
        m_client->send_host("Runtime.addBinding", add_binding_params, session_id).get();

        /** remove previously registered script for this session (if any) to avoid stacking */
        {
            std::lock_guard<std::mutex> lock(m_targets_mutex);
            for (auto& [tid, ctx] : m_attached_targets) {
                if (ctx.session_id == session_id && !ctx.script_id.empty()) {
                    try {
                        m_client
                            ->send_host("Page.removeScriptToEvaluateOnNewDocument",
                                        json{
                                            { "identifier", ctx.script_id }
                        },
                                        session_id)
                            .get();
                    } catch (...) {
                    }
                    ctx.script_id.clear();
                    break;
                }
            }
        }

        /** register script to run on every navigation (main world) */
        const json add_script_params = {
            { "source", this->compile_api_shim() }
        };
        auto script_result = m_client->send_host("Page.addScriptToEvaluateOnNewDocument", add_script_params, session_id).get();

        /** save script id so we can remove it later */
        if (script_result.contains("identifier")) {
            std::lock_guard<std::mutex> lock(m_targets_mutex);
            for (auto& [tid, ctx] : m_attached_targets) {
                if (ctx.session_id == session_id) {
                    ctx.script_id = script_result["identifier"].get<std::string>();
                    break;
                }
            }
        }

        if (can_reload) {
            /** reload page to apply CSP bypass and run script */
            m_client->send_host("Page.reload", json::object(), session_id).get();
            logger.log("webkit_world_mgr: reloaded page for session {} (top-level target)", session_id);
        } else {
            /** nested target - can't reload, just evaluate immediately (CSP bypass already applied) */
            const json evaluate_params = {
                { "expression", this->compile_api_shim() }
            };
            m_client->send_host("Runtime.evaluate", evaluate_params, session_id).get();
            logger.log("webkit_world_mgr: evaluated script in session {} (nested target, no reload)", session_id);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("webkit_world_mgr: failed to expose millennium to session {}: {}", session_id, e.what());
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
        attach_to_target(target_id, url);

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
    std::string url;

    if (target_info.contains("canAccessOpener") && target_info["canAccessOpener"].get<bool>()) {
        return;
    }

    if (target_info.contains("url") && target_info["url"].is_string()) {
        url = target_info["url"].get<std::string>();
    }

    if (!is_valid_target_url(url)) return;
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
        attach_to_target(target_id, url);

        std::lock_guard<std::mutex> lock(m_inflight_mutex);
        m_attachments_in_flight.erase(target_id);
    }
}

void webkit_world_mgr::setup_event_listeners()
{
    m_listener_tokens.push_back(m_client->on("Target.targetCreated", std::bind(&webkit_world_mgr::target_create_hdlr, this, std::placeholders::_1)));
    m_listener_tokens.push_back(m_client->on("Target.targetDestroyed", std::bind(&webkit_world_mgr::target_destroy_hdlr, this, std::placeholders::_1)));
    m_listener_tokens.push_back(m_client->on("Target.targetInfoChanged", std::bind(&webkit_world_mgr::target_change_hdlr, this, std::placeholders::_1)));
}
