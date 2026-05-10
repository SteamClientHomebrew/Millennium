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

#include "millennium/ffi_binder.h"
#include "millennium/logger.h"
#include "millennium/core_ipc.h"
#include "mep/ffi_recorder.h"
#include "mep/sdk_ready_bus.h"

#include <chrono>

ffi_binder::ffi_binder(std::shared_ptr<cdp_client> client, std::shared_ptr<plugin_manager> plugin_manager, std::shared_ptr<ipc_main> ipc_main)
    : m_client(client), m_plugin_manager(std::move(plugin_manager)), m_ipc_main(std::move(ipc_main))
{
    m_internal_tokens.push_back(m_client->on("Runtime.bindingCalled", std::bind(&ffi_binder::binding_call_hdlr, this, std::placeholders::_1)));
    m_internal_tokens.push_back(m_client->on("Runtime.executionContextCreated", std::bind(&ffi_binder::execution_ctx_created_hdlr, this, std::placeholders::_1)));
    m_internal_tokens.push_back(m_client->on("Runtime.executionContextDestroyed", std::bind(&ffi_binder::execution_ctx_destroyed_hdlr, this, std::placeholders::_1)));
}

ffi_binder::~ffi_binder()
{
    std::unique_lock<std::mutex> lock(m_ctx_mutex);
    for (const auto& [event, token] : m_event_sub_tokens) {
        m_client->off(token);
    }
    m_event_sub_tokens.clear();
    lock.unlock();

    for (int token : m_internal_tokens) {
        m_client->off(token);
    }
    logger.log("Successfully shut down ffi_binder...");
}

void ffi_binder::register_isolated_ctx(const std::string& plugin_name, int isolated_ctx_id, const std::string& session_id)
{
    std::lock_guard<std::mutex> lock(m_ctx_mutex);
    auto& ctx = m_plugin_ctxs[plugin_name];
    ctx.isolated_ctx_id = isolated_ctx_id;
    ctx.isolated_session_id = session_id;
    m_ctx_to_plugin[isolated_ctx_id] = plugin_name;
    logger.log("ffi_binder: registered isolated ctx {} for plugin '{}'", isolated_ctx_id, plugin_name);
}

void ffi_binder::callback_into_js(const json params, const int request_id, ordered_json result)
{
    json eval_params = {
        { "contextId", params["executionContextId"] },
        { "expression", std::format("window.{}.__handleResponse({}, {})", ffi_constants::frontend_binding_name, request_id, result.dump()) }
    };

    auto eval_result = m_client->send_host("Runtime.evaluate", eval_params, params["sessionId"].get<std::string>()).get();
    if (eval_result.contains("exceptionDetails")) {
        LOG_ERROR("ffi_binder: failed to evaluate callback: {}", eval_result["exceptionDetails"].dump());
        return;
    }
}

bool ffi_binder::is_valid_request(const json& params)
{
    /** ensure the binding call is actually from the expected binding */
    if (!params.contains("name") || params["name"].get<std::string>() != ffi_constants::binding_name) {
        return false;
    }

    /** ensure the context id is present, otherwise we can't actually respond to the request */
    if (!params.contains("payload") || !params.contains("executionContextId")) {
        return false;
    }

    /** we also need the session id from steams browser window */
    if (!params.contains("sessionId") || !params["sessionId"].is_string()) {
        return false;
    }

    return true;
}

void ffi_binder::extension_route_hdlr(const json& params)
{
    if (!params.contains("payload") || !params.contains("executionContextId") || !params.contains("sessionId")) {
        LOG_ERROR("ffi_binder: corrupted extension route call: {}", params.dump(4));
        return;
    }

    const int context_id = params["executionContextId"].get<int>();
    const std::string session_id = params["sessionId"].get<std::string>();
    int call_id = -1;

    try {
        json payload = json::parse(params["payload"].get<std::string>());

        call_id = payload.at("id").get<int>();
        const std::string method = payload.at("method").get<std::string>();
        json cdp_params = payload.value("params", json::object());
        auto opt_session = payload.contains("sessionId") ? std::optional<std::string>(payload["sessionId"].get<std::string>()) : std::nullopt;

        json response;
        try {
            auto result = m_client->send_host(method, cdp_params, opt_session).get();
            response = {
                { "id",     call_id },
                { "result", result  }
            };
        } catch (const std::exception& e) {
            response = {
                { "id",    call_id                     },
                { "error", { { "message", e.what() } } }
            };
        }

        json eval_params = {
            { "contextId", context_id },
            { "expression", std::format("window.{}.__handleCDPResponse({})", ffi_constants::extension_frontend_response_name, response.dump()) }
        };
        m_client->send_host("Runtime.evaluate", eval_params, session_id);

    } catch (const std::exception& e) {
        LOG_ERROR("ffi_binder: extension route error: {}", e.what());

        json error_response = {
            { "id",    call_id                     },
            { "error", { { "message", e.what() } } }
        };
        try {
            json eval_params = {
                { "contextId", context_id },
                { "expression", std::format("window.{}.__handleCDPResponse({})", ffi_constants::extension_frontend_response_name, error_response.dump()) }
            };
            m_client->send_host("Runtime.evaluate", eval_params, session_id);
        } catch (...) {
        }
    }
}

void ffi_binder::cdp_proxy_hdlr(const json& params)
{
    if (!params.contains("payload") || !params.contains("executionContextId") || !params.contains("sessionId")) {
        return;
    }

    json payload;
    try {
        payload = json::parse(params["payload"].get<std::string>());
    } catch (...) {
        LOG_ERROR("ffi_binder: cdp_proxy_hdlr failed to parse payload");
        return;
    }

    const std::string action = payload.value("action", "");
    const std::string session_id = params["sessionId"].get<std::string>();
    const int ctx_id = params["executionContextId"].get<int>();

    if (action == "cdp_call") {
        const std::string plugin_name = payload.value("pluginName", "");
        const int callback_id = payload.value("callbackId", -1);
        const std::string method = payload.value("method", "");
        const json cdp_params = payload.value("params", json::object());

        /** target session for the CDP command (optional — nullopt targets browser host) */
        std::optional<std::string> target_session;
        if (payload.contains("sessionId") && payload["sessionId"].is_string()) {
            target_session = payload["sessionId"].get<std::string>();
        }

        if (plugin_name.empty() || callback_id == -1 || method.empty()) {
            LOG_ERROR("ffi_binder: cdp_proxy: malformed cdp_call payload");
            return;
        }

        /** always keep main world context id current, it changes on SharedJSContext reload */
        {
            std::lock_guard<std::mutex> lock(m_ctx_mutex);
            auto& ctx = m_plugin_ctxs[plugin_name];
            ctx.main_ctx_id = ctx_id;
            ctx.main_session_id = session_id;
        }

        cdp_proxy_call(plugin_name, callback_id, method, cdp_params, session_id, ctx_id, target_session);
    } else if (action == "relay" || action == "relay_error") {
        const int callback_id = payload.value("callbackId", -1);
        if (callback_id == -1) return;

        std::string main_session;
        int main_ctx = -1;
        std::string plugin_name_dbg;
        {
            std::lock_guard<std::mutex> lock(m_ctx_mutex);
            auto name_it = m_ctx_to_plugin.find(ctx_id);
            if (name_it == m_ctx_to_plugin.end()) {
                logger.log("[CDP PROXY] relay: unknown isolated ctx_id={}, dropping", ctx_id);
                return;
            }
            plugin_name_dbg = name_it->second;
            auto ctx_it = m_plugin_ctxs.find(plugin_name_dbg);
            if (ctx_it == m_plugin_ctxs.end()) return;
            main_session = ctx_it->second.main_session_id;
            main_ctx = ctx_it->second.main_ctx_id;
        }

        if (main_ctx == -1) {
            logger.log("[CDP PROXY] relay: main_ctx_id not set yet for '{}', dropping", plugin_name_dbg);
            return;
        }

        if (action == "relay_error") {
            const std::string error = payload.value("error", "unknown error");
            json eval_params = {
                { "contextId", main_ctx },
                { "expression", std::format("{}({}, {})", ffi_constants::millennium_cdp_reject, callback_id, json(error).dump()) }
            };
            m_client->send_host("Runtime.evaluate", eval_params, main_session);
        } else {
            const json result = payload.value("result", json::object());
            json eval_params = {
                { "contextId", main_ctx },
                { "expression", std::format("{}({}, {})", ffi_constants::millennium_cdp_resolve, callback_id, result.dump()) }
            };
            m_client->send_host("Runtime.evaluate", eval_params, main_session);
        }
    } else if (action == "subscribe" || action == "unsubscribe") {
        const std::string plugin_name = payload.value("pluginName", "");
        const std::string event = payload.value("event", "");

        if (plugin_name.empty() || event.empty()) return;

        std::lock_guard<std::mutex> lock(m_ctx_mutex);

        if (action == "subscribe") {
            bool first = m_event_subs[event].empty();
            m_event_subs[event].insert(plugin_name);

            auto& plugin_ctx_ref = m_plugin_ctxs[plugin_name];
            plugin_ctx_ref.main_ctx_id = ctx_id;
            plugin_ctx_ref.main_session_id = session_id;

            if (first) {
                int token = m_client->on(event, [this, event](const json& event_params)
                {
                    cdp_event_dispatch(event, event_params);
                });
                m_event_sub_tokens[event] = token;
            }
        } else {
            auto it = m_event_subs.find(event);
            if (it != m_event_subs.end()) {
                it->second.erase(plugin_name);
                if (it->second.empty()) {
                    m_event_subs.erase(it);
                    auto tok_it = m_event_sub_tokens.find(event);
                    if (tok_it != m_event_sub_tokens.end()) {
                        m_client->off(tok_it->second);
                        m_event_sub_tokens.erase(tok_it);
                    }
                }
            }
        }
    }
}

void ffi_binder::cdp_proxy_call(const std::string& plugin_name, int callback_id, const std::string& method, const json& params, const std::string& session_id, int main_ctx_id,
                                const std::optional<std::string>& target_session)
{
    const bool targets_shared_js = target_session.has_value() && (target_session.value() == m_client->get_shared_js_session_id());

    if (BLOCKED_CDP_METHODS_ALWAYS.count(method) || (targets_shared_js && BLOCKED_CDP_METHODS_SHARED_JS.count(method))) {
        const std::string msg = std::format("Millennium prohibits calls to '{}' for user safety", method);
        json eval_params = {
            { "contextId", main_ctx_id },
            { "expression", std::format("{}({}, {})", ffi_constants::millennium_cdp_reject, callback_id, json(msg).dump()) }
        };
        m_client->send_host("Runtime.evaluate", eval_params, session_id);
        return;
    }

    auto deliver = [&](const json& value, bool is_error)
    {
        int main_ctx = main_ctx_id;
        std::string main_sess = session_id;
        {
            std::lock_guard<std::mutex> lock(m_ctx_mutex);
            auto it = m_plugin_ctxs.find(plugin_name);
            if (it != m_plugin_ctxs.end() && it->second.main_ctx_id != -1) {
                main_ctx = it->second.main_ctx_id;
                main_sess = it->second.main_session_id;
            }
        }
        const char* js_fn = is_error ? ffi_constants::millennium_cdp_reject : ffi_constants::millennium_cdp_resolve;
        json eval_params = {
            { "contextId", main_ctx },
            { "expression", std::format("{}({}, {})", js_fn, callback_id, value.dump()) }
        };
        m_client->send_host("Runtime.evaluate", eval_params, main_sess);
    };

    try {
        auto result = m_client->send_host(method, params, target_session).get();
        deliver(result, false);
    } catch (const std::exception& e) {
        deliver(json(std::string(e.what())), true);
    }
}

void ffi_binder::cdp_event_dispatch(const std::string& method, const json& params)
{
    std::vector<std::pair<int, std::string>> targets; // { main_ctx_id, session_id }
    {
        std::lock_guard<std::mutex> lock(m_ctx_mutex);
        auto it = m_event_subs.find(method);
        if (it == m_event_subs.end()) return;

        for (const auto& plugin_name : it->second) {
            auto ctx_it = m_plugin_ctxs.find(plugin_name);
            if (ctx_it != m_plugin_ctxs.end() && ctx_it->second.main_ctx_id != -1) {
                targets.emplace_back(ctx_it->second.main_ctx_id, ctx_it->second.main_session_id);
            }
        }
    }

    const json event_data = {
        { "method", method },
        { "params", params }
    };

    for (const auto& [ctx_id, session_id] : targets) {
        json eval_params = {
            { "contextId", ctx_id },
            { "expression", std::format("window.__millennium_cdp_event__({})", event_data.dump()) }
        };
        m_client->send_host("Runtime.evaluate", eval_params, session_id);
    }
}

void ffi_binder::sdk_ready_hdlr(const json& params)
{
    mep::sdk_ready_event ev;

    if (params.contains("payload") && params["payload"].is_string()) {
        try {
            const auto payload = json::parse(params["payload"].get<std::string>());
            ev.sdk_version = payload.value("sdk_version", std::string{});
            ev.millennium_version = payload.value("millennium_version", std::string{});
            ev.api_total = payload.value("api_total", 0);
            if (payload.contains("api_missing") && payload["api_missing"].is_array()) {
                for (const auto& k : payload["api_missing"]) {
                    if (k.is_string()) ev.api_missing.push_back(k.get<std::string>());
                }
            }
        } catch (...) {
        }
    }

    mep::sdk_ready_bus::instance().notify(ev);
}

void ffi_binder::binding_call_hdlr(const json& params)
{
    if (!params.contains("name")) {
        return;
    }

    const std::string& binding_name = params["name"].get_ref<const std::string&>();

    if (binding_name == ffi_constants::extension_binding_name) {
        extension_route_hdlr(params);
        return;
    }

    if (binding_name == ffi_constants::cdp_proxy_binding_name) {
        cdp_proxy_hdlr(params);
        return;
    }

    if (binding_name == ffi_constants::sdk_ready_binding_name) {
        sdk_ready_hdlr(params);
        return;
    }

    if (!this->is_valid_request(params)) {
        LOG_ERROR("ffi_binder: received corrupted binding call, not enough data to respond. called with: {}", params.dump(4));
        return;
    }

    json payload;

    try {
        /** parse the binding payload */
        payload = json::parse(params["payload"].get<std::string>());
        if (!payload.contains("call_id")) {
            throw std::runtime_error("binding payload missing 'call_id' field");
        }

        const auto request_id = payload["call_id"].get<int>();

        const auto t0 = std::chrono::steady_clock::now();
        const auto result = m_ipc_main->process_message(payload);
        const auto t1 = std::chrono::steady_clock::now();

        {
            const int msg_id = payload.value("id", -1);
            const double dur = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::string plugin = payload.value("data", json::object()).value("pluginName", std::string{});
            std::string method = payload.value("data", json::object()).value("methodName", std::string{});

            if (!plugin.empty() && msg_id != ipc_main::ipc_method::FRONT_END_LOADED) {
                mep::ffi_recorder::instance().record({ plugin, method, "fe_to_be", payload.value("data", json::object()).dump(), result.dump(), dur,
                                                       std::chrono::system_clock::now(), payload.value("caller", std::string{}) });
            }
        }

        try {
            this->callback_into_js(params, request_id, result);
        } catch (const std::exception& e) {
            LOG_ERROR("ffi_binder: failed to send binding response: {}", e.what());
            throw;
        }

    } catch (const std::exception& e) {
        LOG_ERROR("ffi_binder: exception while handling binding call: {}", e.what());

        const ordered_json callback_params = {
            { "success",    false    },
            { "returnJson", e.what() }
        };
        /** hope we got the call id before the exception, if not we're fucked */
        this->callback_into_js(params, payload.value("call_id", -1), callback_params);
    }
}

void ffi_binder::execution_ctx_created_hdlr(const json& params)
{
    if (!params.contains("context") || !params.contains("sessionId")) {
        return;
    }

    std::string session_id = params["sessionId"].get<std::string>();

    const auto& context = params["context"];
    if (!context.contains("id")) {
        return;
    }

    int context_id = context["id"].get<int>();

    try {
        const json add_binding_params = {
            { "name",               ffi_constants::binding_name },
            { "executionContextId", context_id                  }
        };
        m_client->send_host("Runtime.addBinding", add_binding_params, session_id).get();
    } catch (...) {
    }

    try {
        const json add_extension_binding_params = {
            { "name",               ffi_constants::extension_binding_name },
            { "executionContextId", context_id                            }
        };
        m_client->send_host("Runtime.addBinding", add_extension_binding_params, session_id).get();
    } catch (...) {
    }

    try {
        const json add_cdp_proxy_params = {
            { "name",               ffi_constants::cdp_proxy_binding_name },
            { "executionContextId", context_id                            }
        };
        m_client->send_host("Runtime.addBinding", add_cdp_proxy_params, session_id).get();
    } catch (...) {
    }

    try {
        const json add_sdk_ready_params = {
            { "name",               ffi_constants::sdk_ready_binding_name },
            { "executionContextId", context_id                            }
        };
        m_client->send_host("Runtime.addBinding", add_sdk_ready_params, session_id).get();
    } catch (...) {
    }
}

void ffi_binder::execution_ctx_destroyed_hdlr(const json& params)
{
    if (!params.contains("executionContextId")) {
        return;
    }

    const int dead_ctx_id = params["executionContextId"].get<int>();

    std::lock_guard<std::mutex> lock(m_ctx_mutex);

    /* rem from reverse lookup so the ID can't be dirty matched later */
    m_ctx_to_plugin.erase(dead_ctx_id);

    for (auto& [name, ctx] : m_plugin_ctxs) {
        if (ctx.main_ctx_id == dead_ctx_id) {
            ctx.main_ctx_id = -1;
        }
    }
}
