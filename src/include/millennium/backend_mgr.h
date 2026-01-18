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
#include "millennium/environment.h"
#include "millennium/life_cycle.h"
#include "millennium/config.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <thread>

#include <Python.h>
#include <lua.hpp>

struct InterpreterMutex
{
    std::mutex mtx, runMutex;
    std::condition_variable cv;
    std::atomic<bool> flag{ false };
    std::atomic<bool> hasFinished{ false };
};

struct LuaThreadPoolItem
{
    std::string pluginName;
    std::thread thread;
    lua_State* L;
    std::atomic<bool> hasFinished{ false };

    LuaThreadPoolItem(std::string pluginName, std::thread& thread, lua_State* L) : pluginName(pluginName), thread(std::move(thread)), L(L)
    {
    }
};

struct PythonThreadState
{
    std::string pluginName;
    PyThreadState* thread_state;
    std::shared_ptr<InterpreterMutex> mutex;

    PythonThreadState(std::string pluginName, PyThreadState* thread_state, std::shared_ptr<InterpreterMutex> mutex)
        : pluginName(pluginName), thread_state(thread_state), mutex(mutex)
    {
    }
};

struct PythonEnvPath
{
    std::string pythonPath, pythonLibs, pythonUserLibs;
};

PythonEnvPath GetPythonEnvPaths();

class backend_manager
{
  public:
    backend_manager(std::shared_ptr<settings_store> settings_store, std::shared_ptr<backend_event_dispatcher> event_dispatcher);
    ~backend_manager();

    void shutdown();

    void lua_lock(lua_State* L);
    void lua_unlock(lua_State* L);
    bool lua_try_lock(lua_State* L);
    bool lua_is_locked(lua_State* L);

    /** trace allocations and frees in each lvm to track total memory usage */
    static void* Lua_MemoryProfiler(void* ud, void* ptr, size_t osize, size_t nsize);
    static size_t Lua_GetTotalMemory();
    static size_t Lua_GetPluginMemorySnapshotByName(const std::string& plugin_name);
    static std::unordered_map<std::string, size_t> Lua_GetAllPluginMemorySnapshot();

    void lua_invoke_plugin_unload(lua_State* L, const std::string& pluginName);
    void lua_cleanup_plugin_name_ptr(lua_State* L);
    void lua_remove_mtx(lua_State* L);
    void lua_remove_mem_tracking(const std::string& pluginName);

    bool python_destroy_vm(std::string targetPluginName, bool isShuttingDown = false);
    bool destroy_lua_vm(std::string pluginName, bool shouldCleanupThreadPool = true, bool isShuttingDown = false);
    bool destroy_generic_vm(std::string plugin_name);

    bool destroy_python_vms();
    bool destroy_lua_vms(bool isShuttingDown = false);

    bool create_python_vm(settings_store::plugin_t& plugin, std::function<void(settings_store::plugin_t)> callback);
    bool create_lua_vm(settings_store::plugin_t& plugin, std::function<void(settings_store::plugin_t, lua_State*)> callback);

    bool has_any_python_backends();
    bool has_any_lua_backends();

    bool has_all_python_backends_stopped();
    bool has_all_lua_backends_stopped();

    bool is_python_backend_running(std::string pluginName);
    bool is_lua_backend_running(std::string pluginName);
    bool is_any_backend_running(std::string plugin_name);

    bool has_python_backend(std::string pluginName);

    settings_store::backend_t get_plugin_backend_type(std::string pluginName);

    std::optional<std::shared_ptr<PythonThreadState>> python_thread_state_from_plugin_name(std::string pluginName);
    std::optional<lua_State*> lua_thread_state_from_plugin_name(std::string pluginName);

    std::string get_plugin_name_from_thread_state(PyThreadState* thread);

  private:
    struct LuaThreadWrapper
    {
        lua_State* L;
        settings_store::plugin_t plugin;
        std::function<void(settings_store::plugin_t)> callback;
        std::thread thread;

        LuaThreadWrapper(lua_State* state, settings_store::plugin_t p, std::function<void(settings_store::plugin_t)> cb) : L(state), plugin(std::move(p)), callback(std::move(cb))
        {
        }

        ~LuaThreadWrapper()
        {
            if (thread.joinable()) thread.join();
        }
    };

    static std::unordered_map<std::string, std::atomic<size_t>> sPluginMemoryUsage;
    static std::mutex sPluginMapMutex;
    static std::atomic<size_t> sTotalAllocated;

    static std::atomic<size_t>& Lua_GetPluginCounter(const std::string& plugin_name);

    std::mutex m_pythonMutex;
    std::vector<std::tuple<lua_State*, std::unique_ptr<std::mutex>>> m_luaMutexPool;
    PyThreadState* m_InterpreterThreadSave;

    std::vector<std::tuple<std::string, std::thread>> m_pyThreadPool;

    std::vector<std::shared_ptr<LuaThreadPoolItem>> m_luaThreadPool;
    std::vector<std::shared_ptr<PythonThreadState>> m_pythonInstances;

    std::tuple<lua_State*, std::unique_ptr<std::mutex>>* Lua_FindEntry(lua_State* L);

    std::shared_ptr<settings_store> m_settings_store;
    std::shared_ptr<backend_event_dispatcher> m_backend_event_dispatcher;
};
