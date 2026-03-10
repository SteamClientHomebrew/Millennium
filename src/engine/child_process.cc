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

#include "millennium/child_process.h"
#include "millennium/logger.h"
#include "millennium/plugin_ipc.h"
#include "millennium/filesystem.h"
#include "mep/crash_event_bus.h"

#include <filesystem>

#include <cstring>
#include <future>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <afunix.h>
#include <windows.h>
#include <io.h>
#define unlink _unlink
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(__linux__)
#include <fstream>
#include <sstream>

static PluginProcess::process_metrics read_proc_metrics(pid_t pid)
{
    PluginProcess::process_metrics m;

    /* rss from /proc/<pid>/statm — second field is resident pages */
    {
        std::ifstream f(fmt::format("/proc/{}/statm", pid));
        if (f.is_open()) {
            size_t size_pages = 0, resident_pages = 0;
            f >> size_pages >> resident_pages;
            m.rss_bytes = resident_pages * static_cast<size_t>(sysconf(_SC_PAGESIZE));
        }
    }

    /* cpu usage needs delta tracking over time — not worth the complexity yet */
    m.cpu_percent = 0.0;

    return m;
}
#elif defined(__APPLE__)
#include <mach/mach.h>

static PluginProcess::process_metrics read_proc_metrics(pid_t pid)
{
    PluginProcess::process_metrics m;
    /* use task_info for memory */
    mach_port_t task;
    if (task_for_pid(mach_task_self(), pid, &task) == KERN_SUCCESS) {
        mach_task_basic_info_data_t info;
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(task, MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
            m.rss_bytes = info.resident_size;
        }
        mach_port_deallocate(mach_task_self(), task);
    }
    m.cpu_percent = 0.0;
    return m;
}
#elif defined(_WIN32)
#include <psapi.h>

static PluginProcess::process_metrics read_proc_metrics(HANDLE hProcess)
{
    PluginProcess::process_metrics m;
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        m.rss_bytes = pmc.WorkingSetSize;
    }
    m.cpu_percent = 0.0;
    return m;
}
#endif

PluginProcess::PluginProcess(const std::string& plugin_name, const std::string& socket_path, plugin_ipc::socket_fd client_fd, pid_type pid, request_handler handler)
    : m_plugin_name(plugin_name), m_socket_path(socket_path), m_client_fd(client_fd), m_pid(pid), m_request_handler(std::move(handler))
{
    m_reader_thread = std::thread(&PluginProcess::reader_thread_fn, this);
}

PluginProcess::~PluginProcess()
{
    m_running.store(false);

    if (m_client_fd != plugin_ipc::INVALID_FD) {
#ifdef _WIN32
        ::shutdown(m_client_fd, SD_BOTH);
#else
        ::shutdown(m_client_fd, SHUT_RDWR);
#endif
        plugin_ipc::close_fd(m_client_fd);
        m_client_fd = plugin_ipc::INVALID_FD;
    }

    if (m_reader_thread.joinable()) {
        m_reader_thread.join();
    }

    /** check if it failed to join and detach as a fallback */
    if (m_reader_thread.joinable()) {
        m_reader_thread.detach();
    }

    /* clean up the socket file so we don't leak temp entries */
    ::unlink(m_socket_path.c_str());

    /* make sure the child is actually gone */
    if (is_alive()) {
#ifdef _WIN32
        TerminateProcess(m_process_handle, 1);
        CloseHandle(m_process_handle);
#else
        ::kill(m_pid, SIGTERM);
        int status;
        ::waitpid(m_pid, &status, WNOHANG);
#endif
    }

    /* fail anything still waiting for a response */
    {
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        for (auto& [id, entry] : m_pending) {
            try {
                entry->promise.set_exception(std::make_exception_ptr(std::runtime_error("plugin process terminated")));
            } catch (...) {
            }
        }
        m_pending.clear();
    }
}

bool PluginProcess::send_frame(const nlohmann::json& msg)
{
    std::lock_guard<std::mutex> lock(m_write_mutex);
    return plugin_ipc::write_msg(m_client_fd, msg);
}

nlohmann::json PluginProcess::call(const std::string& method, const nlohmann::json& params, std::chrono::milliseconds timeout)
{
    int id = m_next_id.fetch_add(1);

    auto entry = std::make_shared<pending_entry>();
    auto future = entry->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        m_pending[id] = entry;
    }

    nlohmann::json req = {
        { "type",   plugin_ipc::TYPE_REQUEST },
        { "id",     id                       },
        { "method", method                   },
        { "params", params                   }
    };

    if (!send_frame(req)) {
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        m_pending.erase(id);
        throw std::runtime_error("failed to send RPC to child: " + m_plugin_name);
    }

    if (future.wait_for(timeout) != std::future_status::ready) {
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        m_pending.erase(id);
        throw std::runtime_error("RPC timeout for child: " + m_plugin_name);
    }

    return future.get();
}

void PluginProcess::notify_child(const std::string& method, const nlohmann::json& params)
{
    nlohmann::json msg = {
        { "type",   plugin_ipc::TYPE_NOTIFY },
        { "method", method                  },
        { "params", params                  }
    };
    send_frame(msg);
}

void PluginProcess::set_request_handler(request_handler handler)
{
    std::lock_guard<std::mutex> lock(m_handler_mutex);
    m_request_handler = std::move(handler);
}

void PluginProcess::shutdown()
{
    m_shutdown_initiated.store(true, std::memory_order_release);

    try {
        call(plugin_ipc::parent_method::SHUTDOWN, nullptr, std::chrono::seconds(5));
    } catch (...) {
        /* timeout or error — will be killed in destructor */
    }
}

bool PluginProcess::is_alive() const
{
#ifdef _WIN32
    if (m_process_handle == INVALID_HANDLE_VALUE) return false;
    DWORD exitCode;
    if (!GetExitCodeProcess(m_process_handle, &exitCode)) return false;
    return exitCode == STILL_ACTIVE;
#else
    return ::kill(m_pid, 0) == 0;
#endif
}

PluginProcess::process_metrics PluginProcess::get_metrics()
{
    process_metrics m;

    /* OS-level memory */
#ifdef _WIN32
    auto os = read_proc_metrics(m_process_handle);
#else
    auto os = read_proc_metrics(m_pid);
#endif
    m.rss_bytes = os.rss_bytes;

#if defined(__linux__)
    {
        std::ifstream f(fmt::format("/proc/{}/stat", m_pid));
        if (f.is_open()) {
            std::string line;
            std::getline(f, line);

            auto close_paren = line.rfind(')');
            if (close_paren != std::string::npos) {
                std::istringstream ss(line.substr(close_paren + 2));
                std::string tok;
                for (int i = 1; i <= 13 && ss >> tok; ++i) {
                    if (i == 12) {
                        uint64_t utime = std::stoull(tok);
                        ss >> tok;
                        uint64_t stime = std::stoull(tok);

                        auto now = std::chrono::steady_clock::now();
                        if (m_prev_cpu_time != std::chrono::steady_clock::time_point{}) {
                            double wall_secs = std::chrono::duration<double>(now - m_prev_cpu_time).count();
                            if (wall_secs > 0.0) {
                                long ticks_per_sec = sysconf(_SC_CLK_TCK);
                                uint64_t delta_ticks = (utime - m_prev_utime) + (stime - m_prev_stime);
                                m.cpu_percent = (static_cast<double>(delta_ticks) / ticks_per_sec) / wall_secs * 100.0;
                            }
                        }
                        m_prev_utime = utime;
                        m_prev_stime = stime;
                        m_prev_cpu_time = now;
                        break;
                    }
                }
            }
        }
    }
#elif defined(__APPLE__)
    {
        mach_port_t task;
        if (task_for_pid(mach_task_self(), m_pid, &task) == KERN_SUCCESS) {
            task_thread_times_info_data_t times;
            mach_msg_type_number_t count = TASK_THREAD_TIMES_INFO_COUNT;
            if (task_info(task, TASK_THREAD_TIMES_INFO, (task_info_t)&times, &count) == KERN_SUCCESS) {
                uint64_t user_us = static_cast<uint64_t>(times.user_time.seconds) * 1000000 + times.user_time.microseconds;
                uint64_t sys_us = static_cast<uint64_t>(times.system_time.seconds) * 1000000 + times.system_time.microseconds;

                auto now = std::chrono::steady_clock::now();
                if (m_prev_cpu_time != std::chrono::steady_clock::time_point{}) {
                    double wall_secs = std::chrono::duration<double>(now - m_prev_cpu_time).count();
                    if (wall_secs > 0.0) {
                        uint64_t delta_us = (user_us - m_prev_utime) + (sys_us - m_prev_stime);
                        m.cpu_percent = (static_cast<double>(delta_us) / 1000000.0) / wall_secs * 100.0;
                    }
                }
                m_prev_utime = user_us;
                m_prev_stime = sys_us;
                m_prev_cpu_time = now;
            }
            mach_port_deallocate(mach_task_self(), task);
        }
    }
#elif defined(_WIN32)
    {
        FILETIME creation, exit, kernel, user;
        if (GetProcessTimes(m_process_handle, &creation, &exit, &kernel, &user)) {
            uint64_t user_ticks = (static_cast<uint64_t>(user.dwHighDateTime) << 32) | user.dwLowDateTime;
            uint64_t kern_ticks = (static_cast<uint64_t>(kernel.dwHighDateTime) << 32) | kernel.dwLowDateTime;

            auto now = std::chrono::steady_clock::now();
            if (m_prev_cpu_time != std::chrono::steady_clock::time_point{}) {
                double wall_secs = std::chrono::duration<double>(now - m_prev_cpu_time).count();
                if (wall_secs > 0.0) {
                    uint64_t delta = (user_ticks - m_prev_utime) + (kern_ticks - m_prev_stime);
                    m.cpu_percent = (static_cast<double>(delta) / 10000000.0) / wall_secs * 100.0;
                }
            }
            m_prev_utime = user_ticks;
            m_prev_stime = kern_ticks;
            m_prev_cpu_time = now;
        }
    }
#endif

    /* Lua heap from the child process */
    try {
        auto result = call(plugin_ipc::parent_method::GET_METRICS, nullptr, std::chrono::seconds(2));
        m.heap_bytes = result.value("heap_bytes", static_cast<size_t>(0));
    } catch (...) {
        /* child might be busy, just leave heap_bytes at 0 */
    }

    return m;
}

void PluginProcess::handle_child_notification(const std::string& method, const nlohmann::json& params)
{
    std::lock_guard<std::mutex> lock(m_handler_mutex);
    if (m_request_handler) {
        try {
            m_request_handler(m_plugin_name, method, params);
        } catch (const std::exception& e) {
            LOG_ERROR("[PluginProcess] notification handler error for '{}': {}", m_plugin_name, e.what());
        }
    }
}

void PluginProcess::reader_thread_fn()
{
    while (m_running.load()) {
        nlohmann::json msg;
        if (!plugin_ipc::read_msg(m_client_fd, msg)) {
            m_running.store(false);
            break;
        }

        std::string type = msg.value("type", "");

        if (type == plugin_ipc::TYPE_RESPONSE) {
            int id = msg.value("id", -1);
            std::shared_ptr<pending_entry> entry;

            {
                std::lock_guard<std::mutex> lock(m_pending_mutex);
                auto it = m_pending.find(id);
                if (it != m_pending.end()) {
                    entry = it->second;
                    m_pending.erase(it);
                }
            }

            if (entry) {
                if (msg.contains("error")) {
                    entry->promise.set_exception(std::make_exception_ptr(std::runtime_error(msg["error"].get<std::string>())));
                } else {
                    entry->promise.set_value(msg.value("result", nlohmann::json(nullptr)));
                }
            }

        } else if (type == plugin_ipc::TYPE_REQUEST) {
            int id = msg.value("id", -1);
            std::string method = msg.value("method", "");
            nlohmann::json params = msg.value("params", nlohmann::json::object());

            request_handler handler;
            {
                std::lock_guard<std::mutex> lock(m_handler_mutex);
                handler = m_request_handler;
            }

            if (handler) {
                try {
                    nlohmann::json result = handler(m_plugin_name, method, params);

                    nlohmann::json resp = {
                        { "type",   plugin_ipc::TYPE_RESPONSE },
                        { "id",     id                        },
                        { "result", result                    }
                    };
                    send_frame(resp);
                } catch (const std::exception& e) {
                    nlohmann::json resp = {
                        { "type",  plugin_ipc::TYPE_RESPONSE },
                        { "id",    id                        },
                        { "error", std::string(e.what())     }
                    };
                    send_frame(resp);
                }
            } else {
                nlohmann::json resp = {
                    { "type",  plugin_ipc::TYPE_RESPONSE },
                    { "id",    id                        },
                    { "error", "no handler registered"   }
                };
                send_frame(resp);
            }

        } else if (type == plugin_ipc::TYPE_NOTIFY) {
            std::string method = msg.value("method", "");
            nlohmann::json params = msg.value("params", nlohmann::json(nullptr));
            handle_child_notification(method, params);
        }
    }

    /* reader loop died — fail anything still pending */
    {
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        for (auto& [id, entry] : m_pending) {
            try {
                entry->promise.set_exception(std::make_exception_ptr(std::runtime_error("child disconnected: " + m_plugin_name)));
            } catch (...) {
            }
        }
        m_pending.clear();
    }

    /* detect crash vs clean exit */
    detect_child_exit();
}

void PluginProcess::detect_child_exit()
{
    /* If we initiated shutdown via the SHUTDOWN RPC, any exit is expected — not a crash. */
    if (m_shutdown_initiated.load(std::memory_order_acquire)) {
        return;
    }

#ifdef _WIN32
    if (m_process_handle == INVALID_HANDLE_VALUE) return;

    WaitForSingleObject(m_process_handle, 3000);
    DWORD exit_code = 0;
    GetExitCodeProcess(m_process_handle, &exit_code);

    if (exit_code != 0 && exit_code != STILL_ACTIVE) {
        LOG_ERROR("Plugin '{}' crashed (exit code 0x{:08X}). Crash dump: {}", m_plugin_name, exit_code, m_crash_dump_dir.empty() ? "none" : m_crash_dump_dir);
        mep::crash_event_bus::instance().notify({ m_plugin_name, exit_code, m_crash_dump_dir });
    }
#else
    int status = 0;
    pid_t result = ::waitpid(m_pid, &status, WNOHANG);

    if (result <= 0) {
        /* child may still be running (clean disconnect) or already reaped */
        return;
    }

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        LOG_ERROR("Plugin '{}' crashed with signal {} (0x{:08X}). Crash dump: {}", m_plugin_name, sig, (unsigned long)sig, m_crash_dump_dir.empty() ? "none" : m_crash_dump_dir);
        mep::crash_event_bus::instance().notify({ m_plugin_name, (unsigned long)sig, m_crash_dump_dir });
    } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        unsigned long code = (unsigned long)WEXITSTATUS(status);
        LOG_ERROR("Plugin '{}' exited with code {} (0x{:08X}). Crash dump: {}", m_plugin_name, code, code, m_crash_dump_dir.empty() ? "none" : m_crash_dump_dir);
        mep::crash_event_bus::instance().notify({ m_plugin_name, code, m_crash_dump_dir });
    }
#endif
}

std::unique_ptr<PluginProcess> spawn_plugin_process(const std::string& plugin_name, const std::string& exe_path, const std::string& socket_path, const nlohmann::json& init_params,
                                                    PluginProcess::request_handler handler)
{
    /* set up a listening socket for the child to connect back to */
    ::unlink(socket_path.c_str());

    plugin_ipc::socket_fd server_fd = static_cast<plugin_ipc::socket_fd>(::socket(AF_UNIX, SOCK_STREAM, 0));

#ifdef _WIN32
    if (server_fd == INVALID_SOCKET) {
#else
    if (server_fd < 0) {
#endif
        LOG_ERROR("[spawn] socket() failed for plugin '{}'", plugin_name);
        return nullptr;
    }

#ifndef _WIN32
    ::fcntl(server_fd, F_SETFD, FD_CLOEXEC);
#endif

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("[spawn] bind({}) failed for plugin '{}'", socket_path, plugin_name);
        plugin_ipc::close_fd(server_fd);
        return nullptr;
    }

    if (::listen(server_fd, 1) < 0) {
        LOG_ERROR("[spawn] listen() failed for plugin '{}'", plugin_name);
        plugin_ipc::close_fd(server_fd);
        return nullptr;
    }

    /* fork the child */
    PluginProcess::pid_type child_pid;

#ifdef _WIN32
    HANDLE hProcess = INVALID_HANDLE_VALUE;
    {
        std::string cmd = "\"" + exe_path + "\" \"" + socket_path + "\"";

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            LOG_ERROR("[spawn] CreateProcess failed for plugin '{}'", plugin_name);
            plugin_ipc::close_fd(server_fd);
            return nullptr;
        }

        child_pid = pi.dwProcessId;
        hProcess = pi.hProcess;
        CloseHandle(pi.hThread);
    }
#else
    {
        child_pid = ::fork();
        if (child_pid < 0) {
            LOG_ERROR("[spawn] fork() failed for plugin '{}'", plugin_name);
            plugin_ipc::close_fd(server_fd);
            return nullptr;
        }

        if (child_pid == 0) {
            /* child process */
            ::close(server_fd);
            ::execl(exe_path.c_str(), exe_path.c_str(), socket_path.c_str(), nullptr);
            /* execl only returns on error */
            _exit(127);
        }
    }
#endif

    /* wait for the child to connect back (10s timeout via select, not SO_RCVTIMEO —
       SO_RCVTIMEO on a listening socket is inherited by accepted sockets on Windows,
       which would silently timeout all recv() calls on the IPC channel) */
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);
        struct timeval tv = { 10, 0 };
#ifdef _WIN32
        int ready = ::select(0, &fds, nullptr, nullptr, &tv);
#else
        int ready = ::select(static_cast<int>(server_fd) + 1, &fds, nullptr, nullptr, &tv);
#endif
        if (ready <= 0) {
            LOG_ERROR("[spawn] accept() timed out waiting for plugin '{}' to connect", plugin_name);
            plugin_ipc::close_fd(server_fd);
#ifdef _WIN32
            TerminateProcess(hProcess, 1);
            CloseHandle(hProcess);
#else
            ::kill(child_pid, SIGTERM);
            ::waitpid(child_pid, nullptr, 0);
#endif
            return nullptr;
        }
    }

    plugin_ipc::socket_fd client_fd = static_cast<plugin_ipc::socket_fd>(::accept(server_fd, nullptr, nullptr));

    plugin_ipc::close_fd(server_fd);

#ifdef _WIN32
    if (client_fd == INVALID_SOCKET) {
#else
    if (client_fd < 0) {
#endif
        LOG_ERROR("[spawn] accept() failed for plugin '{}'", plugin_name);
#ifdef _WIN32
        TerminateProcess(hProcess, 1);
        CloseHandle(hProcess);
#else
        ::kill(child_pid, SIGTERM);
        ::waitpid(child_pid, nullptr, 0);
#endif
        return nullptr;
    }

    /* tell the child who it is and what to load */
    nlohmann::json init_msg = {
        { "type",   plugin_ipc::TYPE_REQUEST        },
        { "id",     0                               },
        { "method", plugin_ipc::parent_method::INIT },
        { "params", init_params                     }
    };

    if (!plugin_ipc::write_msg(client_fd, init_msg)) {
        LOG_ERROR("[spawn] failed to send init to plugin '{}'", plugin_name);
        plugin_ipc::close_fd(client_fd);
#ifdef _WIN32
        TerminateProcess(hProcess, 1);
        CloseHandle(hProcess);
#else
        ::kill(child_pid, SIGTERM);
        ::waitpid(child_pid, nullptr, 0);
#endif
        return nullptr;
    }

    /* wrap it up — handler must be set before the reader thread starts,
       otherwise early child RPCs (patches, ready) race against set_request_handler */
    auto process = std::make_unique<PluginProcess>(plugin_name, socket_path, client_fd, child_pid, std::move(handler));

#ifdef _WIN32
    process->m_process_handle = hProcess;
#endif

    process->m_crash_dump_dir = init_params.value("crash_dump_dir", "");

    /* wait for the child to finish init and respond.
       we manually register id=0 in the pending map since the reader thread is
       already running and will deliver the response for us. */
    {
        auto entry = std::make_shared<PluginProcess::pending_entry>();
        auto future = entry->promise.get_future();

        {
            std::lock_guard<std::mutex> lock(process->m_pending_mutex);
            process->m_pending[0] = entry;
        }

        if (future.wait_for(std::chrono::seconds(30)) != std::future_status::ready) {
            LOG_ERROR("[spawn] init response timed out for plugin '{}'", plugin_name);
            return nullptr;
        }

        try {
            auto result = future.get();
            if (!result.value("ok", false)) {
                LOG_ERROR("[spawn] init failed for plugin '{}': {}", plugin_name, result.dump());
                return nullptr;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("[spawn] init error for plugin '{}': {}", plugin_name, e.what());
            return nullptr;
        }
    }

    logger.log("Plugin '{}' child process started (pid={})", plugin_name, child_pid);
    return process;
}
