#include "loader.hpp"
#include <string>
#include <iostream>
#include <Python.h>
#include <__builtins__/executor.h>
#include <core/py_controller/co_spawn.hpp>
#include <core/ipc/pipe.hpp>
#include <core/ffi/ffi.hpp>
#include <utilities/http.h>
#include <socket/await_pipe.h>
#include <core/hooks/web_load.h>
#include <generic/base.h>
#include <core/co_initialize/co_stub.h>

using namespace std::placeholders;
websocketpp::client<websocketpp::config::asio_client>* sharedClient, *browserClient;
websocketpp::connection_hdl sharedHandle, browserHandle;

enum eSocketTypes {
    SHARED, 
    GLOBAL
};

bool PostSocket(nlohmann::json data, eSocketTypes type) {

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
    std::unique_ptr<WebkitHandler> webKitHandler;

public:

    const void onMessage(websocketpp::client<websocketpp::config::asio_client>* c, websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg)
    {
        const auto json = nlohmann::json::parse(msg->get_payload());
        webKitHandler->DispatchSocketMessage(json);
    }

    const void onConnect(websocketpp::client<websocketpp::config::asio_client>* client, websocketpp::connection_hdl handle)
    {
        browserClient = client; 
        browserHandle = handle;

        Logger.Log("established browser connection @ {:p}", static_cast<void*>(&client));
        webKitHandler->SetupGlobalHooks();
    }

    CEFBrowser() : webKitHandler(&WebkitHandler::get())
    {
    }

    ~CEFBrowser() 
    {
    }
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

        Logger.Log("established shared connection @ {:p}", static_cast<void*>(&sharedClient));
        InjectFrontendShims();
    }
};

struct ConnectSocketProps 
{
    std::string commonName;
    std::function<std::string()> fetchSocketUrl;

    std::function<void(
        websocketpp::client<websocketpp::config::asio_client>*, 
        websocketpp::connection_hdl)> onConnect;

    std::function<void(
        websocketpp::client<websocketpp::config::asio_client>*,
        websocketpp::connection_hdl, 
        std::shared_ptr<websocketpp::config::core_client::message_type>)> onMessage;

    PluginLoader* pluginLoader;
};

void ConnectSocket(ConnectSocketProps socketProps) 
{
    const auto [commonName, fetchSocketUrl, onConnect, onMessage, pluginLoader] = socketProps;

    while (true) 
    {
        const std::string socketUrl = fetchSocketUrl();

        try {
            websocketpp::client<websocketpp::config::asio_client> socketClient;
            socketClient.set_access_channels(websocketpp::log::alevel::none);
            socketClient.init_asio();
            socketClient.set_open_handler(bind(onConnect, &socketClient, std::placeholders::_1));
            socketClient.set_message_handler(bind(onMessage, &socketClient, std::placeholders::_1, std::placeholders::_2));

            websocketpp::lib::error_code ec;
            websocketpp::client<websocketpp::config::asio_client>::connection_ptr con = socketClient.get_connection(socketUrl, ec);

            if (ec) {
                throw websocketpp::exception(ec.message());
            }
            socketClient.connect(con); 
            socketClient.run();
        }
        catch (websocketpp::exception& ex) 
        {
            Logger.Error("webSocket exception thrown -> {}", ex.what());
        }

        Logger.Error("{} tunnel collapsed, attempting to reopen...", commonName);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

PluginLoader::PluginLoader(std::chrono::system_clock::time_point startTime) : m_startTime(startTime), m_pluginsPtr(nullptr)
{
    m_settingsStorePtr = std::make_unique<SettingsStore>();
    m_pluginsPtr = std::make_shared<std::vector<SettingsStore::PluginTypeSchema>>(m_settingsStorePtr->ParseAllPlugins());

    m_settingsStorePtr->InitializeSettingsStore();
    IPCMain::OpenConnection();

    this->PrintActivePlugins();
}

const std::thread PluginLoader::ConnectSharedJSContext(void* sharedJsHandler)
{
    ConnectSocketProps sharedProps;

    sharedProps.commonName     = "SharedJSContext";
    sharedProps.fetchSocketUrl = GetSharedJsContext;
    sharedProps.onConnect      = std::bind(&SharedJSContext::onConnect, (SharedJSContext*)sharedJsHandler, _1, _2);
    sharedProps.onMessage      = std::bind(&SharedJSContext::onMessage, (SharedJSContext*)sharedJsHandler, _1, _2, _3);
    sharedProps.pluginLoader   = this;

    return std::thread(ConnectSocket, sharedProps);
}

const std::thread PluginLoader::ConnectCEFBrowser(void* cefBrowserHandler)
{
    ConnectSocketProps browserProps;

    browserProps.commonName     = "CEFBrowser";
    browserProps.fetchSocketUrl = GetSteamBrowserContext;
    browserProps.onConnect      = std::bind(&CEFBrowser::onConnect, (CEFBrowser*)cefBrowserHandler, _1, _2);
    browserProps.onMessage      = std::bind(&CEFBrowser::onMessage, (CEFBrowser*)cefBrowserHandler, _1, _2, _3);
    browserProps.pluginLoader   = this;

    return std::thread(ConnectSocket, browserProps);
}

const void PluginLoader::StartFrontEnds()
{
    CEFBrowser cefBrowserHandler;
    SharedJSContext sharedJsHandler;

    std::thread browserSocketThread = this->ConnectCEFBrowser(&cefBrowserHandler);
    std::thread sharedSocketThread  = this->ConnectSharedJSContext(&sharedJsHandler);

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

        std::function<void(SettingsStore::PluginTypeSchema&)> cb = std::bind(BackendStartCallback, std::placeholders::_1);
        std::thread(std::bind(&PythonManager::CreatePythonInstance, &manager, std::ref(plugin), cb)).detach();
    }
}
