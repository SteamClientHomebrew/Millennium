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

#include "head/entry_point.h"
#include "millennium/types.h"
#include "millennium/init.h"
#include "millennium/urlp.h"
#include "millennium/backend_init.h"
#include "millennium/backend_mgr.h"
#include "millennium/env.h"
#include "millennium/ffi.h"
#include "millennium/http_hooks.h"
#include "millennium/logger.h"
#include "millennium/plugin_logger.h"
#include "millennium/plugin_webkit_store.h"
#include "millennium/auth.h"
#include "nlohmann/json_fwd.hpp"
#include <memory>

using namespace std::placeholders;
using namespace std::chrono;

std::shared_ptr<plugin_loader> g_plugin_loader;
std::shared_ptr<InterpreterMutex> g_shouldTerminateMillennium = std::make_shared<InterpreterMutex>();

extern std::atomic<bool> ab_shouldDisconnectFrontend;

plugin_loader::plugin_loader() : m_plugin_ptr(nullptr), m_enabledPluginsPtr(nullptr), m_thread_pool(std::make_unique<thread_pool>(2))
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
        auto targets = m_cdp->send_host("Target.getTargets").get();

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

        auto result = m_cdp->send_host("Target.attachToTarget", attach_params).get();
        /** set the session ID of the SharedJSContext */
        m_cdp->set_shared_js_session_id(result.at("sessionId").get<std::string>());

        const json expose_devtools_params = {
            { "targetId",    targetId                                                                },
            { "bindingName", "MILLENNIUM_CHROME_DEV_TOOLS_PROTOCOL_DO_NOT_USE_OR_OVERRIDE_ONMESSAGE" }
        };

        m_cdp->send_host("Target.exposeDevToolsProtocol", expose_devtools_params);
        break;
    }

    m_thread_pool->enqueue([this]()
    {
        Logger.Log("Connected to SharedJSContext in {} ms", duration_cast<milliseconds>(system_clock::now() - m_socket_con_time).count());
        this->inject_frontend_shims(true /** reload SharedJSContext */);
    });
}

void plugin_loader::devtools_connection_hdlr(std::shared_ptr<cdp_client> cdp)
{
    m_cdp = cdp;
    m_socket_con_time = std::chrono::system_clock::now();

    m_thread_pool->enqueue([this]()
    {
        Logger.Log("Starting webkit world manager...");

        this->m_ffi_binder = std::make_unique<ffi_binder>(m_cdp, m_settings_store_ptr, m_ipc_main);
        this->world_mgr = std::make_unique<webkit_world_mgr>(m_cdp, m_settings_store_ptr, m_network_hook_ctl, m_plugin_webkit_store);
    });

    m_thread_pool->enqueue([this]()
    {
        Logger.Log("Connected to Steam devtools protocol...");

        this->init_devtools();
        m_network_hook_ctl = std::make_shared<network_hook_ctl>(m_settings_store_ptr, m_cdp, m_ipc_main);
        this->m_network_hook_ctl->init();
    });
}

void plugin_loader::init()
{
    m_settings_store_ptr = std::make_shared<SettingsStore>();
    m_ipc_main = std::make_shared<ipc_main>(m_settings_store_ptr);
    m_plugin_webkit_store = std::make_shared<plugin_webkit_store>(m_settings_store_ptr);
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

void plugin_loader::setup_webkit_shims()
{
    Logger.Log("Injecting webkit shims...");
    m_plugin_webkit_store->clear();

    const auto plugins = this->m_settings_store_ptr->ParseAllPlugins();

    for (auto& plugin : plugins) {
        const auto abs_path = std::filesystem::path(GetEnv("MILLENNIUM__PLUGINS_PATH")) / plugin.webkitAbsolutePath;
        const auto should_isolate = plugin.pluginJson.value("webkitApiVersion", "1.0.0") == "2.0.0";

        if (this->m_settings_store_ptr->IsEnabledPlugin(plugin.pluginName) && std::filesystem::exists(abs_path)) {
            m_plugin_webkit_store->add({ plugin.pluginName, abs_path, should_isolate });
        }
    }
}

std::string plugin_loader::cdp_generate_bootstrap_module(const std::vector<std::string>& modules)
{
    std::string str_modules;
    std::string preload_path = SystemIO::GetMillenniumPreloadPath();

    for (size_t i = 0; i < modules.size(); i++) {
        str_modules.append(fmt::format("\"{}\"{}", modules[i], (i == modules.size() - 1 ? "" : ",")));
    }

    const std::string token = GetAuthToken();

    const std::string ftpPath = m_network_hook_ctl->get_ftp_url() + preload_path;
    const std::string script = fmt::format("(new module.default).StartPreloader('{}', [{}]);", token, str_modules);

    return fmt::format("import('{}').then(module => {{ {} }})", ftpPath, script);
}

std::string plugin_loader::cdp_generate_shim_module()
{
    std::vector<std::string> script_list;
    std::vector<SettingsStore::PluginTypeSchema> plugins = m_settings_store_ptr->ParseAllPlugins();

    for (auto& plugin : plugins) {
        if (!m_settings_store_ptr->IsEnabledPlugin(plugin.pluginName)) {
            continue;
        }

        const auto frontEndAbs = plugin.frontendAbsoluteDirectory.generic_string();
        script_list.push_back(UrlFromPath(m_network_hook_ctl->get_ftp_url(), frontEndAbs));
    }

    /** Add the builtin Millennium plugin */
    script_list.push_back(fmt::format("{}{}/millennium-frontend.js", m_network_hook_ctl->get_ftp_url(), GetScrambledApiPathToken()));
    return this->cdp_generate_bootstrap_module(script_list);
}

void plugin_loader::inject_frontend_shims(bool reload_frontend)
{
    this->setup_webkit_shims();

    CoInitializer::BackendCallbacks& backendHandler = CoInitializer::BackendCallbacks::GetInstance();
    backendHandler.RegisterForLoad([&, this]()
    {
        Logger.Log("Notifying frontend of backend load...");
        UnPatchSharedJSContext();

        m_cdp->send("Page.enable").get();

        json params = {
            { "source", this->cdp_generate_shim_module() }
        };
        auto resultId = m_cdp->send("Page.addScriptToEvaluateOnNewDocument", params).get();
        document_script_id = resultId["identifier"].get<std::string>();

        json reload_params = {
            { "ignoreCache", true }
        };

        if (reload_frontend) {
            json result;
            do {
                result = m_cdp->send("Page.reload", reload_params).get();
            }
            /** empty means it was successful */
            while (!result.empty());
        }

        Logger.Log("Frontend notifier finished!");
    });
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

    this->log_enabled_plugins();

    static bool hasCoreLoaded = false;
    if (!hasCoreLoaded) {
        Core_Load(m_settings_store_ptr, m_network_hook_ctl);
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
