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

#include "millennium/plugin_ipc.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

/**
 * wraps/interfaces a child process running a plugin's Lua backend.
 *
 * Each plugin gets its own OS process talking over a unix socket (on windows too as unix sockets are support in w10+).
 * A reader thread demuxes incoming frames: responses get matched to their pending call(), requests
 * from the child (like "call_frontend_method") get dispatched to the handler.
 * Writes are mutex-protected so any thread can safely send to the child.
 */
class PluginProcess
{
  public:
    using request_handler = std::function<nlohmann::json(const std::string& plugin_name, const std::string& method, const nlohmann::json& params)>;

#ifdef _WIN32
    using pid_type = DWORD;
#else
    using pid_type = pid_t;
#endif

    PluginProcess(const std::string& plugin_name, const std::string& socket_path, plugin_ipc::socket_fd client_fd, pid_type pid, request_handler handler = nullptr);
    ~PluginProcess();

    nlohmann::json call(const std::string& method, const nlohmann::json& params = nullptr, std::chrono::milliseconds timeout = std::chrono::seconds(30));
    void notify_child(const std::string& method, const nlohmann::json& params = nullptr);
    void set_request_handler(request_handler handler);
    void shutdown();
    bool is_alive() const;
    pid_type pid() const
    {
        return m_pid;
    }
    const std::string& plugin_name() const
    {
        return m_plugin_name;
    }

    struct process_metrics
    {
        size_t rss_bytes = 0;
        size_t heap_bytes = 0;
        double cpu_percent = 0.0;
    };

    process_metrics get_metrics();

    friend std::unique_ptr<PluginProcess> spawn_plugin_process(const std::string& plugin_name, const std::string& exe_path, const std::string& socket_path,
                                                               const nlohmann::json& init_params, PluginProcess::request_handler handler);

  private:
    void reader_thread_fn();
    void detect_child_exit();
    bool send_frame(const nlohmann::json& msg);

    void handle_child_notification(const std::string& method, const nlohmann::json& params);

    std::string m_plugin_name;
    std::string m_socket_path;
    plugin_ipc::socket_fd m_client_fd;
    pid_type m_pid;

    std::mutex m_write_mutex;
    std::thread m_reader_thread;
    std::atomic<bool> m_running{ true };
    std::atomic<bool> m_shutdown_initiated{ false };

    std::atomic<int> m_next_id{ 1 };

  public:
    struct pending_entry
    {
        std::promise<nlohmann::json> promise;
    };

  private:
    std::mutex m_pending_mutex;
    std::unordered_map<int, std::shared_ptr<pending_entry>> m_pending;

    request_handler m_request_handler;
    std::mutex m_handler_mutex;

    std::string m_crash_dump_dir;

    /* cpu tracking state — need two snapshots to compute delta */
    mutable uint64_t m_prev_utime = 0;
    mutable uint64_t m_prev_stime = 0;
    mutable std::chrono::steady_clock::time_point m_prev_cpu_time{};

#ifdef _WIN32
    HANDLE m_process_handle = INVALID_HANDLE_VALUE;
#endif
};

/** Fork off a plugin child process and return a connected PluginProcess, or nullptr if something went wrong. */
std::unique_ptr<PluginProcess> spawn_plugin_process(const std::string& plugin_name, const std::string& exe_path, const std::string& socket_path, const nlohmann::json& init_params,
                                                    PluginProcess::request_handler handler = nullptr);
