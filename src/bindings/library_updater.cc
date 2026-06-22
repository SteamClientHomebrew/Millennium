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

#include "head/library_updater.h"
#include "head/entry_point.h"
#include "head/plugin_mgr.h"
#include "head/theme_mgr.h"

#include "millennium/core_ipc.h"
#include "millennium/http.h"
#include "millennium/logger.h"
#include <cmath>
#include <memory>

head::library_updater::library_updater(std::weak_ptr<millennium_backend> millennium_backend, std::shared_ptr<ipc_main> ipc_main)
    : m_millennium_backend(std::move(millennium_backend)), m_ipc_main(std::move(ipc_main)), m_has_checked_for_updates(false)
{
    logger.log("[library_updater] constructed, ipc_main={}", m_ipc_main ? "set" : "null");
}

void head::library_updater::init(std::shared_ptr<plugin_manager> plugin_manager)
{
    logger.log("[library_updater::init] initializing theme_installer and plugin_installer");
    theme_updater = std::make_shared<theme_installer>(plugin_manager, shared_from_this());
    plugin_updater = std::make_shared<plugin_installer>(m_millennium_backend, plugin_manager, shared_from_this());
    logger.log("[library_updater::init] installers created");

    bool updates_enabled = CONFIG.get({ "general", "checkForPluginAndThemeUpdates" }).get<bool>();
    logger.log("[library_updater::init] checkForPluginAndThemeUpdates={}", updates_enabled);

    if (!updates_enabled) {
        logger.warn("[library_updater::init] user has disabled update checking for plugins and themes — skipping background fetch");
        std::lock_guard<std::mutex> lock(m_updates_mutex);
        cached_updates = json{
            { "themes",  json::array() },
            { "plugins", json::array() }
        };
        m_has_checked_for_updates = true;
        return;
    }

    logger.log("[library_updater::init] launching background update check");
    m_update_future = std::async(std::launch::async, [self = shared_from_this()]()
    {
        logger.log("[library_updater::init] background update thread started");
        self->fetch_updates_from_network();
        logger.log("[library_updater::init] background update thread finished");
    }).share();
    logger.log("[library_updater::init] background future submitted");
}

bool head::library_updater::download_plugin_update(const std::string& id, const std::string& name, const std::string& commit)
{
    logger.log("[library_updater::download_plugin_update] starting update: id='{}' name='{}' commit='{}'", id, name, commit);
    bool ok = plugin_updater->update_plugin(id, name, commit);
    logger.log("[library_updater::download_plugin_update] update_plugin returned {} for id='{}'", ok ? "success" : "failure", id);
    if (ok) {
        logger.log("[library_updater::download_plugin_update] invalidating update cache after successful plugin update");
        std::lock_guard<std::mutex> lock(m_updates_mutex);
        cached_updates.reset();
    }
    return ok;
}

bool head::library_updater::download_theme_update(std::shared_ptr<theme_config_store> themeConfig, const std::string& native)
{
    logger.log("[library_updater::download_theme_update] starting update: native='{}'", native);
    bool ok = theme_updater->update_theme(themeConfig, native);
    logger.log("[library_updater::download_theme_update] update_theme returned {} for native='{}'", ok ? "success" : "failure", native);
    if (ok) {
        logger.log("[library_updater::download_theme_update] invalidating update cache after successful theme update");
        std::lock_guard<std::mutex> lock(m_updates_mutex);
        cached_updates.reset();
    }
    return ok;
}

std::optional<json> head::library_updater::get_cached_updates() const
{
    std::lock_guard<std::mutex> lock(m_updates_mutex);
    return cached_updates;
}

bool head::library_updater::has_checked_for_updates() const
{
    std::lock_guard<std::mutex> lock(m_updates_mutex);
    return m_has_checked_for_updates;
}

std::optional<json> head::library_updater::fetch_updates_from_network()
{
    logger.log("[fetch_updates_from_network] entry");
    try {
        logger.log("[fetch_updates_from_network] building plugin request body...");
        auto plugins = plugin_updater->get_updater_request_body();
        logger.log("[fetch_updates_from_network] plugin request body: {} entry(ies)", plugins.size());

        logger.log("[fetch_updates_from_network] building theme request body...");
        auto themes = theme_updater->get_request_body();
        bool has_theme_post_body = themes.contains("post_body") && themes["post_body"].is_array() && !themes["post_body"].empty();
        logger.log("[fetch_updates_from_network] theme request body: has_post_body={}, post_body_size={}", has_theme_post_body,
                   (has_theme_post_body ? themes["post_body"].size() : 0));

        json request_body;

        if (!plugins.empty()) {
            logger.log("[fetch_updates_from_network] adding {} plugin(s) to request body", plugins.size());
            request_body["plugins"] = plugins;
            for (const auto& p : plugins) {
                logger.log("[fetch_updates_from_network]   plugin: id='{}' version='{}'", p.value("id", "?"), p.value("version", "?"));
            }
        } else {
            logger.log("[fetch_updates_from_network] no plugins to check");
        }

        if (has_theme_post_body) {
            logger.log("[fetch_updates_from_network] adding {} theme(s) to request body", themes["post_body"].size());
            request_body["themes"] = themes["post_body"];
            for (const auto& t : themes["post_body"]) {
                logger.log("[fetch_updates_from_network]   theme: native='{}'", t.value("native", "?"));
            }
        } else {
            logger.log("[fetch_updates_from_network] no themes to check");
        }

        if (request_body.empty()) {
            logger.log("[fetch_updates_from_network] no themes or plugins installed.");
            json result = {
                { "themes",  {} },
                { "plugins", {} }
            };
            std::lock_guard<std::mutex> lock(m_updates_mutex);
            cached_updates = result;
            m_has_checked_for_updates = true;
            return result;
        }

        std::string serialized = request_body.dump();
        logger.log("[fetch_updates_from_network] POSTing to '{}' ({} bytes)", api_url, serialized.size());
        logger.log("[fetch_updates_from_network] request body: {}", serialized);

        auto response_str = Http::Post(api_url.c_str(), serialized);
        logger.log("[fetch_updates_from_network] HTTP response received ({} bytes)", response_str.size());
        logger.log("[fetch_updates_from_network] raw response: {}", response_str.size() <= 128 ? response_str : response_str.substr(0, 128) + "...(truncated)");

        json resp;
        try {
            resp = json::parse(response_str);
            logger.log("[fetch_updates_from_network] response parsed successfully");
        } catch (const std::exception& parse_err) {
            logger.warn("[fetch_updates_from_network] failed to parse API response as JSON: {}", parse_err.what());
            throw;
        }

        if (resp.contains("plugins")) {
            logger.log("[fetch_updates_from_network] response contains {} plugin update(s)", resp["plugins"].is_array() ? resp["plugins"].size() : 0);
            if (resp["plugins"].is_array()) {
                for (const auto& p : resp["plugins"]) {
                    logger.log("[fetch_updates_from_network]   plugin update: id='{}' name='{}' commit='{}'", p.value("id", "?"), p.value("name", "?"), p.value("commit", "?"));
                }
            }
        } else {
            logger.log("[fetch_updates_from_network] response has no 'plugins' key");
        }

        if (resp.contains("themes")) {
            bool has_error = resp["themes"].is_object() && resp["themes"].contains("error");
            bool is_empty = resp["themes"].empty();
            std::string themes_raw = resp["themes"].dump();
            logger.log("[fetch_updates_from_network] response themes: empty={}, has_error={}, raw={}", is_empty, has_error,
                       themes_raw.size() <= 512 ? themes_raw : themes_raw.substr(0, 512) + "...");

            if (!is_empty && !has_error) {
                logger.log("[fetch_updates_from_network] API returned {} theme(s), processing against local state...", resp["themes"].size());
                for (const auto& t : resp["themes"]) {
                    logger.log("[fetch_updates_from_network]   API theme: name='{}' commit='{}'", t.value("name", "?"), t.value("commit", "?"));
                }
                resp["themes"] = theme_updater->process_update(themes["update_query"], resp["themes"]);
                logger.log("[fetch_updates_from_network] process_update done: {} theme(s) need updating", resp["themes"].size());
                for (const auto& t : resp["themes"]) {
                    logger.log("[fetch_updates_from_network]   needs update: name='{}' native='{}'", t.value("name", "?"), t.value("native", "?"));
                }
            } else if (has_error) {
                logger.warn("[fetch_updates_from_network] theme response contains error: {}", resp["themes"].value("error", "?"));
            }
        } else {
            logger.log("[fetch_updates_from_network] response has no 'themes' key");
        }

        logger.log("[fetch_updates_from_network] caching result and marking update check complete");
        {
            std::lock_guard<std::mutex> lock(m_updates_mutex);
            cached_updates = resp;
            m_has_checked_for_updates = true;
        }
        return resp;
    } catch (const std::exception& e) {
        logger.warn("[fetch_updates_from_network] exception caught: {}", e.what());
        json error_result = {
            { "themes", { { "error", e.what() } } }
        };
        std::lock_guard<std::mutex> lock(m_updates_mutex);
        cached_updates = error_result;
        m_has_checked_for_updates = true;
        return error_result;
    }
}

std::optional<json> head::library_updater::check_for_updates(bool force)
{
    logger.log("[check_for_updates] entry: force={}", force);

    {
        std::lock_guard<std::mutex> lock(m_updates_mutex);
        if (!force && cached_updates.has_value()) {
            logger.log("[check_for_updates] cache hit — returning without network call");
            return cached_updates;
        }
        logger.log("[check_for_updates] cache miss (has_value={}, force={})", cached_updates.has_value(), force);
    }

    if (!force && m_update_future.valid()) {
        logger.log("[check_for_updates] waiting for in-flight background update future...");
        m_update_future.wait();
        logger.log("[check_for_updates] background future finished, checking cache");
        std::lock_guard<std::mutex> lock(m_updates_mutex);
        if (cached_updates.has_value()) {
            logger.log("[check_for_updates] background future populated the cache — returning that");
            return cached_updates;
        }
        logger.log("[check_for_updates] background future finished but cache still empty, fetching directly");
    }

    return fetch_updates_from_network();
}

std::string head::library_updater::re_check_for_updates()
{
    logger.log("[library_updater::re_check_for_updates] forced resync requested");
    auto result = check_for_updates(true);
    logger.log("[library_updater::re_check_for_updates] resync complete, has_result={}", result.has_value());
    return result.has_value() ? result->dump() : "{}";
}

std::weak_ptr<head::theme_installer> head::library_updater::get_theme_updater()
{
    return theme_updater;
}

std::weak_ptr<head::plugin_installer> head::library_updater::get_plugin_updater()
{
    return plugin_updater;
}

void head::library_updater::set_ipc_main(std::shared_ptr<ipc_main> ipc_main)
{
    logger.log("[library_updater::set_ipc_main] ipc_main {}", ipc_main ? "assigned" : "cleared (null)");
    m_ipc_main = std::move(ipc_main);
}

// Thread-local state for per-operation progress dispatching.
// Each background thread gets its own op_id and throttle state.
static thread_local int t_current_op_id = 0;
static thread_local std::chrono::steady_clock::time_point t_last_dispatch_time{};
static thread_local double t_last_dispatched_progress = -1.0;

void head::library_updater::dispatch_progress(const std::string& status, double progress, bool is_complete, bool success)
{
    if (!m_ipc_main) {
        logger.warn("[dispatch_progress] skipping: IPC bridge not ready (status='{}', progress={:.1f}, is_complete={}, success={})", status, progress, is_complete, success);
        return;
    }

    // Completion events always go through immediately.
    if (!is_complete) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t_last_dispatch_time).count();
        double delta = std::abs(progress - t_last_dispatched_progress);

        // Throttle: skip if progress hasn't moved ≥2% and it's been less than 250ms.
        // This prevents flooding the CDP channel during rapid download/extraction callbacks.
        if (delta < 2.0 && elapsed_ms < 250) {
            logger.log("[dispatch_progress] throttled: delta={:.2f}% elapsed={}ms status='{}' progress={:.1f}", delta, elapsed_ms, status, progress);
            return;
        }

        logger.log("[dispatch_progress] sending: op_id={} status='{}' progress={:.1f}% delta={:.2f}% elapsed={}ms", t_current_op_id, status, progress, delta, elapsed_ms);
        t_last_dispatched_progress = progress;
        t_last_dispatch_time = now;
    } else {
        logger.log("[dispatch_progress] completion event: op_id={} status='{}' success={}", t_current_op_id, status, success);
    }

    int op_id = t_current_op_id;
    std::vector<ipc_main::javascript_parameter> params = { status, progress, is_complete, success, static_cast<int64_t>(op_id) };
    m_ipc_main->evaluate_javascript_expression(m_ipc_main->compile_javascript_expression("core", "InstallerMessageEmitter", params));
}

int head::library_updater::start_operation()
{
    int id = m_next_op_id.fetch_add(1);
    logger.log("[library_updater::start_operation] allocated op_id={}", id);
    return id;
}

void head::library_updater::set_thread_op_id(int op_id)
{
    logger.log("[library_updater::set_thread_op_id] thread bound to op_id={}, resetting throttle state", op_id);
    t_current_op_id = op_id;
    // Reset per-thread throttle state for the new operation.
    t_last_dispatch_time = {};
    t_last_dispatched_progress = -1.0;
}
