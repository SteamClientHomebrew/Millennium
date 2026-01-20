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

#include "millennium/auth.h"
#include "head/browser_extension_mgr.h"
#include "millennium/logger.h"

browser_extension_manager::browser_extension_manager(std::shared_ptr<cdp_client> cdp) : m_cdp(std::move(cdp))
{
}

browser_extension_manager::~browser_extension_manager()
{
    this->destroy_and_detach();
}

void browser_extension_manager::create_and_attach()
{
    if (!create_target()) {
        LOG_ERROR("Extension manager failed to create target!");
    }

    attach_to_target();
}

void browser_extension_manager::destroy_and_detach()
{
    if (!m_cdp || m_target_id.empty()) {
        return;
    }

    const json params = {
        { "targetId", m_target_id }
    };

    try {
        m_cdp->send_host("Target.closeTarget", params).get();
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to close extension target: {}", e.what());
    }
}

bool browser_extension_manager::create_target()
{
    const auto window_id = create_hidden_window();
    if (window_id.empty()) {
        LOG_ERROR("Failed to create hidden window");
        return false;
    }

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
    json eval_res;
    const std::string window_id = build_random_string(32);
    const std::string script = R"({{
        const parentPopupBrowserID = g_PopupManager.GetExistingPopup('SP Desktop_uid0')?.window.SteamClient.Browser.GetBrowserID()
    	const popup = window.open(SteamClient.BrowserView.CreatePopup({{ parentPopupBrowserID }}).strCreateURL);
    	if (!popup) throw new Error('Failed to open popup window');
    	popup.document.title = '{}';
    }})";

    json eval_params = {
        { "expression", fmt::format(script, window_id) }
    };

    try {
        eval_res = m_cdp->send("Runtime.evaluate", eval_params).get();
        logger.log("Eval result: {}", eval_res.dump(4));
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create hidden extension window: {}", e.what());
        return {};
    }

    if (eval_res.contains("error")) {
        LOG_ERROR("Failed to create hidden extension window: {}", eval_res["error"]["message"].get<std::string>());
        return {};
    }

    return window_id;
}

bool browser_extension_manager::attach_to_target()
{
    if (m_target_id.empty()) {
        return false;
    }

    const json attach_params = {
        { "targetId", m_target_id },
        { "flatten",  true        }
    };

    const json page_params = {
        { "url", "chrome://extensions" }
    };

    try {
        auto attach_res = m_cdp->send_host("Target.attachToTarget", attach_params).get();

        if (attach_res.contains("sessionId")) {
            m_session_id = attach_res["sessionId"];
            m_cdp->send_host("Page.navigate", page_params, m_session_id);
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to attach to extension target: {}", e.what());
    }
    return false;
}

json browser_extension_manager::evaluate_expression(std::string script)
{
    create_and_attach();
    json result;

    try {
        const nlohmann::json eval_params = {
            { "expression",    script },
            { "returnByValue", true   },
            { "awaitPromise",  true   }
        };
        auto eval_res = m_cdp->send_host("Runtime.evaluate", eval_params, m_session_id).get();

        if (eval_res.contains("result") && eval_res["result"].contains("value") && eval_res["result"]["value"].is_array()) {
            result = eval_res["result"]["value"];
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get extensions: {}", e.what());
        m_session_id.clear();
    }

    destroy_and_detach();
    return result;
}

json browser_extension_manager::get_extensions()
{
    return this->evaluate_expression("chrome.management.getAll()");
}
