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
#include "millennium/core_ipc.h"
#include "millennium/encoding.h"
#include "millennium/logger.h"

#include <cstdint>
#include <fmt/core.h>
#include <functional>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

using namespace std::placeholders;

PyObject* ipc_main::javascript_evaluation_result::to_python() const
{
    if (!valid) {
        if (error == "frontend is not loaded!") {
            PyErr_SetString(PyExc_ConnectionError, error.c_str());
        } else {
            PyErr_SetString(PyExc_RuntimeError, error.c_str());
        }
        return nullptr;
    }

    const json& response_json = std::get<0>(response);
    std::string type = response_json["type"];

    if (type == "string") return PyUnicode_FromString(response_json["value"].get<std::string>().c_str());
    if (type == "boolean") return PyBool_FromLong(response_json["value"]);
    if (type == "number") return PyLong_FromLong(response_json["value"]);

    return PyUnicode_FromString(fmt::format("Js function returned unaccepted type '{}'. Accepted types [string, boolean, number]", type).c_str());
}

int ipc_main::javascript_evaluation_result::to_lua(lua_State* L) const
{
    if (!valid) {
        lua_pushnil(L);
        lua_pushstring(L, error.c_str());
        return 2;
    }

    const json& response_json = std::get<0>(response);

    std::string type = response_json["type"];
    if (type == "string") {
        lua_pushstring(L, response_json["value"].get<std::string>().c_str());
        return 1;
    }
    if (type == "boolean") {
        lua_pushboolean(L, response_json["value"]);
        return 1;
    }
    if (type == "number") {
        lua_pushinteger(L, response_json["value"]);
        return 1;
    }

    lua_pushnil(L);
    lua_pushstring(L, fmt::format("Js function returned unaccepted type '{}'. Accepted types [string, boolean, number]", type).c_str());
    return 2;
}

json ipc_main::javascript_evaluation_result::to_json(const std::string& pluginName) const
{
    return {
        { "returnJson", std::get<0>(response) },
        { "pluginName", pluginName            },
        { "success",    valid                 }
    };
}

ipc_main::javascript_evaluation_result ipc_main::evaluate_javascript_expression(std::string script)
{
    try {
        std::tuple<json, bool> response;

        const json params = {
            { "expression",   script },
            { "awaitPromise", true   }
        };

        auto result = m_cdp->send("Runtime.evaluate", params).get();

        if (result.contains("exceptionDetails")) {
            response = std::make_tuple(result["exceptionDetails"]["exception"]["description"], false);
        } else {
            response = std::make_tuple(result["result"], true);
        }

        if (!std::get<1>(response)) {
            return javascript_evaluation_result(std::move(response), false, std::get<0>(response).get<std::string>());
        }
        return javascript_evaluation_result(std::move(response), true);
    } catch (const nlohmann::detail::exception& ex) {
        return javascript_evaluation_result({}, false, fmt::format("Millennium couldn't decode the response from {}, reason: {}", script, ex.what()));
    } catch (const std::exception&) {
        return javascript_evaluation_result({}, false, "frontend is not loaded!");
    }
}

const std::string ipc_main::compile_javascript_expression(std::string plugin, std::string methodName, std::vector<javascript_parameter> fnParams)
{
    constexpr std::string_view error_handler = R"(
        if (typeof window !== 'undefined' && typeof window.MillenniumFrontEndError === 'undefined') {{
            window.MillenniumFrontEndError = class MillenniumFrontEndError extends Error {{
                constructor(message) {{
                    super(message);
                    this.name = 'MillenniumFrontEndError';
                }}
            }}
        }}
        if (typeof PLUGIN_LIST === 'undefined' || !PLUGIN_LIST?.['{0}']) {{
            throw new window.MillenniumFrontEndError('frontend not loaded yet!');
        }}
    )";

    std::string expression;
    expression.reserve(error_handler.size() + 256);

    expression = fmt::format(error_handler, plugin);
    expression += fmt::format("window.PLUGIN_LIST['{}'].{}(", plugin, methodName);

    for (size_t i = 0; i < fnParams.size(); ++i) {
        if (i > 0) expression += ", ";

        expression += std::visit([](auto&& arg) -> std::string
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
                /** if the param is a string, the best way to escape it
                 * is to just pass it as base64 and decode it on the frontend */
                return fmt::format(R"(atob("{}"))", Base64Encode(arg));
            } else {
                return fmt::format("{}", arg);
            }
        }, fnParams[i]);
    }

    expression += ");";
    return expression;
}

/**
 * @brief Handles an internal "call server method" request by dispatching it to the appropriate core IPC handler.
 * This function constructs an EvalResult based on the response from the core IPC handler.
 *
 * @param {json} message - The JSON message containing the request data.
 * @returns {EvalResult} - The result of the core IPC method invocation.
 */
ipc_main::vm_call_result ipc_main::handle_core_server_method(const json& call)
{
    try {
        auto backend = m_millennium_backend.lock();
        if (!backend) {
            return { false, std::string("millennium_backend is no longer available") };
        }

        const auto& functionName = call["data"]["methodName"].get<std::string>();
        const auto& args = call["data"].contains("argumentList") ? call["data"]["argumentList"] : json::object();

        ordered_json result = backend->ipc_message_hdlr(functionName, args);
        return { true, result.dump() };
    }
    /** the internal c ipc method threw an exception */
    catch (const std::exception& ex) {
        return { false, std::string(ex.what()) };
    }
}

/**
 * @brief Handles a plugin "call server method" request by dispatching it to the appropriate backend (Python or Lua).
 * This function uses a static map to associate backend types with their respective handler functions.
 *
 * @param {std::string} pluginName - The name of the plugin making the request.
 * @param {json} message - The JSON message containing the request data.
 *
 * @returns {EvalResult} - The result of the backend method invocation.
 * @note assert is used to ensure that the backend type is known and has a corresponding handler.
 */
ipc_main::vm_call_result ipc_main::handle_plugin_server_method(const std::string& pluginName, const json& message)
{
    static const std::unordered_map<plugin_manager::backend_t, std::function<vm_call_result(const std::string&, const json&)>> handlers = {
        { plugin_manager::backend_t::Python, std::bind(&ipc_main::python_evaluate, this, _1, _2) },
        { plugin_manager::backend_t::Lua,    std::bind(&ipc_main::lua_evaluate,    this, _1, _2) },
    };

    auto backend = m_backend_manager.lock();
    if (!backend) {
        logger.warn("HandlePluginServerMethod: backend_manager is no longer available");
        return { false, std::string("backend_manager unavailable") };
    }

    const auto backendType = backend->get_plugin_backend_type(pluginName);
    auto it = handlers.find(backendType);
    assert(it != handlers.end() && "HandlePluginServerMethod: Unknown backend type encountered?");

    return it->second(pluginName, message["data"]);
}

/**
 * Calls a server method based on the provided JSON message and returns a response.
 * A "server method" is a Python function that is called by the IPC server.
 *
 * @param {json} message - A JSON message containing the method to be called and its arguments.
 * @returns {json} - A JSON response with the result of the server method call.
 *
 * This function constructs a function call script using the provided message, evaluates it using Python,
 * and returns the result. The response may contain the return value, or in case of an error, the error message.
 *
 * The return value's type is determined based on the Python evaluation result:
 * - Boolean: Returned as `true` or `false`.
 * - String: Returned as a Base64-encoded string.
 * - Integer: Returned as an integer.
 * - Error: Returns a failure message and a flag indicating the failure.
 */
json ipc_main::call_server_method(const json& call)
{
    const auto& data = call["data"];
    if (!data.contains("pluginName")) {
        LOG_ERROR("no plugin backend specified, doing nothing...");
        return {};
    }

    const std::string pluginName = data["pluginName"];
    const auto response = pluginName == "core" ? handle_core_server_method(call) : handle_plugin_server_method(pluginName, call);

    json responseMessage{
        { "success",    response.success },
        { "pluginName", pluginName       }
    };

    std::visit([&](auto&& arg)
    {
        if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, std::monostate>)
            responseMessage["returnJson"] = nullptr;
        else
            responseMessage["returnJson"] = arg;
    }, response.value);

    return responseMessage;
}

/**
 * Handles the event when the frontend is loaded by evaluating the corresponding Python method.
 *
 * @param {json} message - A JSON message containing the plugin name and event data.
 * @returns {json} - A JSON response indicating success.
 *
 * This function evaluates the `plugin._front_end_loaded()` method using the provided plugin name and
 * returns a response indicating the success of the operation.
 */
json ipc_main::on_front_end_loaded(const json& call)
{
    const std::string pluginName = call["data"]["pluginName"];

    const auto plugins = m_settings_store->get_all_plugins();
    auto plugin = std::find_if(plugins.begin(), plugins.end(), [&pluginName](const auto& p) { return p.plugin_name == pluginName; });

    /** make sure the plugin has a backend that should be called. */
    if (plugin != plugins.end() && plugin->plugin_json.value("useBackend", true)) {
        logger.log("Delegating frontend load for plugin: {}", pluginName);

        static const std::unordered_map<plugin_manager::backend_t, std::function<void(const std::string&)>> handlers = {
            { plugin_manager::backend_t::Python, std::bind(&ipc_main::python_call_frontend_loaded, this, _1) },
            { plugin_manager::backend_t::Lua,    std::bind(&ipc_main::lua_call_frontend_loaded,    this, _1) }
        };

        auto backend = m_backend_manager.lock();
        if (!backend) {
            logger.warn("Delegating frontend load for plugin: {} but backend_manager not available", pluginName);
        } else {
            const auto backendType = backend->get_plugin_backend_type(pluginName);
            auto it = handlers.find(backendType);
            if (it != handlers.end()) {
                it->second(pluginName);
            } else {
                logger.warn("Unknown backend type for plugin '{}'", pluginName);
            }
        }
    }

    /** call["iteration"] is always null/0 as its not provided by the frontend since moving it from socket to http hook */
    return {
        { "id",      (call.contains("iteration") ? call["iteration"] : nlohmann::json(nullptr)) },
        { "success", true                                                                       }
    };
}

json ipc_main::call_frontend_method(const json& call)
{
    std::vector<javascript_parameter> params;

    if (call["data"].contains("argumentList")) {
        for (const auto& arg : call["data"]["argumentList"]) {

            if (arg.is_string())
                params.push_back(arg.get<std::string>());
            else if (arg.is_boolean())
                params.push_back(arg.get<bool>());
            else if (arg.is_number_float())
                params.push_back(arg.get<double>());
            else if (arg.is_number_integer())
                params.push_back(arg.get<int64_t>());
            else if (arg.is_number_unsigned())
                params.push_back(arg.get<uint64_t>());
        }
    }

    const std::string script = this->compile_javascript_expression(call["data"]["pluginName"], call["data"]["methodName"], params);
    return this->evaluate_javascript_expression(script).to_json(call["data"]["pluginName"]);
}

/**
 * Handles incoming WebSocket messages and dispatches them to the appropriate handler function.
 *
 * @param {socketServer*} serv - A pointer to the WebSocket server instance.
 * @param {websocketpp::connection_hdl} hdl - The connection handle.
 * @param {socketServer::message_ptr} msg - The WebSocket message received.
 *
 * This function checks the message's `id` field and invokes the appropriate server method handler,
 * such as `CallServerMethod` or `OnFrontEndLoaded`. If any exceptions are caught, they are sent back
 * as error messages.
 */
json ipc_main::process_message(const json payload)
{
    try {
        const std::unordered_map<int, std::function<nlohmann::json(nlohmann::json)>> handlers = {
            { ipc_main::ipc_method::CALL_SERVER_METHOD,   std::bind(&ipc_main::call_server_method,   this, _1) },
            { ipc_main::ipc_method::FRONT_END_LOADED,     std::bind(&ipc_main::on_front_end_loaded,  this, _1) },
            { ipc_main::ipc_method::CALL_FRONTEND_METHOD, std::bind(&ipc_main::call_frontend_method, this, _1) }
        };

        int messageId = payload["id"].get<int>();
        auto it = handlers.find(messageId);

        return it != handlers.end() ? it->second(payload) : nlohmann::json{};
    } catch (const nlohmann::detail::exception& ex) {
        return {
            { "error", fmt::format("JSON parsing error: {}", ex.what()) },
            { "type", ipc_main::ipc_error::INTERNAL_ERROR }
        };
    } catch (const std::exception& ex) {
        return {
            { "error", fmt::format("An error occurred while processing the message: {}", ex.what()) },
            { "type", ipc_main::ipc_error::INTERNAL_ERROR }
        };
    }
}
