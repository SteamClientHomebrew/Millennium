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
#include <sys/log.h>

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

        Logger.Log("Connected to Steam @ {}", (void*)client);
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

        if (json.contains("method") && json["method"] == "Console.messageAdded") 
        {
            try
            {
                if (json["params"]["message"]["level"] == "error")
                {
                    const auto data = json["params"]["message"];

                    std::string errorMessage = data["text"];
                    std::string url = data["url"];
                    int line = data["line"];
                    int column = data["column"];

                    std::string traceMessage = fmt::format("{}:{}:{}", url, line, column);
                    Logger.Warn("(Steam-Error) {}\n  Info: (This warning may be non-fatal)\n  Where: ({})", errorMessage, traceMessage);
                }
            }
            catch (const std::exception& e) { }
        }
        else
        {        
            JavaScript::SharedJSMessageEmitter::InstanceRef().EmitMessage("msg", json);
        }
    }

    const void onConnect(websocketpp::client<websocketpp::config::asio_client>* client, websocketpp::connection_hdl handle)
    {
        sharedClient = client; 
        sharedHandle = handle;

        Logger.Log("Connected to SharedJSContext @ {}", (void*)client);
        CoInitializer::InjectFrontendShims(m_ftpPort, m_ipcPort);

        Sockets::PostShared({ {"id", 9494 }, {"method", "Console.enable"} });
    }
};

PluginLoader::PluginLoader(std::chrono::system_clock::time_point startTime, uint16_t ftpPort) 
    : m_startTime(startTime), m_pluginsPtr(nullptr), m_ftpPort(ftpPort)
{
    m_settingsStorePtr = std::make_unique<SettingsStore>();
    m_pluginsPtr = std::make_shared<std::vector<SettingsStore::PluginTypeSchema>>(m_settingsStorePtr->ParseAllPlugins());

    m_settingsStorePtr->InitializeSettingsStore();
    m_ipcPort = IPCMain::OpenConnection();

    Logger.LogHead("Internal Process Ports:");
    Logger.LogItem("+", fmt::format("File Transfer Protocol Port: {}", m_ftpPort));
    Logger.LogItem("+", fmt::format("Inter Process Communication Port: {}", m_ipcPort), true);

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
    Logger.LogItem("+", fmt::format("Finished in [{}ms]\n", socketEnd.count()), true);


    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - this->m_startTime);
    Logger.Log("Startup took {} ms", duration.count());

    browserSocketThread.join();
    sharedSocketThread.join();

    LOG_ERROR("browser thread and shared thread exited...");
    this->StartFrontEnds();
}

/* debug function, just for developers */
const void PluginLoader::PrintActivePlugins()
{
    Logger.LogHead("Plugin States:");

    for (auto it = (*this->m_pluginsPtr).begin(); it != (*this->m_pluginsPtr).end(); ++it)
    {
        const auto pluginName = (*it).pluginName;
        Logger.LogItem(pluginName, m_settingsStorePtr->IsEnabledPlugin(pluginName) ? "Enabled" : "Disabled", std::next(it) == (*this->m_pluginsPtr).end());
    }
}

const void StartPreloader(PythonManager& manager)
{
    std::promise<void> promise;

    SettingsStore::PluginTypeSchema plugin = 
    {
        .pluginName = "pipx",
        .backendAbsoluteDirectory = SystemIO::GetSteamPath() / "ext" / "data" / "assets" / "pipx",
        .isInternal = true
    };

    manager.CreatePythonInstance(plugin, [&promise](SettingsStore::PluginTypeSchema plugin) 
    {
        Logger.Log("Started Preloader...", plugin.pluginName);

        const auto backendMainModule = (plugin.backendAbsoluteDirectory / "main.py").generic_string();

        PyObject *mainModuleObj = Py_BuildValue("s", backendMainModule.c_str());
        FILE *mainModuleFilePtr = _Py_fopen_obj(mainModuleObj, "r+");

        if (mainModuleFilePtr == NULL) 
        {
            LOG_ERROR("failed to fopen file @ {}", backendMainModule);
            return;
        }

        if (PyRun_SimpleFile(mainModuleFilePtr, backendMainModule.c_str()) != 0) 
        {
            LOG_ERROR("millennium failed to preload plugins", plugin.pluginName);
            return;
        }

        promise.set_value();
    });

    promise.get_future().get();
    manager.RemovePythonInstance("pipx"); // shutdown preloader
}

const void PluginLoader::StartBackEnds(PythonManager& manager)
{
    StartPreloader(manager);

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
