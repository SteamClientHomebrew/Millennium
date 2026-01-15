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

#include "millennium/core_ipc.h"
#include "head/entry_point.h"
#include "lua.h"
#include "millennium/life_cycle.h"
#include "millennium/millennium.h"
#include "millennium/millennium_updater.h"
#include "millennium/sysfs.h"
#include "millennium/types.h"
#include "millennium/plugin_loader.h"
#include "millennium/urlp.h"
#include "millennium/backend_init.h"
#include "millennium/backend_mgr.h"
#include "millennium/env.h"
#include "millennium/http_hooks.h"
#include "millennium/logger.h"
#include "millennium/plugin_logger.h"
#include "millennium/plugin_webkit_store.h"
#include "millennium/auth.h"
#include "nlohmann/json_fwd.hpp"
#include <memory>
#include <mutex>

using namespace std::placeholders;
using namespace std::chrono;

std::shared_ptr<InterpreterMutex> g_shouldTerminateMillennium = std::make_shared<InterpreterMutex>();

extern std::mutex mtx_hasAllPythonPluginsShutdown, mtx_hasSteamUnloaded, mtx_hasSteamUIStartedLoading;
extern std::condition_variable cv_hasSteamUnloaded, cv_hasAllPythonPluginsShutdown, cv_hasSteamUIStartedLoading;
extern std::atomic<bool> ab_shouldDisconnectFrontend;

plugin_loader::plugin_loader(std::shared_ptr<SettingsStore> settings_store, std::shared_ptr<millennium_updater> millennium_updater)
    : m_thread_pool(std::make_unique<thread_pool>(2)), m_settings_store_ptr(std::move(settings_store)), m_plugin_ptr(nullptr), m_enabledPluginsPtr(nullptr),
      m_millennium_updater(std::move(millennium_updater)), has_loaded_core_plugin(false)
{
    this->init();
}

plugin_loader::~plugin_loader()
{
    this->shutdown();
}

void plugin_loader::shutdown()
{
    Logger.Log("Successfully shut down plugin_loader...");
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
    m_ipc_main = std::make_shared<ipc_main>(m_settings_store_ptr, m_cdp, m_backend_manager);
    m_network_hook_ctl = std::make_shared<network_hook_ctl>(m_settings_store_ptr, m_cdp);
    m_millennium_backend = std::make_shared<millennium_backend>(m_ipc_main, m_settings_store_ptr, m_network_hook_ctl, m_millennium_updater);

    m_millennium_backend->init(); /** manual ctor, shared_from_this requires a ref holder before its valid */
    m_ipc_main->set_millennium_backend(m_millennium_backend);
    m_millennium_updater->set_ipc_main(m_ipc_main);

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
    });
}

void plugin_loader::init()
{
    m_backend_event_dispatcher = std::make_shared<backend_event_dispatcher>(m_settings_store_ptr);
    m_backend_manager = std::make_shared<backend_manager>(m_settings_store_ptr, m_backend_event_dispatcher);
    m_backend_initializer = std::make_shared<backend_initializer>(m_settings_store_ptr, m_backend_manager, m_backend_event_dispatcher);
    m_plugin_webkit_store = std::make_shared<plugin_webkit_store>(m_settings_store_ptr);
    m_plugin_ptr = std::make_shared<std::vector<SettingsStore::plugin_t>>(m_settings_store_ptr->ParseAllPlugins());
    m_enabledPluginsPtr = std::make_shared<std::vector<SettingsStore::plugin_t>>(m_settings_store_ptr->GetEnabledBackends());

    m_settings_store_ptr->InitializeSettingsStore();

    /** setup steam hooks once backends have loaded */
    m_backend_event_dispatcher->on_all_backends_ready(Plat_WaitForBackendLoad);
}

std::shared_ptr<std::thread> plugin_loader::connect_steam_socket(std::shared_ptr<SocketHelpers> socketHelpers)
{
    std::shared_ptr<SocketHelpers::ConnectSocketProps> browserProps = std::make_shared<SocketHelpers::ConnectSocketProps>();

    browserProps->commonName = "CEFBrowser";
    browserProps->fetchSocketUrl = std::bind(&SocketHelpers::GetSteamBrowserContext, socketHelpers);
    browserProps->onConnect = [this](std::shared_ptr<cdp_client> cdp) { this->devtools_connection_hdlr(std::move(cdp)); };
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
    std::vector<SettingsStore::plugin_t> plugins = m_settings_store_ptr->ParseAllPlugins();

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

    g_millennium->get_plugin_loader()->get_backend_event_dispatcher()->on_all_backends_ready([&, this]()
    {
        Logger.Log("Notifying frontend of backend load...");
        m_backend_initializer->compat_restore_shared_js_context();

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

        m_cdp->send("Runtime.enable").get();

        /** add the initial binding for the shared js context. the rest is handled by the ffi_binder */
        const json add_binding_params = {
            { "name", ffi_constants::binding_name }
        };
        m_cdp->send("Runtime.addBinding", add_binding_params).get();
        Logger.Log("Frontend notifier finished!");
    });
}

void plugin_loader::start_plugin_frontends()
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
    this->start_plugin_frontends();
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

void plugin_loader::start_plugin_backends()
{
    m_backend_initializer->start_package_manager();
    this->log_enabled_plugins();

    auto weak_init = std::weak_ptr(m_backend_initializer);

    for (auto& plugin : *m_enabledPluginsPtr) {
        switch (plugin.backendType) {
            case SettingsStore::PluginBackendType::Python:
                if (m_backend_manager->is_python_backend_running(plugin.pluginName)) break;

                m_backend_manager->create_python_vm(plugin, [weak_init](auto plugin)
                {
                    if (auto init = weak_init.lock()) init->python_backend_started_cb(plugin);
                });
                break;
            case SettingsStore::PluginBackendType::Lua:
                if (m_backend_manager->is_lua_backend_running(plugin.pluginName)) break;

                m_backend_manager->create_lua_vm(plugin, [weak_init](auto plugin, lua_State* L)
                {
                    if (auto init = weak_init.lock()) init->lua_backend_started_cb(plugin, L);
                });
                break;
        }
    }
}

std::shared_ptr<ipc_main> plugin_loader::get_ipc_main()
{
    return m_ipc_main;
}

std::shared_ptr<backend_manager> plugin_loader::get_backend_manager()
{
    return m_backend_manager;
}

std::shared_ptr<backend_event_dispatcher> plugin_loader::get_backend_event_dispatcher()
{
    return m_backend_event_dispatcher;
}

std::shared_ptr<millennium_backend> plugin_loader::get_millennium_backend()
{
    return this->m_millennium_backend;
}
