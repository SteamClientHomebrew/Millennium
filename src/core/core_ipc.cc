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

#include "head/ipc_handler.h"

#include "millennium/auth.h"
#include "millennium/backend_init.h"
#include "millennium/core_ipc.h"
#include "millennium/encode.h"
#include "millennium/ffi.h"
#include "millennium/sysfs.h"

#include <fmt/core.h>
#include <functional>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

/**
 * @brief Handles an internal "call server method" request by dispatching it to the appropriate core IPC handler.
 * This function constructs an EvalResult based on the response from the core IPC handler.
 *
 * @param {nlohmann::basic_json<>} message - The JSON message containing the request data.
 * @returns {EvalResult} - The result of the core IPC method invocation.
 */
EvalResult HandleCoreServerMethod(const nlohmann::basic_json<>& call)
{
    try {
        const auto& functionName = call["data"]["methodName"].get<std::string>();
        const auto& args = call["data"].contains("argumentList") ? call["data"]["argumentList"] : nlohmann::json::object();

        nlohmann::json result = HandleIpcMessage(functionName, args);

        auto it = FFIMap_t.find(result.type());
        return it != FFIMap_t.end() ? EvalResult{ it->second, result.dump() } : EvalResult{ FFI_Type::UnknownType, "core IPC call returned unknown type" };
    }
    /** the internal c ipc method threw an exception */
    catch (const std::exception& ex) {
        return { FFI_Type::Error, ex.what() };
    }
}

/**
 * @brief Handles a plugin "call server method" request by dispatching it to the appropriate backend (Python or Lua).
 * This function uses a static map to associate backend types with their respective handler functions.
 *
 * @param {std::string} pluginName - The name of the plugin making the request.
 * @param {nlohmann::basic_json<>} message - The JSON message containing the request data.
 *
 * @returns {EvalResult} - The result of the backend method invocation.
 * @note assert is used to ensure that the backend type is known and has a corresponding handler.
 */
EvalResult HandlePluginServerMethod(const std::string& pluginName, const nlohmann::basic_json<>& message)
{
    static const std::unordered_map<SettingsStore::PluginBackendType, std::function<EvalResult(const std::string&, const nlohmann::json&)>> handlers = {
        { SettingsStore::PluginBackendType::Python, Python::LockGILAndInvokeMethod },
        { SettingsStore::PluginBackendType::Lua,    Lua::LockAndInvokeMethod       }
    };

    const auto backendType = BackendManager::GetInstance().GetPluginBackendType(pluginName);
    auto it = handlers.find(backendType);
    assert(it != handlers.end() && "HandlePluginServerMethod: Unknown backend type encountered?");

    return it->second(pluginName, message["data"]);
}

/**
 * @brief Determines if a request is internal (i.e., from the core plugin).
 */
bool IsInternalRequest(const std::string& pluginName)
{
    return pluginName == "core";
}

/**
 * Calls a server method based on the provided JSON message and returns a response.
 * A "server method" is a Python function that is called by the IPC server.
 *
 * @param {nlohmann::basic_json<>} message - A JSON message containing the method to be called and its arguments.
 * @returns {nlohmann::json} - A JSON response with the result of the server method call.
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
nlohmann::json CallServerMethod(nlohmann::basic_json<> call)
{
    if (!call["data"].contains("pluginName")) {
        LOG_ERROR("no plugin backend specified, doing nothing...");
        return {};
    }
    const auto pluginName = call["data"]["pluginName"].get<std::string>();

    /** Conditionally dispatch call between internal IPC call, and Python / Lua */
    EvalResult response = IsInternalRequest(pluginName) ? HandleCoreServerMethod(call) : HandlePluginServerMethod(pluginName, call);

    /** Convert the EvalResult into a JSON response */
    auto convertResponseValue = [](const auto& response)
    {
        switch (response.type) {
            case FFI_Type::Boolean:
                /** handle truthy types for python and lua backends */
                return nlohmann::json(response.plain == "True" || response.plain == "true");
            case FFI_Type::Integer:
                return nlohmann::json(std::stoi(response.plain));
            default:
                return nlohmann::json(Base64Encode(response.plain));
        }
    };

    nlohmann::json responseMessage = {
        { "returnType", response.type     },
        { "id",         call["iteration"] }
    };

    if (response.type == FFI_Type::UnknownType || response.type == FFI_Type::Error) {
        responseMessage["failedRequest"] = true;
        responseMessage["failMessage"] = response.plain;
    } else {
        responseMessage["returnValue"] = convertResponseValue(response);
    }

    return responseMessage;
}

/**
 * Handles the event when the frontend is loaded by evaluating the corresponding Python method.
 *
 * @param {nlohmann::basic_json<>} message - A JSON message containing the plugin name and event data.
 * @returns {nlohmann::json} - A JSON response indicating success.
 *
 * This function evaluates the `plugin._front_end_loaded()` method using the provided plugin name and
 * returns a response indicating the success of the operation.
 */
nlohmann::json OnFrontEndLoaded(nlohmann::basic_json<> call)
{
    const std::string pluginName = call["data"]["pluginName"];

    auto settingsStore = std::make_unique<SettingsStore>();
    const auto allPlugins = settingsStore->ParseAllPlugins();

    auto plugin = std::find_if(allPlugins.begin(), allPlugins.end(), [&pluginName](const auto& p) { return p.pluginName == pluginName; });

    /** make sure the plugin has a backend that should be called. */
    if (plugin != allPlugins.end() && plugin->pluginJson.value("useBackend", true)) {
        Logger.Log("Delegating frontend load for plugin: {}", pluginName);

        static const std::unordered_map<SettingsStore::PluginBackendType, std::function<void(const std::string&)>> handlers = {
            { SettingsStore::PluginBackendType::Python, Python::CallFrontEndLoaded },
            { SettingsStore::PluginBackendType::Lua,    Lua::CallFrontEndLoaded    }
        };

        const auto backendType = BackendManager::GetInstance().GetPluginBackendType(pluginName);
        auto it = handlers.find(backendType);
        if (it != handlers.end()) {
            it->second(pluginName);
        } else {
            Logger.Warn("Unknown backend type for plugin '{}'", pluginName);
        }
    }

    /** call["iteration"] is always null/0 as its not provided by the frontend since moving it from socket to http hook */
    return {
        { "id",      call["iteration"] },
        { "success", true              }
    };
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
nlohmann::json IPCMain::HandleEventMessage(nlohmann::json jsonPayload)
{
    try {
        static const std::unordered_map<int, std::function<nlohmann::json(nlohmann::json)>> handlers = {
            { IPCMain::Builtins::CALL_SERVER_METHOD, CallServerMethod },
            { IPCMain::Builtins::FRONT_END_LOADED,   OnFrontEndLoaded }
        };

        int messageId = jsonPayload["id"].get<int>();
        auto it = handlers.find(messageId);

        return it != handlers.end() ? it->second(jsonPayload) : nlohmann::json{};
    } catch (const nlohmann::detail::exception& ex) {
        return {
            { "error", fmt::format("JSON parsing error: {}", ex.what()) },
            { "type", IPCMain::ErrorType::INTERNAL_ERROR }
        };
    } catch (const std::exception& ex) {
        return {
            { "error", fmt::format("An error occurred while processing the message: {}", ex.what()) },
            { "type", IPCMain::ErrorType::INTERNAL_ERROR }
        };
    }
}