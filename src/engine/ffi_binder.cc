/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
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
#include "millennium/logger.h"
#include "millennium/core_ipc.h"
#include "millennium/plugin_webkit_world_mgr.h"

ffi_binder::ffi_binder(std::shared_ptr<cdp_client> client, std::shared_ptr<settings_store> settings_store, std::shared_ptr<ipc_main> ipc_main)
    : m_client(client), m_settings_store(std::move(settings_store)), m_ipc_main(std::move(ipc_main))
{
    m_client->on("Runtime.bindingCalled", std::bind(&ffi_binder::binding_call_hdlr, this, std::placeholders::_1));
    m_client->on("Runtime.executionContextCreated", std::bind(&ffi_binder::execution_ctx_created_hdlr, this, std::placeholders::_1));
}

ffi_binder::~ffi_binder()
{
    m_client->off("Runtime.bindingCalled");
    m_client->off("Runtime.executionContextCreated");

    logger.log("Successfully shut down ffi_binder...");
}

void ffi_binder::callback_into_js(const json params, const int request_id, json result)
{
    json eval_params = {
        { "contextId", params["executionContextId"] },
        { "expression", fmt::format("window.{}.__handleResponse({}, {})", ffi_constants::frontend_binding_name, request_id, result.dump()) }
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

void ffi_binder::binding_call_hdlr(const json& params)
{
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
        const auto result = m_ipc_main->process_message(payload);

        try {
            this->callback_into_js(params, request_id, result);
        } catch (const std::exception& e) {
            LOG_ERROR("ffi_binder: failed to send binding response: {}", e.what());
            throw;
        }

    } catch (const std::exception& e) {
        LOG_ERROR("ffi_binder: exception while handling binding call: {}", e.what());

        const json callback_params = {
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
    if (!context.contains("id") || !context.contains("name")) {
        return;
    }

    std::string context_name = context["name"].get<std::string>();
    if (context_name != webkit_world_mgr::m_context_name) {
        return;
    }

    int context_id = context["id"].get<int>();

    try {
        const json add_binding_params = {
            { "name",               ffi_constants::binding_name },
            { "executionContextId", context_id                  }
        };
        m_client->send_host("Runtime.addBinding", add_binding_params, session_id).get();
        logger.log("ffi_binder: re-added binding to context {} in session {}", context_id, session_id);
    } catch (const std::exception& e) {
        LOG_ERROR("ffi_binder: failed to re-add binding: {}", e.what());
    }
}
