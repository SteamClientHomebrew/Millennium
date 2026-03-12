/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

#include "millennium/backend_mgr.h"
#include "millennium/filesystem.h"
#include "millennium/life_cycle.h"
#include "millennium/logger.h"
#include "millennium/plugin_ipc.h"
#include "state/shared_memory.h"

#include <fmt/core.h>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

extern std::condition_variable cv_hasSteamUnloaded;
extern std::mutex mtx_hasSteamUnloaded;

/**
 * path to the lua sandbox executable.
 * set at compile time via CMake generator expression.
 */
#ifndef __LUA_HOST_OUTPUT_ABSPATH__
#define __LUA_HOST_OUTPUT_ABSPATH__ "millennium_luasb86"
#endif

static std::string get_lua_host_exe()
{
    return __LUA_HOST_OUTPUT_ABSPATH__;
}

static std::string get_socket_path(const std::string& pluginName)
{
#ifdef _WIN32
    const char* tmp = std::getenv("TEMP");
    if (!tmp) tmp = "C:\\Windows\\Temp";
    return fmt::format("{}\\millennium-plugin-{}-{}.sock", tmp, GetCurrentProcessId(), pluginName);
#else
    return fmt::format("/tmp/millennium-plugin-{}-{}.sock", getpid(), pluginName);
#endif
}

backend_manager::backend_manager(std::shared_ptr<plugin_manager> plugin_manager, std::shared_ptr<backend_event_dispatcher> event_dispatcher)
    : m_plugin_manager(std::move(plugin_manager)), m_backend_event_dispatcher(std::move(event_dispatcher))
{
}

backend_manager::~backend_manager()
{
    this->shutdown();
}

void backend_manager::shutdown()
{
    if (m_has_shutdown.exchange(true)) return;

    std::lock_guard<std::mutex> lock(m_processes_mutex);
    logger.warn("Unloading {} plugin(s) and preparing for exit...", m_processes.size());

    const auto startTime = std::chrono::steady_clock::now();

    /* Send SHUTDOWN RPCs to all plugins in parallel so the total grace period
       is ~5 s regardless of plugin count, not 5 s × N sequentially. */
    std::vector<std::thread> shutdown_threads;
    shutdown_threads.reserve(m_processes.size());

    for (auto& kv : m_processes) {
        auto& name = kv.first;
        auto& process = kv.second;
        shutdown_threads.emplace_back([&name, &process]()
        {
            logger.log("Shutting down plugin '{}'...", name);
            process->shutdown();
        });
    }

    for (auto& t : shutdown_threads) {
        if (t.joinable()) t.join();
    }

    m_processes.clear();

    logger.log("Finished shutdown! Bye bye!");
    logger.log("Shutdown took {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count());
}

bool backend_manager::has_any_backends()
{
    std::lock_guard<std::mutex> lock(m_processes_mutex);
    return !m_processes.empty();
}

bool backend_manager::has_all_backends_stopped()
{
    std::lock_guard<std::mutex> lock(m_processes_mutex);
    for (auto& [name, process] : m_processes) {
        if (process->is_alive()) return false;
    }
    return true;
}

bool backend_manager::is_any_backend_running(std::string plugin_name)
{
    std::lock_guard<std::mutex> lock(m_processes_mutex);
    auto it = m_processes.find(plugin_name);
    return it != m_processes.end() && it->second->is_alive();
}

bool backend_manager::spawn_plugin(plugin_manager::plugin_t& plugin)
{
    logger.log("Spawning child process for plugin '{}'", plugin.plugin_name);

    const std::string exe_path = get_lua_host_exe();
    const std::string socket_path = get_socket_path(plugin.plugin_name);

    /* Pre-determine the crash dump directory so both parent and child know
       the exact path — no filesystem scanning needed after a crash. */
    const auto crash_dump_dir = platform::get_crash_dump_dir(plugin.plugin_name);

    nlohmann::json init_params = {
        { "plugin_name",    plugin.plugin_name                               },
        { "backend_dir",    plugin.plugin_backend_dir.parent_path().string() },
        { "backend_file",   plugin.plugin_backend_dir.string()               },
        { "steam_path",     platform::get_steam_path().string()              },
        { "crash_dump_dir", crash_dump_dir                                   },
#ifdef _WIN32
        { "steam_pid",      static_cast<uint64_t>(GetCurrentProcessId())     },
#endif
    };

    auto process = spawn_plugin_process(plugin.plugin_name, exe_path, socket_path, init_params, m_child_request_handler);
    if (!process) {
        LOG_ERROR("Failed to spawn child process for plugin '{}'", plugin.plugin_name);
        m_backend_event_dispatcher->backend_loaded_event_hdlr({ plugin.plugin_name, backend_event_dispatcher::backend_ready_event::BACKEND_LOAD_FAILED });
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_processes_mutex);
        m_processes[plugin.plugin_name] = std::move(process);
    }

    return true;
}

bool backend_manager::destroy_plugin(const std::string& pluginName, bool isShuttingDown)
{
    std::unique_ptr<PluginProcess> process;

    {
        std::lock_guard<std::mutex> lock(m_processes_mutex);
        auto it = m_processes.find(pluginName);
        if (it == m_processes.end()) return false;

        process = std::move(it->second);
        m_processes.erase(it);
    }

    logger.log("Stopping plugin '{}'...", pluginName);
    process->shutdown();

    /* remove patches from shared memory */
    if (g_lb_patch_arena) {
        hashmap_remove(g_lb_patch_arena, pluginName.c_str());
    }

    process.reset();

    m_backend_event_dispatcher->backend_unloaded_event_hdlr({ pluginName }, isShuttingDown);

    {
        std::unique_lock<std::mutex> lk(mtx_hasSteamUnloaded);
        cv_hasSteamUnloaded.notify_all();
    }

    return true;
}

bool backend_manager::destroy_all_plugins(bool isShuttingDown)
{
    std::vector<std::string> names;

    {
        std::lock_guard<std::mutex> lock(m_processes_mutex);
        for (auto& [name, _] : m_processes) {
            names.push_back(name);
        }
    }

    for (auto& name : names) {
        destroy_plugin(name, isShuttingDown);
    }

    return true;
}

nlohmann::json backend_manager::evaluate(const std::string& pluginName, const nlohmann::json& script)
{
    std::lock_guard<std::mutex> lock(m_processes_mutex);
    auto it = m_processes.find(pluginName);
    if (it == m_processes.end()) {
        return {
            { "success", false                               },
            { "error",   "plugin not running: " + pluginName }
        };
    }

    try {
        return it->second->call(plugin_ipc::parent_method::EVALUATE, script);
    } catch (const std::exception& e) {
        return {
            { "success", false                 },
            { "error",   std::string(e.what()) }
        };
    }
}

void backend_manager::notify_frontend_loaded(const std::string& pluginName)
{
    std::lock_guard<std::mutex> lock(m_processes_mutex);
    auto it = m_processes.find(pluginName);
    if (it == m_processes.end()) return;

    it->second->notify_child(plugin_ipc::parent_method::ON_FRONTEND_LOADED);
}

void backend_manager::set_child_request_handler(PluginProcess::request_handler handler)
{
    m_child_request_handler = std::move(handler);

    /* apply to all existing processes */
    std::lock_guard<std::mutex> lock(m_processes_mutex);
    for (auto& [name, process] : m_processes) {
        process->set_request_handler(m_child_request_handler);
    }
}

PluginProcess::process_metrics backend_manager::get_plugin_metrics(const std::string& pluginName)
{
    std::lock_guard<std::mutex> lock(m_processes_mutex);
    auto it = m_processes.find(pluginName);
    if (it == m_processes.end()) return {};
    return it->second->get_metrics();
}

std::unordered_map<std::string, PluginProcess::process_metrics> backend_manager::get_all_plugin_metrics()
{
    std::lock_guard<std::mutex> lock(m_processes_mutex);
    std::unordered_map<std::string, PluginProcess::process_metrics> result;
    for (auto& [name, process] : m_processes) {
        result[name] = process->get_metrics();
    }
    return result;
}
