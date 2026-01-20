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

#include "head/browser_extension_mgr.h"
#include "millennium/logger.h"
#include <fstream>
#include <regex>

browser_extension_manager::~browser_extension_manager()
{
    if (m_cdp && !m_target_id.empty()) {
        try {
            m_cdp->send_host("Target.closeTarget", {{"targetId", m_target_id}}).get();
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to close extension target: {}", e.what());
        }
    }
}

void browser_extension_manager::initialize(const std::shared_ptr<cdp_client>& cdp)
{
    m_cdp = cdp;
    if (!m_cdp) return;

    if (create_target()) {
        attach_to_target();
    }
}

bool browser_extension_manager::create_target()
{
    const auto window_id = create_hidden_window();
    if (window_id.empty()) return false;

    auto targets = m_cdp->send_host("Target.getTargets").get();
    if (!targets.contains("targetInfos") || !targets["targetInfos"].is_array() || targets["targetInfos"].empty()) {
        return false;
    }

    const auto created_window = std::find_if(targets["targetInfos"].begin(), targets["targetInfos"].end(), [&](const auto& target) { return target["title"] == window_id; });
    if (created_window == targets["targetInfos"].end()) {
        return false;
    }

    m_target_id = created_window->at("targetId");
    return true;
}

std::string browser_extension_manager::create_hidden_window() const
{
     std::string window_id = "millennium_ext_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    json eval_params = {
        { "expression", "window.createHiddenWindow(\"" + window_id + "\")" }
    };

    json eval_res;
    try {
        eval_res = m_cdp->send("Runtime.evaluate", eval_params).get();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create hidden extension window: {}", e.what());
        return "";
    }

    // Check for error
    if (eval_res.contains("error")) {
        LOG_ERROR("Failed to create hidden extension window: {}", eval_res["error"]["message"].get<std::string>());
        return "";
    }

    return window_id;
}

bool browser_extension_manager::attach_to_target()
{
    if (m_target_id.empty()) return false;

    try {
        const nlohmann::json attach_params = {
            { "targetId", m_target_id },
            { "flatten", true }
        };
        auto attach_res = m_cdp->send_host("Target.attachToTarget", attach_params).get();

        if (attach_res.contains("sessionId")) {
            m_session_id = attach_res["sessionId"];

            m_cdp->send_host("Page.navigate", {
                { "url", "chrome://extensions" }
            }, m_session_id);

            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to attach to extension target: {}", e.what());
    }
    return false;
}

nlohmann::json browser_extension_manager::get_all_extensions()
{
    if (!m_cdp || m_session_id.empty()) {
        // Try to re-initialize if session is missing
        if (m_cdp) {
            if (m_target_id.empty()) {
                if (create_target()) attach_to_target();
            } else {
                attach_to_target();
            }
        }

        if (m_session_id.empty()) {
            return nlohmann::json::array();
        }
    }

    try {
        const nlohmann::json eval_params = {
            { "expression", "(async function() { return (await chrome.management.getAll()) })()" },
            { "returnByValue", true },
            { "awaitPromise", true }
        };
        auto eval_res = m_cdp->send_host("Runtime.evaluate", eval_params, m_session_id).get();

        if (eval_res.contains("result") && eval_res["result"].contains("value") && eval_res["result"]["value"].is_array()) {
            return eval_res["result"]["value"];
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get extensions: {}", e.what());
        // If it failed, maybe the session is stale, clear it
        m_session_id.clear();
    }

    return nlohmann::json::array();
}

// TODO: figure out a way to call this when our target get's destroyed from for example a dev reload.
void browser_extension_manager::target_destroyed(const nlohmann::json& params)
{
    if (!params.contains("targetId")) {
        LOG_ERROR("webkit_world_mgr: Target.targetDestroyed missing targetId");
        return;
    }

    std::string target_id = params["targetId"];
    if (target_id == m_target_id) {
        m_target_id.clear();
        m_session_id.clear();

        if (create_target()) {
            attach_to_target();
        }
    }
}