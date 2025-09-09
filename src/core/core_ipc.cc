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

typedef websocketpp::server<websocketpp::config::asio> socketServer;

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
nlohmann::json CallServerMethod(nlohmann::basic_json<> message)
{
    if (!message["data"].contains("pluginName")) {
        LOG_ERROR("no plugin backend specified, doing nothing...");
        return {};
    }

    EvalResult response;
    const auto pluginName = message["data"]["pluginName"].get<std::string>();

    /** An internal IPC call, handle it in C++ instead of Python or Lua */
    if (pluginName == "core") {
        try {
            const auto& functionName = message["data"]["methodName"].get<std::string>();
            const auto& args = message["data"].contains("argumentList") ? message["data"]["argumentList"] : nlohmann::json::object();

            nlohmann::json result = HandleIpcMessage(functionName, args);

            response.plain = result.dump();

            if (result.is_boolean())
                response.type = FFI_Type::Boolean;
            else if (result.is_string())
                response.type = FFI_Type::String;
            else if (result.is_number_integer())
                response.type = FFI_Type::Integer;
            else if (result.is_object() || result.is_array())
                response.type = FFI_Type::JSON;
            else {
                response.type = FFI_Type::UnknownType;
                response.plain = "core IPC call returned unknown type";
            }
        } catch (const std::exception& ex) {
            LOG_ERROR("Error handling core IPC call: {}", ex.what());

            response.type = FFI_Type::Error;
            response.plain = ex.what();
        }
    } else {
        const auto backendType = BackendManager::GetInstance().GetPluginBackendType(pluginName);

        switch (backendType) {
            case SettingsStore::PluginBackendType::Python:
            {
                response = Python::LockGILAndInvokeMethod(pluginName, message["data"]);
                break;
            }
            case SettingsStore::PluginBackendType::Lua:
            {
                response = Lua::LockAndInvokeMethod(pluginName, message["data"]);
                break;
            }
            default:
            {
                LOG_ERROR("Unknown backend type for plugin '{}'", pluginName);
                return {};
            }
        }
    }

    nlohmann::json responseMessage;

    responseMessage["returnType"] = response.type;

    switch (response.type) {
        case FFI_Type::Boolean:
        {
            /** handle truthy for both python and lua */
            responseMessage["returnValue"] = ((response.plain == "True" || response.plain == "true") ? true : false);
            break;
        }
        case FFI_Type::String:
        {
            responseMessage["returnValue"] = Base64Encode(response.plain);
            break;
        }
        case FFI_Type::JSON:
        {
            responseMessage["returnValue"] = Base64Encode(response.plain);
            break;
        }
        case FFI_Type::Integer:
        {
            responseMessage["returnValue"] = stoi(response.plain);
            break;
        }

        case FFI_Type::UnknownType:
        case FFI_Type::Error:
        {
            responseMessage["failedRequest"] = true;
            responseMessage["failMessage"] = response.plain;
            break;
        }
    }

    responseMessage["id"] = message["iteration"];
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
nlohmann::json OnFrontEndLoaded(nlohmann::basic_json<> message)
{
    const std::string pluginName = message["data"]["pluginName"];

    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
    const auto allPlugins = settingsStore->ParseAllPlugins();

    /** Check if the plugin uses a backend, and if so delegate the notification. */
    for (auto& plugin : allPlugins) {
        if (plugin.pluginName != pluginName) {
            continue;
        }

        if (!plugin.pluginJson.value("useBackend", true)) {
            continue;
        }

        Logger.Log("Delegating frontend load for plugin: {}", pluginName);
        const auto backendType = BackendManager::GetInstance().GetPluginBackendType(pluginName);

        switch (backendType) {
            case SettingsStore::PluginBackendType::Python:
            {
                Python::CallFrontEndLoaded(pluginName);
                break;
            }
            case SettingsStore::PluginBackendType::Lua:
            {
                Lua::CallFrontEndLoaded(pluginName);
                break;
            }
            default:
            {
                Logger.Warn("Unknown backend type for plugin '{}'", pluginName);
                break;
            }
        }
        break;
    }

    /** Return a success message. */
    return nlohmann::json({
        { "id",      message["iteration"] },
        { "success", true                 }
    });
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
        nlohmann::json responseMessage;

        switch (jsonPayload["id"].get<int>()) {
            case IPCMain::Builtins::CALL_SERVER_METHOD:
            {
                responseMessage = CallServerMethod(jsonPayload);
                break;
            }
            case IPCMain::Builtins::FRONT_END_LOADED:
            {
                responseMessage = OnFrontEndLoaded(jsonPayload);
                break;
            }
        }
        return responseMessage;
    } catch (nlohmann::detail::exception& ex) {
        return {
            { "error", fmt::format("JSON parsing error: {}", ex.what()), "type", IPCMain::ErrorType::INTERNAL_ERROR }
        };
    } catch (std::exception& ex) {
        return {
            { "error", fmt::format("An error occurred while processing the message: {}", ex.what()) },
            { "type", IPCMain::ErrorType::INTERNAL_ERROR }
        };
    }
}