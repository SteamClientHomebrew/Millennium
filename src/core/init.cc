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

#include "millennium/types.h"
#include "millennium/init.h"
#include "head/entry_point.h"
#include "millennium/backend_init.h"
#include "millennium/backend_mgr.h"
#include "millennium/env.h"
#include "millennium/ffi.h"
#include "millennium/http_hooks.h"
#include "millennium/logger.h"
#include "millennium/plugin_logger.h"
#include "nlohmann/json_fwd.hpp"
#include <memory>

using namespace std::placeholders;
using namespace std::chrono;

std::shared_ptr<cdp_client> cdp;
std::shared_ptr<plugin_loader> g_plugin_loader;
std::shared_ptr<InterpreterMutex> g_shouldTerminateMillennium = std::make_shared<InterpreterMutex>();

extern std::atomic<bool> ab_shouldDisconnectFrontend;

plugin_loader::plugin_loader()
    : m_plugin_ptr(nullptr), m_enabledPluginsPtr(nullptr), webkit_handler(network_hook_ctl::GetInstance()), m_thread_pool(std::make_unique<thread_pool>(2))
{
    this->init();
}

plugin_loader::~plugin_loader()
{
    this->shutdown();
}

void plugin_loader::shutdown()
{
    m_thread_pool->shutdown();
}

void plugin_loader::init_devtools()
{
    while (true) {
        auto targets = cdp->send_host("Target.getTargets").get();

        if (!targets.contains("targetInfos") || !targets["targetInfos"].is_array() || targets["targetInfos"].empty()) {
            continue;
        }

        auto targetFrame = "SharedJSContext";
        auto it = std::find_if(targets["targetInfos"].begin(), targets["targetInfos"].end(), [&](const auto& target) { return target["title"] == targetFrame; });

        if (it == targets["targetInfos"].end()) {
            continue;
        }

        auto targetId = it->at("targetId");

        const json attach_params = {
            { "targetId", targetId },
            { "flatten",  true     }
        };

        auto result = cdp->send_host("Target.attachToTarget", attach_params).get();
        /** set the session ID of the SharedJSContext */
        cdp->set_shared_js_session_id(result.at("sessionId").get<std::string>());

        const json expose_devtools_params = {
            { "targetId",    targetId                                                                },
            { "bindingName", "MILLENNIUM_CHROME_DEV_TOOLS_PROTOCOL_DO_NOT_USE_OR_OVERRIDE_ONMESSAGE" }
        };

        cdp->send_host("Target.exposeDevToolsProtocol", expose_devtools_params);
        break;
    }

    m_thread_pool->enqueue([this]()
    {
        Logger.Log("Connected to SharedJSContext in {} ms", duration_cast<milliseconds>(system_clock::now() - m_socket_con_time).count());
        CoInitializer::InjectFrontendShims();
    });
}

void plugin_loader::devtools_connection_hdlr(std::shared_ptr<cdp_client> cdp)
{
    ::cdp = cdp;
    m_socket_con_time = std::chrono::system_clock::now();

    m_thread_pool->enqueue([this]()
    {
        Logger.Log("Connected to Steam devtools protocol...");

        this->init_devtools();
        network_hook_ctl::GetInstance().init();
    });
}

void plugin_loader::init()
{
    m_settings_store_ptr = std::make_unique<SettingsStore>();
    m_plugin_ptr = std::make_shared<std::vector<SettingsStore::PluginTypeSchema>>(m_settings_store_ptr->ParseAllPlugins());
    m_enabledPluginsPtr = std::make_shared<std::vector<SettingsStore::PluginTypeSchema>>(m_settings_store_ptr->GetEnabledBackends());

    m_settings_store_ptr->InitializeSettingsStore();
}

std::shared_ptr<std::thread> plugin_loader::connect_steam_socket(std::shared_ptr<SocketHelpers> socketHelpers)
{
    std::shared_ptr<SocketHelpers::ConnectSocketProps> browserProps = std::make_shared<SocketHelpers::ConnectSocketProps>();

    browserProps->commonName = "CEFBrowser";
    browserProps->fetchSocketUrl = std::bind(&SocketHelpers::GetSteamBrowserContext, socketHelpers);
    browserProps->onConnect = std::bind(&plugin_loader::devtools_connection_hdlr, this, _1);
    return std::make_shared<std::thread>(std::thread([ptrSocketHelpers = socketHelpers, browserProps] { ptrSocketHelpers->ConnectSocket(browserProps); }));
}

void plugin_loader::StartFrontEnds()
{
    if (g_shouldTerminateMillennium->flag.load()) {
        Logger.Log("Terminating frontend thread pool...");
        return;
    }

    std::shared_ptr<SocketHelpers> socketHelpers = std::make_shared<SocketHelpers>();

    Logger.Log("Starting frontend socket...");
    std::shared_ptr<std::thread> browserSocketThread = this->connect_steam_socket(socketHelpers);

    if (browserSocketThread->joinable()) {
        Logger.Log("Joining browser socket thread {}", (void*)browserSocketThread.get());
        browserSocketThread->join();
        Logger.Log("Browser socket thread joined {}", (void*)browserSocketThread.get());
    }

    if (g_shouldTerminateMillennium->flag.load()) {
        Logger.Log("Terminating frontend thread pool...");
        return;
    }

    Logger.Warn("Unexpectedly Disconnected from Steam, attempting to reconnect...");

#ifdef _WIN32
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
        if (ab_shouldDisconnectFrontend.load()) {
            Logger.Log("Steam is shutting down, terminating frontend thread pool...");
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#endif

    Logger.Warn("Reconnecting to Steam...");
    this->StartFrontEnds();
}

/* debug function, just for developers */
void plugin_loader::log_enabled_plugins()
{
    std::string pluginList = "Plugins: { ";
    for (auto it = (*this->m_plugin_ptr).begin(); it != (*this->m_plugin_ptr).end(); ++it) {
        const auto pluginName = (*it).pluginName;
        pluginList.append(fmt::format("{}: {}{}", pluginName, m_settings_store_ptr->IsEnabledPlugin(pluginName) ? "Enabled" : "Disabled",
                                      std::next(it) == (*this->m_plugin_ptr).end() ? " }" : ", "));
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
void StartPreloader(BackendManager& manager)
{
    std::promise<void> promise;

    SettingsStore::PluginTypeSchema plugin;
    plugin.pluginName = "pipx";
    plugin.backendAbsoluteDirectory = std::filesystem::path(GetEnv("MILLENNIUM__ASSETS_PATH")) / "pipx";
    plugin.isInternal = true;

    /** Create instance on a separate thread to prevent IO blocking of concurrent
     * threads */
    manager.CreatePythonInstance(plugin, [&promise](SettingsStore::PluginTypeSchema plugin)
    {
        Logger.Log("Started preloader module");
        const auto backendMainModule = (plugin.backendAbsoluteDirectory / "main.py").generic_string();

        PyObject* globalDictionary = PyModule_GetDict(PyImport_AddModule("__main__"));
        /** Set plugin name in the global dictionary so its stdout can be
         * retrieved by the logger. */
        SetPluginSecretName(globalDictionary, plugin.pluginName);

        PyObject* mainModuleObj = Py_BuildValue("s", backendMainModule.c_str());
        FILE* mainModuleFilePtr = _Py_fopen_obj(mainModuleObj, "r");

        if (mainModuleFilePtr == NULL) {
            LOG_ERROR("Failed to fopen file @ {}", backendMainModule);
            ErrorToLogger(plugin.pluginName, fmt::format("Failed to open file @ {}", backendMainModule));
            return;
        }

        try {
            Logger.Log("Starting package manager thread @ {}", backendMainModule);

            if (PyRun_SimpleFile(mainModuleFilePtr, backendMainModule.c_str()) != 0) {
                LOG_ERROR("Failed to run PIPX preload", plugin.pluginName);
                ErrorToLogger(plugin.pluginName, "Failed to preload plugins");
                return;
            }
        } catch (const std::system_error& error) {
            LOG_ERROR("Failed to run PIPX preload due to a system error: {}", error.what());
        }

        Logger.Log("Preloader finished...");
        promise.set_value();
    });

    /* Wait for the package manager plugin to exit, signalling we can now start
     * other plugins */
    promise.get_future().get();
    manager.DestroyPythonInstance("pipx");
}

void plugin_loader::StartBackEnds(BackendManager& manager)
{
    Logger.Log("Starting plugin backends...");
    StartPreloader(manager);
    Logger.Log("Starting backends...");

    this->init();
    this->log_enabled_plugins();

    static bool hasCoreLoaded = false;
    if (!hasCoreLoaded) {
        Core_Load();
        hasCoreLoaded = true;
    }

    for (auto& plugin : *this->m_enabledPluginsPtr) {
        if (plugin.backendType == SettingsStore::PluginBackendType::Python) {
            // check if plugin is already running
            if (manager.IsPythonBackendRunning(plugin.pluginName)) {
                Logger.Log("[Python] Skipping load for '{}' as it's already running", plugin.pluginName);
                continue;
            }

            std::function<void(SettingsStore::PluginTypeSchema)> cb = std::bind(CoInitializer::PyBackendStartCallback, std::placeholders::_1);

            Logger.Log("[Python] Starting backend for '{}'", plugin.pluginName);
            manager.CreatePythonInstance(plugin, cb);
        } else if (plugin.backendType == SettingsStore::PluginBackendType::Lua) {
            /** check if the Lua backend is already running */
            if (manager.IsLuaBackendRunning(plugin.pluginName)) {
                Logger.Log("[Lua] Skipping load for '{}' as it's already running", plugin.pluginName);
                continue;
            }

            std::function<void(SettingsStore::PluginTypeSchema, lua_State*)> cb = std::bind(CoInitializer::LuaBackendStartCallback, std::placeholders::_1, std::placeholders::_2);

            Logger.Log("[Lua] Starting backend for '{}'", plugin.pluginName);
            manager.CreateLuaInstance(plugin, cb);
        }
    }
}
