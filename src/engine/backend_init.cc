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

#include "millennium/plugin_loader.h"
#include <memory>
#ifdef _WIN32
#include "millennium/plat_msg.h"
#endif
#include "millennium/lua_modules.h"
#include "millennium/backend_init.h"
#include "millennium/types.h"
#include "millennium/backend_mgr.h"
#include "millennium/logger.h"
#include "millennium/life_cycle.h"
#include "millennium/filesystem.h"
#include "millennium/environment.h"
#include "state/shared_memory.h"

static int plugin_loader_userdata_id;

std::shared_ptr<plugin_loader> backend_initializer::get_plugin_loader_from_lua_vm(lua_State* L)
{
    lua_pushlightuserdata(L, &plugin_loader_userdata_id);
    lua_gettable(L, LUA_REGISTRYINDEX);

    auto* wp = static_cast<std::weak_ptr<plugin_loader>*>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    if (!wp) return {};
    return wp->lock();
}

int backend_initializer::lua_set_plugin_loader_ud(lua_State* L, std::weak_ptr<plugin_loader> wp)
{
    lua_pushlightuserdata(L, &plugin_loader_userdata_id);

    void* ud = lua_newuserdata(L, sizeof(std::weak_ptr<plugin_loader>));
    new (ud) std::weak_ptr<plugin_loader>(std::move(wp));

    constexpr const char* plugin_loader_userdata_binding_name = "millennium_internal_plugin_loader_ud";

    if (luaL_newmetatable(L, plugin_loader_userdata_binding_name)) {
        lua_pushcfunction(L, [](lua_State* L) -> int
        {
            auto* wp = static_cast<std::weak_ptr<plugin_loader>*>(lua_touserdata(L, 1));
            wp->~weak_ptr();
            return 0;
        });
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);

    lua_settable(L, LUA_REGISTRYINDEX);
    return plugin_loader_userdata_id;
}

static void RegisterModule(lua_State* L, const char* name, lua_CFunction func)
{
    lua_pushcclosure(L, func, 0);
    lua_setfield(L, -2, name);
}

void backend_initializer::lua_backend_started_cb(plugin_manager::plugin_t plugin, const std::weak_ptr<plugin_loader> weak_plugin_loader, lua_State* L)
{
    logger.log("Starting Lua backend for '{}'", plugin.plugin_name);

    if (m_backend_manager->lua_is_locked(L)) {
        LOG_ERROR("Lua mutex is already locked for plugin '{}'", plugin.plugin_name);
        return;
    }

    m_backend_manager->lua_lock(L);

    lua_pushstring(L, plugin.plugin_name.c_str());
    lua_setglobal(L, "MILLENNIUM_PLUGIN_SECRET_NAME");

    lua_pushstring(L, plugin.plugin_backend_dir.parent_path().string().c_str());
    lua_setglobal(L, "MILLENNIUM_PLUGIN_SECRET_BACKEND_ABSOLUTE");

    lua_set_plugin_loader_ud(L, weak_plugin_loader);

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    std::string currentPath = lua_tostring(L, -1);
    std::string pluginPath = plugin.plugin_backend_dir.parent_path().string();
    std::string newPath = pluginPath + "/?.lua;" + currentPath;
    lua_pop(L, 1);
    lua_pushstring(L, newPath.c_str());
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");

    RegisterModule(L, "json", luaopen_cjson);
    RegisterModule(L, "millennium", luaopen_millennium_lib);
    RegisterModule(L, "http", luaopen_http_lib);
    RegisterModule(L, "utils", luaopen_utils_lib);
    RegisterModule(L, "logger", luaopen_logger_lib);
    RegisterModule(L, "fs", luaopen_fs_lib);
    RegisterModule(L, "regex", luaopen_regex_lib);
    RegisterModule(L, "datetime", luaopen_datatime_lib);

    lua_pop(L, 2);

    if (luaL_dofile(L, (plugin.plugin_backend_dir).string().c_str()) != LUA_OK) {
        LOG_ERROR("Lua error: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_close(L);
        m_backend_manager->lua_unlock(L);
        return;
    }

    lua_pushvalue(L, -1);
    lua_setglobal(L, "MILLENNIUM_PLUGIN_DEFINITION");

    /** parse the "patches" block */
    load_patches(L, g_lb_patch_arena);
    hashmap_print(g_lb_patch_arena);

    if (!lua_istable(L, -1)) {
        LOG_ERROR("Lua file should return a table with functions");
        lua_pop(L, 1);
        lua_close(L);
        m_backend_manager->lua_unlock(L);
        return;
    }

    lua_getfield(L, -1, "on_load");
    if (!lua_isfunction(L, -1)) {
        LOG_ERROR("Failed to locate 'on_load' function in plugin backend for '{}'", plugin.plugin_name);
        lua_pop(L, 2);
        lua_close(L);
        m_backend_manager->lua_unlock(L);
        return;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        LOG_ERROR("Error calling on_load in plugin backend for '{}': {}", plugin.plugin_name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    m_backend_manager->lua_unlock(L);
}

/**
 * Restores the original `SharedJSContext` by renaming the backup file to the original file.
 * It reverses the patches done in the preloader module
 *
 * @note this function is only applicable to Windows
 */
void backend_initializer::compat_restore_shared_js_context()
{
#ifdef _WIN32
    logger.log("Restoring SharedJSContext...");

    const auto SteamUIModulePath = platform::get_steam_path() / "steamui" / "index.html";

    /** if the sequence isn't found, it indicates it hasn't been patched by millennium <= 2.30.0 preloader */
    if (platform::read_file(SteamUIModulePath.string()).find("<!doctype html><html><head><title>SharedJSContext</title></head></html>") == std::string::npos) {
        logger.log("SharedJSContext isn't patched, skipping...");
        return;
    }

    const auto librariesPath = platform::get_steam_path() / "steamui" / "libraries";
    std::string libraryChunkJS;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(librariesPath)) {
            if (entry.is_regular_file() && entry.path().filename().string().substr(0, 10) == "libraries~" && entry.path().extension() == ".js") {
                libraryChunkJS = entry.path().filename().string();
                break;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        logger.warn("Failed to find libraries~xxx.js: {}", e.what());
    }

    if (libraryChunkJS.empty()) {
        MessageBoxA(NULL,
                    "Millennium failed to find a key library used by Steam. "
                    "Let our developers know if you see this message, it's likely a bug.\n"
                    "You can reach us over at steambrew.app/discord",
                    "Millennium", MB_ICONERROR);

        return;
    }

    std::string fileContent = fmt::format(
        R"(<!doctype html><html style="width: 100%; height: 100%"><head><title>SharedJSContext</title><meta charset="utf-8"><script defer="defer" src="/libraries/{}"></script><script defer="defer" src="/library.js"></script><link href="/css/library.css" rel="stylesheet"></head><body style="width: 100%; height: 100%; margin: 0; overflow: hidden;"><div id="root" style="height:100%; width: 100%"></div><div style="display:none"></div></body></html>)",
        libraryChunkJS);

    try {
        platform::write_file(SteamUIModulePath.string(), fileContent);
    } catch (const std::system_error& e) {
        logger.warn("Failed to restore SharedJSContext: {}", e.what());
    } catch (const std::exception& e) {
        logger.warn("Failed to restore SharedJSContext: {}", e.what());
    }

    logger.log("Restored SharedJSContext...");
#endif
}
