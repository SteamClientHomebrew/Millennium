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
}

void head::library_updater::init(std::shared_ptr<plugin_manager> plugin_manager)
{
    theme_updater = std::make_shared<theme_installer>(plugin_manager, shared_from_this());
    plugin_updater = std::make_shared<plugin_installer>(m_millennium_backend, plugin_manager, shared_from_this());

    bool updates_enabled = CONFIG.get({ "general", "checkForPluginAndThemeUpdates" }).get<bool>();

    if (!updates_enabled) {
        logger.warn("User has disabled update checking for plugins and themes.");
        std::lock_guard<std::mutex> lock(m_updates_mutex);
        cached_updates = json{
            { "themes",  json::array() },
            { "plugins", json::array() }
        };
        m_has_checked_for_updates = true;
        return;
    }

    m_update_future = std::async(std::launch::async, [self = shared_from_this()]()
    {
        self->fetch_updates_from_network();
    }).share();
}

bool head::library_updater::download_plugin_update(const std::string& id, const std::string& name, const std::string& commit)
{
    bool ok = plugin_updater->update_plugin(id, name, commit);
    if (ok) {
        std::lock_guard<std::mutex> lock(m_updates_mutex);
        cached_updates.reset();
    }
    return ok;
}

bool head::library_updater::download_theme_update(std::shared_ptr<theme_config_store> themeConfig, const std::string& native)
{
    bool ok = theme_updater->update_theme(themeConfig, native);
    if (ok) {
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
    try {
        auto plugins = plugin_updater->get_updater_request_body();
        auto themes = theme_updater->get_request_body();
        bool has_theme_post_body = themes.contains("post_body") && themes["post_body"].is_array() && !themes["post_body"].empty();

        json request_body;

        if (!plugins.empty()) request_body["plugins"] = plugins;
        if (has_theme_post_body) request_body["themes"] = themes["post_body"];

        if (request_body.empty()) {
            logger.log("No themes or plugins to update!");
            json result = {
                { "themes",  {} },
                { "plugins", {} }
            };
            std::lock_guard<std::mutex> lock(m_updates_mutex);
            cached_updates = result;
            m_has_checked_for_updates = true;
            return result;
        }

        auto response_str = Http::Post(api_url.c_str(), request_body.dump());
        json resp;
        try {
            resp = json::parse(response_str);
        } catch (const std::exception& parse_err) {
            logger.warn("Failed to parse update API response: {}", parse_err.what());
            throw;
        }

        if (resp.contains("themes") && !resp["themes"].empty() && !resp["themes"].contains("error")) {
            logger.log("[update-check] API returned {} theme(s), processing...", resp["themes"].size());
            resp["themes"] = theme_updater->process_update(themes["update_query"], resp["themes"]);
            logger.log("[update-check] After filtering: {} theme(s) have updates", resp["themes"].size());
        } else if (resp.contains("themes") && resp["themes"].contains("error")) {
            logger.warn("[update-check] Theme update check returned an error: {}", resp["themes"].value("error", "?"));
        }

        {
            std::lock_guard<std::mutex> lock(m_updates_mutex);
            cached_updates = resp;
            m_has_checked_for_updates = true;
        }
        return resp;
    } catch (const std::exception& e) {
        logger.warn("An error occurred while checking for updates: {}", e.what());
        /* Do not cache — allow the next call to retry after a transient failure. */
        return json{
            { "themes",  { { "error", e.what() } } },
            { "plugins", json::array()              }
        };
    }
}

std::optional<json> head::library_updater::check_for_updates(bool force)
{
    {
        std::lock_guard<std::mutex> lock(m_updates_mutex);
        if (!force && cached_updates.has_value()) {
            logger.log("Using cached updates.");
            return cached_updates;
        }
    }

    if (!force && m_update_future.valid()) {
        m_update_future.wait();
        std::lock_guard<std::mutex> lock(m_updates_mutex);
        if (cached_updates.has_value()) {
            logger.log("Using cached updates (waited for background check).");
            return cached_updates;
        }
    }

    return fetch_updates_from_network();
}

std::string head::library_updater::re_check_for_updates()
{
    logger.log("Resyncing updates...");
    auto result = check_for_updates(true);
    logger.log("Resync complete.");
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
        logger.warn("Skipping installer progress dispatch because IPC bridge is not ready. status='{}', progress={}", status, progress);
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
            return;
        }

        t_last_dispatched_progress = progress;
        t_last_dispatch_time = now;
    }

    int op_id = t_current_op_id;
    std::vector<ipc_main::javascript_parameter> params = { status, progress, is_complete, success, static_cast<int64_t>(op_id) };
    m_ipc_main->evaluate_javascript_expression(m_ipc_main->compile_javascript_expression("core", "InstallerMessageEmitter", params));
}

int head::library_updater::start_operation()
{
    return m_next_op_id.fetch_add(1);
}

void head::library_updater::set_thread_op_id(int op_id)
{
    t_current_op_id = op_id;
    // Reset per-thread throttle state for the new operation.
    t_last_dispatch_time = {};
    t_last_dispatched_progress = -1.0;
}
