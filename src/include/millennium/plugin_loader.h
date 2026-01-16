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

#pragma once

#include "millennium/backend_init.h"
#include "millennium/http_hooks.h"
#include "millennium/core_ipc.h"
#include "millennium/ffi_binder.h"
#include "millennium/cdp_api.h"
#include "millennium/backend_mgr.h"
#include "millennium/cef_bridge.h"
#include "millennium/http_hooks.h"
#include "millennium/life_cycle.h"
#include "millennium/millennium_updater.h"
#include "millennium/sysfs.h"
#include "millennium/plugin_webkit_world_mgr.h"
#include "millennium/plugin_webkit_store.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#ifdef _WIN32
#undef _WINSOCKAPI_
#include <winsock2.h>
#endif

extern std::shared_ptr<InterpreterMutex> g_shouldTerminateMillennium;

class plugin_loader : std::enable_shared_from_this<plugin_loader>
{
  public:
    using plugin_state = std::vector<std::pair<std::string, bool>>;

    plugin_loader(std::shared_ptr<SettingsStore> settings_store, std::shared_ptr<millennium_updater> millennium_updater);
    ~plugin_loader();

    void start_plugin_backends();
    void start_plugin_frontends();
    void setup_webkit_shims();

    void set_plugin_enable(std::string plugin_name, bool enabled);
    void set_plugins_enabled(const plugin_state& plugins);

    void inject_frontend_shims(bool reload_frontend);

    std::shared_ptr<ipc_main> get_ipc_main();
    std::shared_ptr<backend_manager> get_backend_manager();
    std::shared_ptr<backend_event_dispatcher> get_backend_event_dispatcher();
    std::shared_ptr<millennium_backend> get_millennium_backend();

  private:
    void init();
    void shutdown();

    void init_devtools();
    void devtools_connection_hdlr(std::shared_ptr<cdp_client> cdp);

    void log_enabled_plugins();
    std::shared_ptr<std::thread> connect_steam_socket(std::shared_ptr<SocketHelpers> socketHelpers);

    std::string cdp_generate_bootstrap_module(const std::vector<std::string>& modules);
    std::string cdp_generate_shim_module();

    std::unique_ptr<thread_pool> m_thread_pool;
    std::shared_ptr<SettingsStore> m_settings_store_ptr;
    std::shared_ptr<std::vector<SettingsStore::plugin_t>> m_plugin_ptr, m_enabledPluginsPtr;

    std::shared_ptr<millennium_backend> m_millennium_backend;
    std::shared_ptr<millennium_updater> m_millennium_updater;
    std::shared_ptr<backend_event_dispatcher> m_backend_event_dispatcher;
    std::shared_ptr<backend_manager> m_backend_manager;
    std::shared_ptr<backend_initializer> m_backend_initializer;
    std::shared_ptr<ipc_main> m_ipc_main;
    std::shared_ptr<network_hook_ctl> m_network_hook_ctl;
    std::shared_ptr<plugin_webkit_store> m_plugin_webkit_store;
    std::shared_ptr<webkit_world_mgr> world_mgr;
    std::shared_ptr<ffi_binder> m_ffi_binder;
    std::shared_ptr<cdp_client> m_cdp;

    std::chrono::system_clock::time_point m_socket_con_time;
    std::string document_script_id;
    bool has_loaded_core_plugin;
};
