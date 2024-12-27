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
using namespace std::chrono;
websocketpp::client<websocketpp::config::asio_client>*browserClient;
websocketpp::connection_hdl browserHandle;

std::string sharedJsContextSessionId;
std::shared_ptr<InterpreterMutex> g_threadTerminateFlag = std::make_shared<InterpreterMutex>();

bool Sockets::PostShared(nlohmann::json data) 
{
    if (sharedJsContextSessionId.empty()) 
    {
        return false;
    }

    data["sessionId"] = sharedJsContextSessionId;
    return Sockets::PostGlobal(data);
}

bool Sockets::PostGlobal(nlohmann::json data) 
{
    if (browserClient == nullptr) 
    {
        return false;
    }

    browserClient->send(browserHandle, data.dump(), websocketpp::frame::opcode::text);
    return true;
}

void Sockets::Shutdown() 
{
    try
    {
        if (browserClient != nullptr) 
        {
            browserClient->close(browserHandle, websocketpp::close::status::normal, "Shutting down");
        }
    }
    catch(const websocketpp::exception& e)
    {
        LOG_ERROR("Failed to close browser connection: {}", e.what());
    }
    
}

class CEFBrowser
{
    WebkitHandler webKitHandler;
    uint16_t m_ftpPort, m_ipcPort;
    bool m_sharedJsConnected = false;

    std::chrono::system_clock::time_point m_startTime;
public:

    const void HandleConsoleMessage(const nlohmann::json& json)
    {
        try
        {
            if (json["params"]["message"]["level"] == "error")
            {
                const auto data = json["params"]["message"];

                std::string traceMessage = fmt::format("{}:{}:{}", data["url"].get<std::string>(), data["line"].get<int>(), data["column"].get<int>());
                Logger.Warn("(Steam-Error) {}\n  Info: (This warning may be non-fatal)\n  Where: ({})", data["text"].get<std::string>(), traceMessage);
            }
        }
        catch (const std::exception& e) { }
    }

    const void onMessage(websocketpp::client<websocketpp::config::asio_client>* c, websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg)
    {
        const auto json = nlohmann::json::parse(msg->get_payload());
        const std::string method = json.value("method", std::string());
        
        if (json.contains("id") && json["id"] == 0 && 
            json.contains("result") && json["result"].is_object() && 
            json["result"].contains("targetInfos") && json["result"]["targetInfos"].is_array()) 
        { 
            const auto targets = json["result"]["targetInfos"];
            auto targetIterator = std::find_if(targets.begin(), targets.end(), [](const auto& target) { return target["title"] == "SharedJSContext"; });
            
            if (targetIterator != targets.end() && !m_sharedJsConnected) 
            {
                Sockets::PostGlobal({ { "id", 0 }, { "method", "Target.attachToTarget" }, 
                    { "params", { { "targetId", (*targetIterator)["targetId"] }, { "flatten", true } } } 
                });
                m_sharedJsConnected = true;
            }
            else if (!m_sharedJsConnected)
            {
                this->SetupSharedJSContext();
            }
        }
        
        if (method == "Target.attachedToTarget" && json["params"]["targetInfo"]["title"] == "SharedJSContext")
        {
            sharedJsContextSessionId = json["params"]["sessionId"];
            this->onSharedJsConnect();
        }
        else if (method == "Console.messageAdded") 
        {
            this->HandleConsoleMessage(json);
        }
        else
        {        
            JavaScript::SharedJSMessageEmitter::InstanceRef().EmitMessage("msg", json);
        }
        webKitHandler.DispatchSocketMessage(json);
    }

    const void SetupSharedJSContext()
    {
        Sockets::PostGlobal({ { "id", 0 }, { "method", "Target.getTargets" } });
    }

    const void onSharedJsConnect()
    {
        std::thread([this]() {
            Logger.Log("Connected to SharedJSContext in {} ms", duration_cast<milliseconds>(system_clock::now() - m_startTime).count());
            CoInitializer::InjectFrontendShims(m_ftpPort, m_ipcPort);
            Sockets::PostShared({ {"id", 9494 }, {"method", "Console.enable"} });
        }).detach();
    }

    const void onConnect(websocketpp::client<websocketpp::config::asio_client>* client, websocketpp::connection_hdl handle)
    {
        m_startTime = std::chrono::system_clock::now();
        browserClient = client; 
        browserHandle = handle;

        Logger.Log("Connected to Steam @ {}", (void*)client);

        this->SetupSharedJSContext();
        webKitHandler.SetupGlobalHooks();
    }

    CEFBrowser(uint16_t fptPort, uint16_t ipcPort) : m_ftpPort(fptPort), m_ipcPort(ipcPort), webKitHandler(WebkitHandler::get()) 
    {
        webKitHandler.SetIPCPort(ipcPort);
    }
};

const void PluginLoader::Initialize()
{
    m_settingsStorePtr = std::make_unique<SettingsStore>();
    m_pluginsPtr = std::make_shared<std::vector<SettingsStore::PluginTypeSchema>>(m_settingsStorePtr->ParseAllPlugins());
    m_enabledPluginsPtr = std::make_shared<std::vector<SettingsStore::PluginTypeSchema>>(m_settingsStorePtr->GetEnabledBackends());

    m_settingsStorePtr->InitializeSettingsStore();
    m_ipcPort = IPCMain::OpenConnection();

    Logger.Log("Ports: {{ FTP: {}, IPC: {} }}", m_ftpPort, m_ipcPort);
    this->PrintActivePlugins();
}

PluginLoader::PluginLoader(std::chrono::system_clock::time_point startTime, uint16_t ftpPort) 
    : m_startTime(startTime), m_pluginsPtr(nullptr), m_enabledPluginsPtr(nullptr), m_ftpPort(ftpPort)
{
    this->Initialize();
}

const std::thread PluginLoader::ConnectCEFBrowser(void* cefBrowserHandler, SocketHelpers* socketHelpers)
{
    SocketHelpers::ConnectSocketProps browserProps;

    browserProps.commonName     = "CEFBrowser";
    browserProps.fetchSocketUrl = std::bind(&SocketHelpers::GetSteamBrowserContext, socketHelpers);
    browserProps.onConnect      = std::bind(&CEFBrowser::onConnect, (CEFBrowser*)cefBrowserHandler, _1, _2);
    browserProps.onMessage      = std::bind(&CEFBrowser::onMessage, (CEFBrowser*)cefBrowserHandler, _1, _2, _3);
    browserProps.bAutoReconnect = false;

    return std::thread(std::bind(&SocketHelpers::ConnectSocket, socketHelpers, browserProps));
}

const void PluginLoader::InjectWebkitShims() 
{
    static std::vector<int> hookIds;

    if (!hookIds.empty())
    {
        const auto moduleList = WebkitHandler::get().m_hookListPtr;

        for (auto it = moduleList->begin(); it != moduleList->end();)
        {
            if (std::find(hookIds.begin(), hookIds.end(), it->id) != hookIds.end())
            {
                Logger.Log("Removing hook for module id: {}", it->id);
                it = moduleList->erase(it);
            }
            else ++it;
        }
    }

    const auto allPlugins = this->m_settingsStorePtr->ParseAllPlugins();
    std::vector<SettingsStore::PluginTypeSchema> enabledBackends;

    for (auto& plugin : allPlugins)
    {
        const auto absolutePath = SystemIO::GetSteamPath() / "plugins" / plugin.webkitAbsolutePath;

        if (this->m_settingsStorePtr->IsEnabledPlugin(plugin.pluginName) && std::filesystem::exists(absolutePath))
        {
            g_hookedModuleId++;
            hookIds.push_back(g_hookedModuleId);

            Logger.Log("Injecting hook for '{}' with id {}", plugin.pluginName, g_hookedModuleId);
            WebkitHandler::get().m_hookListPtr->push_back({ absolutePath.generic_string(), std::regex(".*"), WebkitHandler::TagTypes::JAVASCRIPT, g_hookedModuleId });
        }
    }
}

const void PluginLoader::StartFrontEnds()
{
    CEFBrowser cefBrowserHandler(m_ftpPort, m_ipcPort);
    SocketHelpers socketHelpers;

    this->InjectWebkitShims();

    auto socketStart = std::chrono::high_resolution_clock::now();
    Logger.Log("Starting frontend socket...");
    std::thread browserSocketThread = this->ConnectCEFBrowser(&cefBrowserHandler, &socketHelpers);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - this->m_startTime);
    Logger.Log("Startup took {} ms", duration.count());

    browserSocketThread.join();

    if (g_threadTerminateFlag->flag.load())
    {
        Logger.Log("Terminating frontend thread pool...");
        return;
    }

    Logger.Warn("Unexpectedly Disconnected from Steam, attempting to reconnect...");
    
    this->m_startTime = std::chrono::system_clock::now();
    this->StartFrontEnds();
}

/* debug function, just for developers */
const void PluginLoader::PrintActivePlugins()
{
    std::string pluginList = "Plugins: { ";
    for (auto it = (*this->m_pluginsPtr).begin(); it != (*this->m_pluginsPtr).end(); ++it)
    {
        const auto pluginName = (*it).pluginName;
        pluginList.append(fmt::format("{}: {}{}", pluginName, m_settingsStorePtr->IsEnabledPlugin(pluginName) ? "Enabled" : "Disabled", std::next(it) == (*this->m_pluginsPtr).end() ? " }" : ", "));
    }

    Logger.Log(pluginList);
}

const void StartPreloader(PythonManager& manager)
{
    std::promise<void> promise;

    SettingsStore::PluginTypeSchema plugin = 
    {
        .pluginName = "pipx",
        .backendAbsoluteDirectory = SystemIO::GetInstallPath() / "ext" / "data" / "assets" / "pipx",
        .isInternal = true
    };

    manager.CreatePythonInstance(plugin, [&promise](SettingsStore::PluginTypeSchema plugin) 
    {
        Logger.Log("Started preloader module");
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

        Logger.Log("Preloader finished...");

        promise.set_value();
    });

    promise.get_future().get();
    manager.DestroyPythonInstance("pipx");
}

const void PluginLoader::StartBackEnds(PythonManager& manager)
{
    Logger.Log("Starting plugin backends...");
    StartPreloader(manager);
    Logger.Log("Starting backends...");

    this->Initialize();
    this->PrintActivePlugins();

    for (auto& plugin : *this->m_enabledPluginsPtr)
    {
        // check if plugin is already running
        if (manager.IsRunning(plugin.pluginName))
        {
            Logger.Log("Skipping load for '{}' as it's already running", plugin.pluginName);
            continue;
        }

        std::function<void(SettingsStore::PluginTypeSchema)> cb = std::bind(CoInitializer::BackendStartCallback, std::placeholders::_1);

        std::thread(
            [&manager, &plugin, cb]() {
                Logger.Log("Starting backend for '{}'", plugin.pluginName);
                manager.CreatePythonInstance(plugin, cb);
            }
        ).detach();
    }
}
