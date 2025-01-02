#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "pipe.h"
#include <core/ffi/ffi.h>
#include <sys/encoding.h>
#include <core/ipc/pipe.h>
#include <functional>
#include <sys/asio.h>

typedef websocketpp::server<websocketpp::config::asio> socketServer;

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

static nlohmann::json OnFrontEndLoaded(nlohmann::basic_json<> message)
{
    Python::LockGILAndDiscardEvaluate(message["data"]["pluginName"], "plugin._front_end_loaded()");

    return nlohmann::json({
        { "id", message["iteration"] },
        { "success", true }
    });
}

static nlohmann::json GetFrontendSettings(nlohmann::basic_json<> message)
{
    const std::string pluginName = message["data"]["pluginName"];

    JavaScript::EvalResult response = JavaScript::ExecuteOnSharedJsContext(fmt::format("PLUGIN_LIST?.['{}']?.settings?.serialize()", pluginName));

    return nlohmann::json({
        { "id", message["iteration"] },
        { "success", true },
        { "settings", response.json },
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
            case IPCMain::Builtins::GET_FRONTEND_SETTINGS:
            {
                responseMessage = GetFrontendSettings(json_data).dump();
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

const uint16_t IPCMain::OpenConnection()
{
    uint16_t ipcPort = Asio::GetRandomOpenPort();
    std::thread(OpenIPCSocket, ipcPort).detach();
    return ipcPort;
}
