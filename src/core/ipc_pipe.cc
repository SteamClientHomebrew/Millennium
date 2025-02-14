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

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "pipe.h"
#include "ffi.h"
#include "encoding.h"
#include "pipe.h"
#include <functional>
#include "asio.h"

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
static nlohmann::json CallServerMethod(nlohmann::basic_json<> message)
{
    const std::string fnCallScript = Python::ConstructFunctionCall(message["data"]);

    if (!message["data"].contains("pluginName")) 
    {
        LOG_ERROR("no plugin backend specified, doing nothing...");
        return {};
    }

    Python::EvalResult response = Python::LockGILAndEvaluate(message["data"]["pluginName"], fnCallScript);
    nlohmann::json responseMessage;

    switch (response.type)
    {
        case Python::Types::Boolean: { responseMessage["returnValue"] = (response.plain == "True" ? true : false); break; }
        case Python::Types::String:  { responseMessage["returnValue"] = Base64Encode(response.plain);              break; }
        case Python::Types::Integer: { responseMessage["returnValue"] = stoi(response.plain);                      break; }

        case Python::Types::Error: 
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
static nlohmann::json OnFrontEndLoaded(nlohmann::basic_json<> message)
{
    Python::LockGILAndDiscardEvaluate(message["data"]["pluginName"], "plugin._front_end_loaded()");

    return nlohmann::json({
        { "id", message["iteration"] },
        { "success", true }
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
void OnMessage(socketServer* serv, websocketpp::connection_hdl hdl, socketServer::message_ptr msg) 
{
    socketServer::connection_ptr serverConnection = serv->get_con_from_hdl(hdl);

    try
    {
        auto json_data = nlohmann::json::parse(msg->get_payload());
        std::string responseMessage;

        switch (json_data["id"].get<int>()) 
        {
            case IPCMain::Builtins::CALL_SERVER_METHOD: 
            {
                responseMessage = CallServerMethod(json_data).dump(); 
                break;
            }
            case IPCMain::Builtins::FRONT_END_LOADED: 
            {
                responseMessage = OnFrontEndLoaded(json_data).dump(); 
                break;
            }
        }
        serverConnection->send(responseMessage, msg->get_opcode());
    }
    catch (nlohmann::detail::exception& ex) 
    {
        serverConnection->send(std::string(ex.what()), msg->get_opcode());
    }
    catch (std::exception& ex) 
    {
        serverConnection->send(std::string(ex.what()), msg->get_opcode());
    }
}

/**
 * Handles the WebSocket connection open event, starting the server's accept loop.
 */
void OnOpen(socketServer* IPCSocketMain, websocketpp::connection_hdl hdl) 
{
    IPCSocketMain->start_accept();
}

/**
 * Opens a WebSocket server for IPC communication on a specified port.
 *
 * @param {uint16_t} ipcPort - The port number to open the WebSocket server on.
 * @returns {int} - Returns `true` if the WebSocket server was successfully opened, `false` if an error occurred.
 * 
 * This function initializes a WebSocket server, sets up necessary handlers for incoming messages and connections,
 * listens on the specified port, and runs the server. If any exceptions occur during initialization or handling,
 * they are caught and logged.
 */
const int OpenIPCSocket(uint16_t ipcPort) 
{
    socketServer IPCSocketMain;

    try 
    {
        IPCSocketMain.set_access_channels(websocketpp::log::alevel::none);
        IPCSocketMain.clear_error_channels(websocketpp::log::elevel::none);
        IPCSocketMain.set_error_channels(websocketpp::log::elevel::none);

        IPCSocketMain.init_asio();
        IPCSocketMain.set_message_handler(bind(OnMessage, &IPCSocketMain, std::placeholders::_1, std::placeholders::_2));
        IPCSocketMain.set_open_handler(bind(&OnOpen, &IPCSocketMain, std::placeholders::_1));

        IPCSocketMain.listen(ipcPort);
        IPCSocketMain.start_accept();
        IPCSocketMain.run();
    }
    catch (const std::exception& error) 
    {
        LOG_ERROR("[ipcMain] uncaught error -> {}", error.what());
        return false;
    }
    return true;
}

/**
 * Opens an asynchronous WebSocket connection for IPC communication.
 *
 * @returns {uint16_t} - The port number on which the WebSocket server is running.
 * 
 * This function obtains a random open port, initializes the WebSocket server, and runs it in a separate thread.
 * The assigned port number is returned.
 */
const uint16_t IPCMain::OpenConnection()
{
    uint16_t ipcPort = Asio::GetRandomOpenPort();
    std::thread(OpenIPCSocket, ipcPort).detach();
    return ipcPort;
}
