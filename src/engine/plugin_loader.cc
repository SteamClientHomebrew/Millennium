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
#include "millennium/life_cycle.h"
#include "millennium/millennium_updater.h"
#include "millennium/filesystem.h"
#include "millennium/plugin_manager.h"
#include "millennium/steam_hooks.h"
#include "millennium/types.h"
#include "millennium/plugin_loader.h"
#include "millennium/url_parser.h"
#include "millennium/backend_init.h"
#include "millennium/backend_mgr.h"
#include "millennium/environment.h"
#include "millennium/http_hooks.h"
#include "millennium/logger.h"
#include "millennium/plugin_webkit_store.h"
#include "millennium/auth.h"
#include <memory>
#include <mutex>
#include <fmt/ranges.h>
#include <tuple>
#include <utility>

using namespace std::placeholders;
using namespace std::chrono;

std::shared_ptr<InterpreterMutex> g_shouldTerminateMillennium = std::make_shared<InterpreterMutex>();

extern std::mutex mtx_hasAllPythonPluginsShutdown, mtx_hasSteamUnloaded, mtx_hasSteamUIStartedLoading;
extern std::condition_variable cv_hasSteamUnloaded, cv_hasAllPythonPluginsShutdown, cv_hasSteamUIStartedLoading;
extern std::atomic<bool> ab_shouldDisconnectFrontend;

plugin_loader::plugin_loader(std::shared_ptr<plugin_manager> plugin_manager, std::shared_ptr<millennium_updater> millennium_updater)
    : m_thread_pool(std::make_unique<thread_pool>(2)), m_plugin_manager(std::move(plugin_manager)), m_plugin_ptr(nullptr), m_enabledPluginsPtr(nullptr),
      m_millennium_updater(std::move(millennium_updater)), has_loaded_core_plugin(false)
{
    logger.log("Initializing plugin_loader...");
    this->init();
}

plugin_loader::~plugin_loader()
{
    this->shutdown();
}

void plugin_loader::shutdown()
{
    logger.log("Successfully shut down plugin_loader...");
}

void plugin_loader::devtools_connection_hdlr(std::shared_ptr<cdp_client> cdp)
{
    m_cdp = cdp;
    m_socket_con_time = std::chrono::system_clock::now();

    m_network_hook_ctl->set_cdp_client(m_cdp);
    m_network_hook_ctl->init();

    m_extension_mgr = std::make_shared<browser_extension_manager>(cdp);
    m_ipc_main = std::make_shared<ipc_main>(m_millennium_backend, m_plugin_manager, m_cdp, m_backend_manager);

    m_millennium_updater->set_ipc_main(m_ipc_main);
    m_millennium_backend->set_ipc_main(m_ipc_main);
    m_millennium_backend->set_extension_mgr(m_extension_mgr);

    m_thread_pool->enqueue([this]()
    {
        logger.log("Starting webkit world manager...");

        this->m_ffi_binder = std::make_unique<ffi_binder>(m_cdp, m_plugin_manager, m_ipc_main);
        this->world_mgr = std::make_unique<webkit_world_mgr>(m_cdp, m_plugin_manager, m_network_hook_ctl, m_plugin_webkit_store);
    });

    m_thread_pool->enqueue([this]()
    {
        logger.log("Connected to Steam devtools protocol...");
        this->init_devtools();
    });
}

void plugin_loader::init()
{
    m_network_hook_ctl = std::make_shared<network_hook_ctl>(m_plugin_manager);

    m_millennium_backend = std::make_shared<head::millennium_backend>(m_network_hook_ctl, m_plugin_manager, m_millennium_updater);
    m_millennium_backend->init(); /** manual ctor, shared_from_this requires a ref holder before its valid */

    m_backend_event_dispatcher = std::make_shared<backend_event_dispatcher>(m_plugin_manager);
    m_backend_manager = std::make_shared<backend_manager>(m_plugin_manager, m_backend_event_dispatcher);
    m_backend_initializer = std::make_shared<backend_initializer>(m_plugin_manager, m_backend_manager, m_backend_event_dispatcher);
    m_plugin_webkit_store = std::make_shared<plugin_webkit_store>(m_plugin_manager);
    m_plugin_ptr = std::make_shared<std::vector<plugin_manager::plugin_t>>(m_plugin_manager->get_all_plugins());
    m_enabledPluginsPtr = std::make_shared<std::vector<plugin_manager::plugin_t>>(m_plugin_manager->get_enabled_backends());

    m_plugin_manager->init();

    /** setup steam hooks once backends have loaded */
    m_backend_event_dispatcher->on_all_backends_ready(Plat_WaitForBackendLoad);
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
        logger.log("Connected to SharedJSContext in {} ms", duration_cast<milliseconds>(system_clock::now() - m_socket_con_time).count());
        this->inject_frontend_shims(true /** reload SharedJSContext */);
    });
}

std::shared_ptr<std::thread> plugin_loader::connect_steam_socket(std::shared_ptr<socket_utils> socketHelpers)
{
    std::shared_ptr<socket_utils::socket_t> browserProps = std::make_shared<socket_utils::socket_t>();

    browserProps->name = "CEFBrowser";
    browserProps->fetch_socket_url = std::bind(&socket_utils::get_steam_browser_context, socketHelpers);
    browserProps->on_connect = [this](std::shared_ptr<cdp_client> cdp) { this->devtools_connection_hdlr(std::move(cdp)); };
    return std::make_shared<std::thread>(std::thread([ptrSocketHelpers = socketHelpers, browserProps] { ptrSocketHelpers->connect_socket(browserProps); }));
}

void plugin_loader::setup_webkit_shims()
{
    logger.log("Injecting webkit shims...");
    m_plugin_webkit_store->clear();

    const auto plugins = this->m_plugin_manager->get_all_plugins();

    for (auto& plugin : plugins) {
        const auto abs_path = std::filesystem::path(platform::environment::get("MILLENNIUM__PLUGINS_PATH")) / plugin.plugin_webkit_path;
        const auto should_isolate = plugin.plugin_json.value("webkitApiVersion", "1.0.0") == "2.0.0";

        if (this->m_plugin_manager->is_enabled(plugin.plugin_name) && std::filesystem::exists(abs_path)) {
            m_plugin_webkit_store->add({ plugin.plugin_name, abs_path, should_isolate });
        }
    }
}

std::string plugin_loader::cdp_generate_bootstrap_module(const std::vector<std::string>& modules)
{
    const std::string preload_path = platform::get_millennium_preload_path();
    const std::string token = GetAuthToken();
    const std::string ftp_path = m_network_hook_ctl->get_ftp_url() + preload_path;
    const std::string module_list = fmt::format(R"("{}")", fmt::join(modules, R"(", ")"));

    return fmt::format("import('{}').then(m => (new m.default).StartPreloader('{}', [{}]))", ftp_path, token, module_list);
}

std::string plugin_loader::cdp_generate_shim_module()
{
    std::vector<std::string> script_list;
    std::vector<plugin_manager::plugin_t> plugins = m_plugin_manager->get_all_plugins();

    for (auto& plugin : plugins) {
        if (!m_plugin_manager->is_enabled(plugin.plugin_name)) {
            continue;
        }

        const auto frontEndAbs = plugin.plugin_frontend_dir.generic_string();
        script_list.push_back(utils::url::get_url_from_path(m_network_hook_ctl->get_ftp_url(), frontEndAbs));
    }

    /** Add the builtin Millennium plugin */
    script_list.push_back(fmt::format("{}{}/millennium-frontend.js", m_network_hook_ctl->get_ftp_url(), GetScrambledApiPathToken()));
    return this->cdp_generate_bootstrap_module(script_list);
}

void plugin_loader::inject_frontend_shims(bool reload_frontend)
{
    this->setup_webkit_shims();
    auto self = this->shared_from_this();

    /**
     * add the initial binding for the shared js context.
     * the rest is handled by the ffi_binder.
     */
    const auto insert_ipc = std::make_shared<std::function<void()>>([self]()
    {
        self->m_cdp->send("Runtime.enable").get();

        const json add_binding_params = {
            { "name", ffi_constants::binding_name }
        };
        self->m_cdp->send("Runtime.addBinding", add_binding_params).get();
        logger.log("Frontend notifier finished!");
    });

    /**
     * reload the frontend of Steam if need be.
     * sometimes we need to queue a change instead of instantly applying it.
     */
    const auto reload = std::make_shared<std::function<void()>>([reload_frontend, self]()
    {
        json reload_params = {
            { "ignoreCache", true }
        };

        if (reload_frontend) {
            json result;
            do {
                result = self->m_cdp->send("Page.reload", reload_params).get();
            }
            /** empty means it was successful */
            while (!result.empty());
        }
    });

    /**
     * tell chromium to run Millennium's frontend every time a new document is loaded.
     */
    const auto insert_millennium = std::make_shared<std::function<void()>>([self]()
    {
        self->m_cdp->send("Page.enable").get();

        json params = {
            { "source", self->cdp_generate_shim_module() }
        };
        auto resultId = self->m_cdp->send("Page.addScriptToEvaluateOnNewDocument", params).get();
        self->document_script_id = resultId["identifier"].get<std::string>();
    });

    /**
     * wait for millennium to finish starting the plugin backends
     * we garunetee the backends will be loaded before the plugin, so
     * we need to enforce that here.
     */
    m_backend_event_dispatcher->on_all_backends_ready([insert_millennium, reload, insert_ipc, self]()
    {
        logger.log("Notifying frontend of backend load...");
        self->m_backend_initializer->compat_restore_shared_js_context();

        (*insert_millennium)();
        (*reload)();
        (*insert_ipc)();
    });
}

void plugin_loader::start_plugin_frontends()
{
    if (g_shouldTerminateMillennium->flag.load()) {
        logger.log("Terminating frontend thread pool...");
        return;
    }

    std::shared_ptr<socket_utils> helper = std::make_shared<socket_utils>();

    logger.log("Starting frontend socket...");
    std::shared_ptr<std::thread> socket_thread = this->connect_steam_socket(helper);

    if (socket_thread->joinable()) {
        socket_thread->join();
    }

    if (g_shouldTerminateMillennium->flag.load()) {
        logger.log("Terminating frontend thread pool...");
        return;
    }

    logger.warn("Unexpectedly Disconnected from Steam, attempting to reconnect...");

#ifdef _WIN32
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
        if (ab_shouldDisconnectFrontend.load()) {
            logger.log("Steam is shutting down, terminating frontend thread pool...");
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#endif

    logger.warn("Reconnecting to Steam...");
    this->start_plugin_frontends();
}

/* debug function, just for developers */
void plugin_loader::log_enabled_plugins()
{
    std::vector<std::string> statuses;
    statuses.reserve(m_plugin_ptr->size());

    for (const auto& plugin : *m_plugin_ptr) {
        statuses.push_back(fmt::format("{}: {}", plugin.plugin_name, m_plugin_manager->is_enabled(plugin.plugin_name) ? "enabled" : "disabled"));
    }

    logger.log("Plugins: {{ {} }}", fmt::join(statuses, ", "));
}

void plugin_loader::start_plugin_backends()
{
    std::weak_ptr<plugin_loader> weak_plugin_loader = weak_from_this();
    m_backend_initializer->start_package_manager(weak_plugin_loader);
    this->log_enabled_plugins();

    std::weak_ptr<backend_initializer> weak_init = std::weak_ptr(m_backend_initializer);

    for (auto& plugin : *m_enabledPluginsPtr) {
        if (m_backend_manager->is_any_backend_running(plugin.plugin_name)) {
            continue;
        }

        if (plugin.backend_type == plugin_manager::backend_t::Lua) {
            m_backend_manager->create_lua_vm(plugin, [weak_init, weak_plugin_loader](auto plugin, lua_State* L)
            {
                if (auto init = weak_init.lock()) {
                    init->lua_backend_started_cb(plugin, weak_plugin_loader, L);
                }
            });
        } else {
            m_backend_manager->create_python_vm(plugin, [weak_init, weak_plugin_loader](auto plugin)
            {
                if (auto init = weak_init.lock()) {
                    init->python_backend_started_cb(plugin, weak_plugin_loader);
                }
            });
        }
    }
}
void plugin_loader::set_plugin_enable(std::string plugin_name, bool enabled)
{
    this->set_plugins_enabled({ std::make_pair(plugin_name, enabled) });
}

void plugin_loader::set_plugins_enabled(const std::vector<std::pair<std::string, bool>>& plugins)
{
    bool should_start_backends = false;
    std::vector<std::string> plugins_to_disable;

    for (const auto& [name, enabled] : plugins) {
        m_plugin_manager->set_plugin_enabled(name.c_str(), enabled);
        logger.log("requested to {} plugin [{}]", enabled ? "enable" : "disable", name);

        if (enabled) {
            should_start_backends = true;
        } else {
            plugins_to_disable.push_back(name);
        }
    }

    /** destroy all plugins that need to be disabled */
    for (const auto& name : plugins_to_disable)
        m_backend_manager->destroy_generic_vm(name);

    /** if any new backends were enabled, we need to start them */
    if (should_start_backends) start_plugin_backends();

    inject_frontend_shims(true);
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

std::shared_ptr<head::millennium_backend> plugin_loader::get_millennium_backend()
{
    return this->m_millennium_backend;
}

std::shared_ptr<plugin_manager> plugin_loader::get_plugin_manager()
{
    return this->m_plugin_manager;
}
