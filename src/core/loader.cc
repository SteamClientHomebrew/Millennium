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
#include "py_hooks/logger.h"

using namespace std::placeholders;
using namespace std::chrono;
websocketpp::client<websocketpp::config::asio_client>*browserClient;
websocketpp::connection_hdl browserHandle;

std::string sharedJsContextSessionId;
std::shared_ptr<InterpreterMutex> g_threadTerminateFlag = std::make_shared<InterpreterMutex>();

/**
 * @brief Post a message to the SharedJSContext window.
 * @param data The data to post.
 * 
 * @note ID's are managed by the caller. 
 */
bool Sockets::PostShared(nlohmann::json data) 
{
    if (sharedJsContextSessionId.empty()) 
    {
        return false;
    }

    data["sessionId"] = sharedJsContextSessionId;
    return Sockets::PostGlobal(data);
}

/**
 * @brief Post a message to the entire browser.
 * @param data The data to post.
 * 
 * @note ID's are managed by the caller.
 */
bool Sockets::PostGlobal(nlohmann::json data) 
{
    if (browserClient == nullptr) 
    {
        return false;
    }

    browserClient->send(browserHandle, data.dump(), websocketpp::frame::opcode::text);
    return true;
}

/**
 * @brief Shutdown the browser connection.
 * 
 */
void Sockets::Shutdown()
{
    try
    {
        if (browserClient != nullptr) 
        {
            browserClient->close(browserHandle, websocketpp::close::status::normal, "Shutting down");
            Logger.Log("Shut down browser connection...");
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
            if (
                json.contains("params") && json["params"].is_object() &&
                json["params"].contains("message") && json["params"]["message"].is_object() &&
                json["params"]["message"].contains("level") && json["params"]["message"]["level"].is_string()
            )
            {
                if (json["params"]["message"]["level"] == "error")
                {
                    const auto data = json["params"]["message"];

                    std::string traceMessage = fmt::format("{}:{}:{}", data["url"].get<std::string>(), data["line"].get<int>(), data["column"].get<int>());
                    Logger.Warn("(Steam-Error) {}\n  Info: (This warning may be non-fatal)\n  Where: ({})", data["text"].get<std::string>(), traceMessage);
                }
            }
        }
        catch (const nlohmann::detail::exception& e) { }
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
            Sockets::PostShared({ {"id", 9494 }, {"method", "Log.enable"}, {"sessionId", sharedJsContextSessionId} });

            this->UnPatchSharedJSContext();
        }
        else if (json.value("id", -1) == 9773) 
        {
            this->onSharedJsConnect();
        }
        else if (method == "Log.entryAdded") 
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

    const void UnPatchSharedJSContext()
    {
        Logger.Log("Restoring SharedJSContext...");

        const auto SteamUIModulePath = SystemIO::GetSteamPath() / "steamui" / "index.html";
        const auto SteamUIModulePathBackup = SystemIO::GetSteamPath() / "steamui" / "orig.html";

        try
        {
            if (std::filesystem::exists(SteamUIModulePathBackup) && std::filesystem::is_regular_file(SteamUIModulePathBackup))
            {
                std::filesystem::remove(SteamUIModulePath);
            }

            std::filesystem::rename(SteamUIModulePathBackup, SteamUIModulePath);
        }
        catch (const std::exception& e)
        {
            Logger.Warn("Failed to restore SharedJSContext: {}", e.what());
        }

        Logger.Log("Restored SharedJSContext...");
        Sockets::PostShared({ { "id", 9773 }, { "method", "Page.reload" } });
    }

    const void onSharedJsConnect()
    {
        std::thread([this]() {
            Logger.Log("Connected to SharedJSContext in {} ms", duration_cast<milliseconds>(system_clock::now() - m_startTime).count());
            CoInitializer::InjectFrontendShims(m_ftpPort, m_ipcPort);
        }).detach();
    }

    const void onConnect(websocketpp::client<websocketpp::config::asio_client>* client, websocketpp::connection_hdl handle)
    {
        m_startTime   = std::chrono::system_clock::now();
        browserClient = client; 
        browserHandle = handle;

        Logger.Log("Connected to Steam @ {}", (void*)client);

        this->SetupSharedJSContext();
        webKitHandler.SetupGlobalHooks();
    }

    CEFBrowser(uint16_t ftpPort, uint16_t ipcPort) : m_ftpPort(ftpPort), m_ipcPort(ipcPort), webKitHandler(WebkitHandler::get()) 
    {
        webKitHandler.SetIPCPort(ipcPort);
        webKitHandler.SetFTPPort(ftpPort);
    }
};

const void PluginLoader::Initialize()
{
    m_settingsStorePtr  = std::make_unique<SettingsStore>();
    m_pluginsPtr        = std::make_shared<std::vector<SettingsStore::PluginTypeSchema>>(m_settingsStorePtr->ParseAllPlugins());
    m_enabledPluginsPtr = std::make_shared<std::vector<SettingsStore::PluginTypeSchema>>(m_settingsStorePtr->GetEnabledBackends());

    m_settingsStorePtr->InitializeSettingsStore();

    static bool hasCreatedIPC = false;

    if (!hasCreatedIPC)
    {
        m_ipcPort = IPCMain::OpenConnection();
        Logger.Log("Ports: {{ FTP: {}, IPC: {} }}", m_ftpPort, m_ipcPort);

        hasCreatedIPC = true;
    }
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

/**
 * @brief Injects webkit shims into the SteamUI.    
 * All hooks are internally stored in the function and are removed upon re-injection. 
 */
const void PluginLoader::InjectWebkitShims() 
{
    Logger.Warn("Injecting webkit shims...");
    
    this->Initialize();
    static std::vector<int> hookIds;

    /** Clear all previous hooks if there are any */
    if (!hookIds.empty())
    {
        auto moduleList = WebkitHandler::get().m_hookListPtr;

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

    // Inject all webkit shims for enabled plugins if they have shims
    for (auto& plugin : allPlugins)
    {
        const auto absolutePath = SystemIO::GetInstallPath() / "plugins" / plugin.webkitAbsolutePath;

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

/**
 * @brief Start the package manager preload module.
 * 
 * The preloader module is responsible for python package management.
 * All packages are grouped and shared when needed, to prevent wasting space.
 * @see assets\pipx\main.py
 */
const void StartPreloader(PythonManager& manager)
{
    std::promise<void> promise;

    SettingsStore::PluginTypeSchema plugin
    {
        .pluginName = "pipx",
        .backendAbsoluteDirectory = SystemIO::GetInstallPath() / "ext" / "data" / "assets" / "pipx",
        .isInternal = true
    };

    /** Create instance on a separate thread to prevent IO blocking of concurrent threads */
    manager.CreatePythonInstance(plugin, [&promise](SettingsStore::PluginTypeSchema plugin) 
    {
        Logger.Log("Started preloader module");
        const auto backendMainModule = (plugin.backendAbsoluteDirectory / "main.py").generic_string();

        PyObject* globalDictionary = PyModule_GetDict(PyImport_AddModule("__main__"));
        /** Set plugin name in the global dictionary so its stdout can be retrieved by the logger. */
        SetPluginSecretName(globalDictionary, plugin.pluginName);

        PyObject *mainModuleObj = Py_BuildValue("s", backendMainModule.c_str());
        FILE *mainModuleFilePtr = _Py_fopen_obj(mainModuleObj, "r+");

        if (mainModuleFilePtr == NULL) 
        {
            LOG_ERROR("failed to fopen file @ {}", backendMainModule);
            ErrorToLogger(plugin.pluginName, fmt::format("Failed to open file @ {}", backendMainModule));
            return;
        }

        if (PyRun_SimpleFile(mainModuleFilePtr, backendMainModule.c_str()) != 0) 
        {
            LOG_ERROR("millennium failed to preload plugins", plugin.pluginName);
            ErrorToLogger(plugin.pluginName, "Failed to preload plugins");
            return;
        }

        Logger.Log("Preloader finished...");
        promise.set_value();
    });

    /* Wait for the package manager plugin to exit, signalling we can now start other plugins */
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

        m_threadPool.push_back(std::thread(
            [&manager, &plugin, cb]() {
                Logger.Log("Starting backend for '{}'", plugin.pluginName);
                manager.CreatePythonInstance(plugin, cb);
            }
        ));
    }
}