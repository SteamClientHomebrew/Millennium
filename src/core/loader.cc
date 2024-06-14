#include "loader.h"
#include <string>
#include <iostream>
#include <Python.h>
#include <api/executor.h>
#include <core/co_initialize/co_stub.h>
#include <core/py_controller/co_spawn.h>
#include <core/ipc/pipe.h>
#include <core/ffi/ffi.h>
#include <sys/http.h>
#include <core/hooks/web_load.h>

using namespace std::placeholders;
websocketpp::client<websocketpp::config::asio_client>* sharedClient, *browserClient;
websocketpp::connection_hdl sharedHandle, browserHandle;

enum eSocketTypes 
{
    SHARED, 
    GLOBAL
};

bool PostSocket(nlohmann::json data, eSocketTypes type) 
{
    const auto client = (type == eSocketTypes::SHARED ? sharedClient : browserClient);
    const auto handle = (type == eSocketTypes::SHARED ? sharedHandle : browserHandle);

    if (client == nullptr) {
        Logger.Error("not connected to steam, cant post message");
        return false;
    }

    client->send(handle, data.dump(), websocketpp::frame::opcode::text);
    return true;
}

bool Sockets::PostShared(nlohmann::json data) 
{
    return PostSocket(data, eSocketTypes::SHARED);
}

bool Sockets::PostGlobal(nlohmann::json data) 
{
    return PostSocket(data, eSocketTypes::GLOBAL);
}

class CEFBrowser
{
    WebkitHandler webKitHandler;

public:

    const void onMessage(websocketpp::client<websocketpp::config::asio_client>* c, websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg)
    {
        const auto json = nlohmann::json::parse(msg->get_payload());
        webKitHandler.DispatchSocketMessage(json);
    }

    const void onConnect(websocketpp::client<websocketpp::config::asio_client>* client, websocketpp::connection_hdl handle)
    {
        browserClient = client; 
        browserHandle = handle;

        Logger.Log("successfully connected to steam!");
        webKitHandler.SetupGlobalHooks();
    }

    CEFBrowser() : webKitHandler(WebkitHandler::get()) { }
};

class SharedJSContext
{
public:

    const void onMessage(websocketpp::client<websocketpp::config::asio_client>*c, websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg)
    {
        const auto json = nlohmann::json::parse(msg->get_payload());
        JavaScript::SharedJSMessageEmitter::InstanceRef().EmitMessage("msg", json);
    }

    const void onConnect(websocketpp::client<websocketpp::config::asio_client>* client, websocketpp::connection_hdl handle)
    {
        sharedClient = client; 
        sharedHandle = handle;

        Logger.Log("successfully connected to steam JSVM!");
        CoInitializer::InjectFrontendShims();
    }
};

PluginLoader::PluginLoader(std::chrono::system_clock::time_point startTime) : m_startTime(startTime), m_pluginsPtr(nullptr)
{
    m_settingsStorePtr = std::make_unique<SettingsStore>();
    m_pluginsPtr = std::make_shared<std::vector<SettingsStore::PluginTypeSchema>>(m_settingsStorePtr->ParseAllPlugins());

    m_settingsStorePtr->InitializeSettingsStore();
    IPCMain::OpenConnection();

    this->PrintActivePlugins();
}

const std::thread PluginLoader::ConnectSharedJSContext(void* sharedJsHandler, SocketHelpers* socketHelpers)
{
    SocketHelpers::ConnectSocketProps sharedProps;

    sharedProps.commonName     = "SharedJSContext";
    sharedProps.fetchSocketUrl = std::bind(&SocketHelpers::GetSharedJsContext, socketHelpers);
    sharedProps.onConnect      = std::bind(&SharedJSContext::onConnect, (SharedJSContext*)sharedJsHandler, _1, _2);
    sharedProps.onMessage      = std::bind(&SharedJSContext::onMessage, (SharedJSContext*)sharedJsHandler, _1, _2, _3);

    return std::thread(std::bind(&SocketHelpers::ConnectSocket, socketHelpers, sharedProps));
}

const std::thread PluginLoader::ConnectCEFBrowser(void* cefBrowserHandler, SocketHelpers* socketHelpers)
{
    SocketHelpers::ConnectSocketProps browserProps;

    browserProps.commonName     = "CEFBrowser";
    browserProps.fetchSocketUrl = std::bind(&SocketHelpers::GetSteamBrowserContext, socketHelpers);
    browserProps.onConnect      = std::bind(&CEFBrowser::onConnect, (CEFBrowser*)cefBrowserHandler, _1, _2);
    browserProps.onMessage      = std::bind(&CEFBrowser::onMessage, (CEFBrowser*)cefBrowserHandler, _1, _2, _3);

    return std::thread(std::bind(&SocketHelpers::ConnectSocket, socketHelpers, browserProps));
}

const void PluginLoader::StartFrontEnds()
{
    CEFBrowser cefBrowserHandler;
    SharedJSContext sharedJsHandler;
    SocketHelpers socketHelpers;

    std::thread browserSocketThread = this->ConnectCEFBrowser(&cefBrowserHandler, &socketHelpers);
    std::thread sharedSocketThread  = this->ConnectSharedJSContext(&sharedJsHandler, &socketHelpers);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - this->m_startTime);
    Logger.LogItem("exec", fmt::format("finished in [{}ms]\n", duration.count()), true);

    browserSocketThread.join();
    sharedSocketThread.join();

    Logger.Log("browser thread and shared thread exited...");
    this->StartFrontEnds();
}

/* debug function, just for developers */
const void PluginLoader::PrintActivePlugins()
{
    Logger.LogHead("hosting query:");

    for (auto it = (*this->m_pluginsPtr).begin(); it != (*this->m_pluginsPtr).end(); ++it)
    {
        const auto pluginName = (*it).pluginName;
        Logger.LogItem(pluginName, m_settingsStorePtr->IsEnabledPlugin(pluginName) ? "enabled" : "disabled", std::next(it) == (*this->m_pluginsPtr).end());
    }
}

const void PluginLoader::StartBackEnds()
{
    PythonManager& manager = PythonManager::GetInstance();

    for (auto& plugin : *this->m_pluginsPtr)
    {
        if (!m_settingsStorePtr->IsEnabledPlugin(plugin.pluginName)) 
        {
            continue;
        }

        std::function<void(SettingsStore::PluginTypeSchema)> cb = std::bind(CoInitializer::BackendStartCallback, std::placeholders::_1);

        std::thread(
            [&manager, &plugin, cb]() {
                manager.CreatePythonInstance(plugin, cb);
            }
        ).detach();
    }
}
