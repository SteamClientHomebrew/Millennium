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
#include "millennium/life_cycle.h"
#include "millennium/logger.h"
#include "state/shared_memory.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <optional>

std::unordered_map<std::string, std::atomic<size_t>> backend_manager::sPluginMemoryUsage;
std::mutex backend_manager::sPluginMapMutex;
std::atomic<size_t> backend_manager::sTotalAllocated{ 0 };

extern std::condition_variable cv_hasSteamUnloaded;
extern std::mutex mtx_hasSteamUnloaded;

backend_manager::backend_manager(std::shared_ptr<plugin_manager> plugin_manager, std::shared_ptr<backend_event_dispatcher> event_dispatcher)
    : m_plugin_manager(std::move(plugin_manager)), m_backend_event_dispatcher(std::move(event_dispatcher))
{
}

backend_manager::~backend_manager()
{
#if defined(__linux__) || defined(__APPLE__) || defined(MILLENNIUM_32BIT)
    logger.log("Shutting down backend_manager...");
    this->shutdown();
#endif
}

void backend_manager::shutdown()
{
    logger.warn("Unloading {} plugin(s) and preparing for exit...", this->m_luaThreadPool.size());

#ifdef MILLENNIUM_32BIT
    std::exit(0);
#endif

    const auto startTime = std::chrono::steady_clock::now();

    logger.log("[BackendManager::Shutdown] Destroying all Lua instances...");
    this->destroy_lua_vms(true);

    logger.log("[BackendManager::Shutdown] All plugins have been shut down...");

    /** Shutdown Lua interpreters */
    for (auto it : m_luaThreadPool) {
        auto& [pluginName, thread, L, hasFinished] = *it;
        logger.log("Joining Lua thread for plugin '{}'", pluginName);
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_luaThreadPool.clear();

    logger.log("Finished shutdown! Bye bye!");
    logger.log("Shutdown took {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count());
}

bool backend_manager::has_any_lua_backends()
{
    return this->m_luaThreadPool.size() > 0;
}

bool backend_manager::destroy_lua_vms(bool isShuttingDown)
{
    if (m_luaThreadPool.empty()) {
        logger.log("No Lua instances to destroy.");
        return true;
    }

    std::vector<std::string> pluginNames;
    for (auto it : m_luaThreadPool) {
        auto& [pluginName, thread, L, hasFinished] = *it;
        pluginNames.push_back(pluginName);
    }

    for (auto& pluginName : pluginNames) {
        logger.log("[Lua] Shutting down plugin [{}]", pluginName);
        destroy_lua_vm(pluginName, true, isShuttingDown);
    }

    return true;
}

void backend_manager::lua_invoke_plugin_unload(lua_State* L, const std::string& pluginName)
{
    lua_getglobal(L, "MILLENNIUM_PLUGIN_DEFINITION");

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_getfield(L, -1, "on_unload");

    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        LOG_ERROR("Failed to unload lua plugin '{}': {}", pluginName, err ? err : "unknown error");
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}

void backend_manager::lua_cleanup_plugin_name_ptr(lua_State* L)
{
    void* ud = lua_touserdata(L, lua_upvalueindex(1));
    if (ud) {
        delete static_cast<std::shared_ptr<std::string>*>(ud);
    }
}

void backend_manager::lua_remove_mtx(lua_State* L)
{
    m_luaMutexPool.erase(std::remove_if(m_luaMutexPool.begin(), m_luaMutexPool.end(), [L](const auto& entry) { return std::get<0>(entry) == L; }), m_luaMutexPool.end());
}

void backend_manager::lua_remove_mem_tracking(const std::string& pluginName)
{
    std::lock_guard<std::mutex> lock(sPluginMapMutex);
    sPluginMemoryUsage.erase(pluginName);
}

bool backend_manager::destroy_lua_vm(std::string pluginName, bool shouldCleanupThreadPool, bool isShuttingDown)
{
    for (auto it = this->m_luaThreadPool.begin(); it != this->m_luaThreadPool.end(); /* No increment */) {
        auto& [threadPluginName, thread, L, hasFinished] = **it;

        if (threadPluginName != pluginName) {
            ++it;
            continue;
        }

        logger.log("Joining Lua thread for plugin '{}'", pluginName);

        if (thread.joinable()) {
            thread.join();
        }

        if (!L) {
            if (shouldCleanupThreadPool) {
                it = this->m_luaThreadPool.erase(it);
            } else {
                (*it)->hasFinished.store(true);
                {
                    std::unique_lock<std::mutex> lk(mtx_hasSteamUnloaded);
                    cv_hasSteamUnloaded.notify_all();
                }
                ++it;
            }
            m_backend_event_dispatcher->backend_unloaded_event_hdlr({ pluginName }, isShuttingDown);
            return true;
        }

        lua_lock(L);
        {
            lua_invoke_plugin_unload(L, pluginName);
            lua_cleanup_plugin_name_ptr(L);

            lua_close(L);
        }
        lua_unlock(L);

        lua_remove_mtx(L);
        lua_remove_mem_tracking(pluginName);

        /** remove all patches from this plugin from the shared memory arena */
        if (g_lb_patch_arena) {
            hashmap_remove(g_lb_patch_arena, pluginName.c_str());
        }

        if (shouldCleanupThreadPool) {
            it = this->m_luaThreadPool.erase(it);
        } else {
            (*it)->hasFinished.store(true);
            {
                std::unique_lock<std::mutex> lk(mtx_hasSteamUnloaded);
                cv_hasSteamUnloaded.notify_all();
            }
            ++it;
        }
        m_backend_event_dispatcher->backend_unloaded_event_hdlr({ pluginName }, isShuttingDown);
        return true;
    }
    return false;
}

bool backend_manager::has_all_lua_backends_stopped()
{
    for (auto it : m_luaThreadPool) {
        auto& [pluginName, thread, L, hasFinished] = *it;
        if (!hasFinished.load()) {
            return false;
        }
    }
    return true;
}

bool backend_manager::is_lua_backend_running(std::string targetPluginName)
{
    for (auto it : m_luaThreadPool) {
        auto& [pluginName, thread, L, hasFinished] = *it;
        if (targetPluginName == pluginName) {
            return true;
        }
    }
    return false;
}

bool backend_manager::is_any_backend_running(std::string plugin_name)
{
    return is_lua_backend_running(plugin_name);
}

plugin_manager::backend_t backend_manager::get_plugin_backend_type(std::string)
{
    return plugin_manager::backend_t::Lua;
}

std::optional<lua_State*> backend_manager::lua_thread_state_from_plugin_name(std::string pluginName)
{
    for (auto it : m_luaThreadPool) {
        auto& [name, thread, L, hasFinished] = *it;
        if (name == pluginName) {
            return L;
        }
    }
    return std::nullopt;
}

std::tuple<lua_State*, std::unique_ptr<std::mutex>>* backend_manager::Lua_FindEntry(lua_State* L)
{
    for (auto& entry : m_luaMutexPool) {
        if (std::get<0>(entry) == L) {
            return &entry;
        }
    }
    return nullptr;
}

void backend_manager::lua_lock(lua_State* L)
{
    auto entry = Lua_FindEntry(L);
    if (!entry) throw std::runtime_error("lua_State not found in pool");

    std::get<1>(*entry)->lock();
}

void backend_manager::lua_unlock(lua_State* L)
{
    auto entry = Lua_FindEntry(L);
    if (!entry) throw std::runtime_error("lua_State not found in pool");

    std::get<1>(*entry)->unlock();
}

bool backend_manager::lua_try_lock(lua_State* L)
{
    auto entry = Lua_FindEntry(L);
    if (!entry) throw std::runtime_error("lua_State not found in pool");

    return std::get<1>(*entry)->try_lock();
}

bool backend_manager::lua_is_locked(lua_State* L)
{
    auto entry = Lua_FindEntry(L);
    if (!entry) throw std::runtime_error("lua_State not found in pool");

    auto& m = std::get<1>(*entry);
    if (m->try_lock()) {
        m->unlock();
        return false;
    }
    return true;
}

std::atomic<size_t>& backend_manager::Lua_GetPluginCounter(const std::string& plugin_name)
{
    std::lock_guard<std::mutex> lock(sPluginMapMutex);
    return sPluginMemoryUsage[plugin_name];
}

size_t backend_manager::Lua_GetTotalMemory()
{
    return sTotalAllocated.load();
}

size_t backend_manager::Lua_GetPluginMemorySnapshotByName(const std::string& plugin_name)
{
    std::lock_guard<std::mutex> lock(sPluginMapMutex);
    auto it = sPluginMemoryUsage.find(plugin_name);
    return (it != sPluginMemoryUsage.end()) ? it->second.load() : 0;
}

std::unordered_map<std::string, size_t> backend_manager::Lua_GetAllPluginMemorySnapshot()
{
    std::lock_guard<std::mutex> lock(sPluginMapMutex);
    std::unordered_map<std::string, size_t> snapshot;
    for (const auto& pair : sPluginMemoryUsage) {
        snapshot[pair.first] = pair.second.load();
    }
    return snapshot;
}

void* backend_manager::Lua_MemoryProfiler(void* ud, void* ptr, size_t osize, size_t nsize)
{
    const std::string& pluginName = **static_cast<std::shared_ptr<std::string>*>(ud);

    if (nsize == 0) {
        if (ptr) {
            sTotalAllocated.fetch_sub(osize);
            Lua_GetPluginCounter(pluginName).fetch_sub(osize);
            free(ptr);
        }
        return nullptr;
    } else {
        void* newptr = realloc(ptr, nsize);
        if (!newptr) {
            return nullptr;
        }

        auto& pluginCounter = Lua_GetPluginCounter(pluginName);

        if (ptr) {
            sTotalAllocated.fetch_sub(osize);
            pluginCounter.fetch_sub(osize);
        }

        sTotalAllocated.fetch_add(nsize);
        pluginCounter.fetch_add(nsize);
        return newptr;
    }
}

bool backend_manager::create_lua_vm(plugin_manager::plugin_t& plugin, std::function<void(plugin_manager::plugin_t, lua_State*)> callback)
{
    auto pluginNamePtr = new std::shared_ptr<std::string>(std::make_shared<std::string>(plugin.plugin_name));

    lua_State* L = lua_newstate(backend_manager::Lua_MemoryProfiler, pluginNamePtr);
    m_luaMutexPool.emplace_back(L, std::make_unique<std::mutex>());

    if (!L) {
        LOG_ERROR("Failed to create new Lua state");
        return false;
    }

    luaL_openlibs(L);

    std::string pluginName = plugin.plugin_name;
    std::thread luaThread([L, plugin, callback]() mutable
    {
        if (callback) callback(plugin, L);
    });

    m_luaThreadPool.emplace_back(std::make_shared<LuaThreadPoolItem>(pluginName, luaThread, L));
    return true;
}
