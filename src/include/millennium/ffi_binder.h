/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
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
#include "millennium/core_ipc.h"
#include "millennium/cdp_api.h"
#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace ffi_constants
{
static const char* const binding_name = "__private_millennium_ffi_do_not_use__";
static const char* const sdk_ready_binding_name = "__millennium_sdk_ready__";
static const char* const frontend_binding_name = "MILLENNIUM_PRIVATE_INTERNAL_FOREIGN_FUNCTION_INTERFACE_DO_NOT_USE";
static const char* const extension_binding_name = "__millennium_extension_route__";
static const char* const extension_frontend_response_name = "MILLENNIUM_CHROME_DEV_TOOLS_PROTOCOL_DO_NOT_USE_OR_OVERRIDE_ONMESSAGE";
static const char* const cdp_proxy_binding_name = "__millennium_cdp_proxy__";
static const char* const plugin_console_binding_name = "__millennium_plugin_console_binding__";
static const char* const millennium_cdp_reject = "window.__millennium_cdp_reject__";
static const char* const millennium_cdp_resolve = "window.__millennium_cdp_resolve__";
} // namespace ffi_constants

/** methods blocked unconditionally regardless of session */
static const std::unordered_set<std::string> BLOCKED_CDP_METHODS_ALWAYS = {
    "Debugger.pause",
    "Emulation.setScriptExecutionDisabled",
    "Debugger.setBreakpointOnException",
    "Target.setAutoAttach",
    "Target.setDiscoverTargets",
    "Target.exposeDevToolsProtocol",
    "Network.clearBrowserCookies",
    "Network.clearBrowserCache",
    "Network.setCookies",
    "Network.deleteCookies",
    "Emulation.setDeviceMetricsOverride",
};

/** methods blocked only when targeting the SharedJSContext session */
static const std::unordered_set<std::string> BLOCKED_CDP_METHODS_SHARED_JS = {
    "Page.navigate",
    "Page.reload",
    "Page.close",
    "Page.crash",
    "Page.stopLoading",
    "Page.setDocumentContent",
    "Page.setBypassCSP",
    "Page.addScriptToEvaluateOnNewDocument",
    "Page.removeScriptToEvaluateOnNewDocument",
};

class ffi_binder
{
  public:
    ffi_binder(std::shared_ptr<cdp_client> client, std::shared_ptr<plugin_manager> plugin_manager, std::shared_ptr<ipc_main> ipc_main);
    ~ffi_binder();

    /** called by plugin_loader after an isolated world is created for a plugin */
    void register_isolated_ctx(const std::string& plugin_name, int isolated_ctx_id, const std::string& session_id);

  private:
    std::shared_ptr<cdp_client> m_client;
    std::shared_ptr<plugin_manager> m_plugin_manager;
    std::shared_ptr<ipc_main> m_ipc_main;

    struct plugin_ctx
    {
        int main_ctx_id = -1;
        int isolated_ctx_id = -1;
        std::string main_session_id;
        std::string isolated_session_id;
    };

    std::unordered_map<std::string, plugin_ctx> m_plugin_ctxs;                     // pluginName -> ctx ids
    std::unordered_map<int, std::string> m_ctx_to_plugin;                          // executionContextId -> pluginName
    std::unordered_map<std::string, std::unordered_set<std::string>> m_event_subs; // event -> {pluginNames}
    std::unordered_map<std::string, int> m_event_sub_tokens;                       // event -> cdp listener token
    std::vector<int> m_internal_tokens;                                            // tokens for our own internal listeners
    std::mutex m_ctx_mutex;

    void callback_into_js(const json params, const int request_id, ordered_json result);

    /**
     * event handlers for Runtime.bindingCalled events
     */
    void binding_call_hdlr(const json& params);
    void extension_route_hdlr(const json& params);
    void cdp_proxy_hdlr(const json& params);
    void sdk_ready_hdlr(const json& params);
    void plugin_console_hdlr(const json& params);
    void execution_ctx_created_hdlr(const json& params);
    void execution_ctx_destroyed_hdlr(const json& params);

    void cdp_proxy_call(const std::string& plugin_name, int callback_id, const std::string& method, const json& params, const std::string& session_id, int main_ctx_id,
                        const std::optional<std::string>& target_session);
    void cdp_proxy_relay(const std::string& plugin_name, int callback_id, const json& result, bool is_error, const std::string& session_id);
    void cdp_event_dispatch(const std::string& method, const json& params);

    bool is_valid_request(const json& params);
};
