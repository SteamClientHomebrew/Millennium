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

#pragma once
#include "millennium/life_cycle.h"
#include "millennium/plugin_manager.h"
#include "millennium/child_process.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>

struct InterpreterMutex
{
    std::mutex mtx, runMutex;
    std::condition_variable cv;
    std::atomic<bool> flag{ false };
    std::atomic<bool> hasFinished{ false };
};

class backend_manager
{
  public:
    backend_manager(std::shared_ptr<plugin_manager> plugin_manager, std::shared_ptr<backend_event_dispatcher> event_dispatcher);
    ~backend_manager();

    void shutdown();

    bool spawn_plugin(plugin_manager::plugin_t& plugin);
    bool destroy_plugin(const std::string& pluginName, bool isShuttingDown = false);

    /** stop all plugin child processes. */
    bool destroy_all_plugins(bool isShuttingDown = false);

    bool has_any_backends();
    bool has_all_backends_stopped();
    bool is_any_backend_running(std::string plugin_name);

    /** rpc evaluate a lua function in the child process. */
    json evaluate(const std::string& pluginName, const json& script);

    /** rpc notify child that frontend loaded. */
    void notify_frontend_loaded(const std::string& pluginName);

    void notify_child(const std::string& pluginName, const std::string& method, const nlohmann::json& params = nullptr);

    /** set the handler for child-initiated RPCs (call_frontend_method, etc.). */
    void set_child_request_handler(PluginProcess::request_handler handler);

    /** OS-level process metrics. */
    PluginProcess::process_metrics get_plugin_metrics(const std::string& pluginName);
    std::unordered_map<std::string, PluginProcess::process_metrics> get_all_plugin_metrics();

  private:
    std::unordered_map<std::string, std::unique_ptr<PluginProcess>> m_processes;
    std::mutex m_processes_mutex;

    PluginProcess::request_handler m_child_request_handler;

    std::atomic<bool> m_has_shutdown{ false };

    std::shared_ptr<plugin_manager> m_plugin_manager;
    std::shared_ptr<backend_event_dispatcher> m_backend_event_dispatcher;
};
