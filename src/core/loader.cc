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

bool PostSocket(nlohmann::json data, 
    websocketpp::client<websocketpp::config::asio_client>* client, websocketpp::connection_hdl handle) 
{
    if (client == nullptr) {
        LOG_ERROR("not connected to steam, cant post message");
        return false;
    }

    client->send(handle, data.dump(), websocketpp::frame::opcode::text);
    return true;
}

bool Sockets::PostShared(nlohmann::json data) 
{
    return PostSocket(data, sharedClient, sharedHandle);
}

bool Sockets::PostGlobal(nlohmann::json data) 
{
    return PostSocket(data, browserClient, browserHandle);
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
    uint16_t m_ftpPort, m_ipcPort;
public:

    SharedJSContext(uint16_t fptPort, uint16_t ipcPort) : m_ftpPort(fptPort), m_ipcPort(ipcPort) { }

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
        CoInitializer::InjectFrontendShims(m_ftpPort, m_ipcPort);
    }
};

PluginLoader::PluginLoader(std::chrono::system_clock::time_point startTime, uint16_t ftpPort) 
    : m_startTime(startTime), m_pluginsPtr(nullptr), m_ftpPort(ftpPort)
{
    m_settingsStorePtr = std::make_unique<SettingsStore>();
    m_pluginsPtr = std::make_shared<std::vector<SettingsStore::PluginTypeSchema>>(m_settingsStorePtr->ParseAllPlugins());

    m_settingsStorePtr->InitializeSettingsStore();
    m_ipcPort = IPCMain::OpenConnection();

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
    SharedJSContext sharedJsHandler(m_ftpPort, m_ipcPort);
    SocketHelpers socketHelpers;

    auto socketStart = std::chrono::high_resolution_clock::now();

    std::thread browserSocketThread = this->ConnectCEFBrowser(&cefBrowserHandler, &socketHelpers);
    std::thread sharedSocketThread  = this->ConnectSharedJSContext(&sharedJsHandler, &socketHelpers);

    auto socketEnd = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - socketStart);
    Logger.LogItem("socket-con", fmt::format("finished in [{}ms]\n", socketEnd.count()), true);


    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - this->m_startTime);
    Logger.Log("total startup time: [{}ms]", duration.count());

    browserSocketThread.join();
    sharedSocketThread.join();

    LOG_ERROR("browser thread and shared thread exited...");
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
    auto start = std::chrono::high_resolution_clock::now();
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

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start);
    Logger.Log("backend python thread started in [{}ms]", duration.count());
}
