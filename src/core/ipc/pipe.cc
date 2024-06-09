#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "pipe.h"
#include <core/ffi/ffi.h>
#include <sys/encoding.h>
#include <core/ipc/pipe.h>
#include <functional>

typedef websocketpp::server<websocketpp::config::asio> socketServer;

static nlohmann::json CallServerMethod(nlohmann::basic_json<> message)
{
    const std::string fnCallScript = Python::ConstructFunctionCall(message["data"]);

    if (!message["data"].contains("pluginName")) 
    {
        Logger.Error("no plugin backend specified, doing nothing...");
        return {};
    }

    Python::EvalResult response = Python::LockGILAndEvaluate(message["data"]["pluginName"], fnCallScript);
    nlohmann::json responseMessage;

    switch (response.type)
    {
        case Python::Types::Boolean:   { responseMessage["returnValue"] = (response.plain == "True" ? true : false); break; }
        case Python::Types::String:    { responseMessage["returnValue"] = Base64Encode(response.plain); break; }
        case Python::Types::Integer:   { responseMessage["returnValue"] = stoi(response.plain); break; }

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

static nlohmann::json OnFrontEndLoaded(nlohmann::basic_json<> message)
{
    Python::LockGILAndDiscardEvaluate(message["data"]["pluginName"], "plugin._front_end_loaded()");

    return nlohmann::json({
        { "id", message["iteration"] },
        { "success", true }
    });
}

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

void OnOpen(socketServer* IPCSocketMain, websocketpp::connection_hdl hdl) 
{
    IPCSocketMain->start_accept();
}

const int OpenIPCSocket() 
{
    socketServer IPCSocketMain;

    try 
    {
        IPCSocketMain.set_access_channels(websocketpp::log::alevel::none);

        IPCSocketMain.init_asio();
        IPCSocketMain.set_message_handler(bind(OnMessage, &IPCSocketMain, std::placeholders::_1, std::placeholders::_2));
        IPCSocketMain.set_open_handler(bind(&OnOpen, &IPCSocketMain, std::placeholders::_1));

        IPCSocketMain.listen(12906);
        IPCSocketMain.start_accept();
        IPCSocketMain.run();
    }
    catch (const std::exception& error) 
    {
        Logger.Error("[ipcMain] uncaught error -> {}", error.what());
        return false;
    }
    return true;
}

const int IPCMain::OpenConnection()
{
    std::thread(OpenIPCSocket).detach();
    return true;
}
