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
#include "millennium/env.h"
#include "millennium/logger.h"
#include "millennium/sysfs.h"

#include <atomic>
#include <condition_variable>
#include <thread>

#include <Python.h>
#include <lua.hpp>

struct InterpreterMutex
{
    std::mutex mtx;
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

    LuaThreadPoolItem(std::string name, std::thread t, lua_State* state) : pluginName(std::move(name)), thread(std::move(t)), L(state)
    {
    }

    LuaThreadPoolItem(const LuaThreadPoolItem&) = delete;
    LuaThreadPoolItem& operator=(const LuaThreadPoolItem&) = delete;

    LuaThreadPoolItem(LuaThreadPoolItem&& other) noexcept
        : pluginName(std::move(other.pluginName)), thread(std::move(other.thread)), L(other.L), hasFinished(other.hasFinished.load())
    {
        other.L = nullptr;
    }

    LuaThreadPoolItem& operator=(LuaThreadPoolItem&& other) noexcept
    {
        if (this != &other) {
            pluginName = std::move(other.pluginName);
            thread = std::move(other.thread);
            L = other.L;
            hasFinished.store(other.hasFinished.load());
            other.L = nullptr;
        }
        return *this;
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

class BackendManager
{
  private:
    struct LuaThreadWrapper
    {
        lua_State* L;
        SettingsStore::PluginTypeSchema plugin;
        std::function<void(SettingsStore::PluginTypeSchema)> callback;
        std::thread thread;

        LuaThreadWrapper(lua_State* state, SettingsStore::PluginTypeSchema p, std::function<void(SettingsStore::PluginTypeSchema)> cb)
            : L(state), plugin(std::move(p)), callback(std::move(cb))
        {
        }

        ~LuaThreadWrapper()
        {
            if (thread.joinable())
                thread.join();
        }
    };

    static std::unordered_map<std::string, std::atomic<size_t>> sPluginMemoryUsage;
    static std::mutex sPluginMapMutex;
    static std::atomic<size_t> sTotalAllocated;

    static std::atomic<size_t>& GetPluginCounter(const std::string& plugin_name);

    std::mutex m_pythonMutex;
    std::vector<std::tuple<lua_State*, std::unique_ptr<std::mutex>>> m_luaMutexPool;
    PyThreadState* m_InterpreterThreadSave;

    std::vector<std::tuple<std::string, std::thread>> m_pyThreadPool;

    std::vector<LuaThreadPoolItem> m_luaThreadPool;
    std::vector<std::shared_ptr<PythonThreadState>> m_pythonInstances;

    std::tuple<lua_State*, std::unique_ptr<std::mutex>>* FindEntry(lua_State* L);

  public:
    BackendManager();
    ~BackendManager();
    void Shutdown();

    void Lua_LockLua(lua_State* L);
    void Lua_UnlockLua(lua_State* L);
    bool Lua_TryLockLua(lua_State* L);
    bool Lua_IsMutexLocked(lua_State* L);

    /** trace allocations and frees in each lvm to track total memory usage */
    static void* Lua_MemoryProfiler(void* ud, void* ptr, size_t osize, size_t nsize);
    static size_t GetTotalMemory();
    static size_t GetPluginMemorySnapshotByName(const std::string& plugin_name);
    static std::unordered_map<std::string, size_t> GetAllPluginMemorySnapshot();

    bool DestroyPythonInstance(std::string targetPluginName, bool isShuttingDown = false);

    void CallLuaOnUnload(lua_State* L, const std::string& pluginName);
    void CleanupPluginNamePointer(lua_State* L);
    void RemoveMutexFromPool(lua_State* L);
    void RemoveMemoryTracking(const std::string& pluginName);
    bool DestroyLuaInstance(std::string pluginName, bool shouldCleanupThreadPool = true);

    bool DestroyAllPythonInstances();
    bool DestroyAllLuaInstances();

    bool CreatePythonInstance(SettingsStore::PluginTypeSchema& plugin, std::function<void(SettingsStore::PluginTypeSchema)> callback);
    bool CreateLuaInstance(SettingsStore::PluginTypeSchema& plugin, std::function<void(SettingsStore::PluginTypeSchema, lua_State*)> callback);

    bool HasAnyPythonBackends();
    bool HasAnyLuaBackends();

    bool HasAllPythonBackendsStopped();
    bool HasAllLuaBackendsStopped();

    bool IsPythonBackendRunning(std::string pluginName);
    bool IsLuaBackendRunning(std::string pluginName);

    bool HasPythonBackend(std::string pluginName);

    SettingsStore::PluginBackendType GetPluginBackendType(std::string pluginName);

    std::optional<std::shared_ptr<PythonThreadState>> GetPythonThreadStateFromName(std::string pluginName);
    std::optional<lua_State*> GetLuaThreadStateFromName(std::string pluginName);

    std::string GetPluginNameFromThreadState(PyThreadState* thread);

    static BackendManager& GetInstance()
    {
        static BackendManager InstanceRef;
        return InstanceRef;
    }
};