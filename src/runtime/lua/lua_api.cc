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

#include "head/webkit.h"
#include "millennium/config.h"
#include "millennium/plugin_loader.h"
#include "millennium/life_cycle.h"
#include "millennium/core_ipc.h"
#include "millennium/http_hooks.h"
#include "millennium/logger.h"
#include "millennium/semver.h"
#include "millennium/filesystem.h"
#include <exception>
#include <fmt/core.h>
#include <lua.hpp>
#include <nlohmann/json.hpp>

/**
 * RAII-style Lua stack guard to ensure stack cleanup in all code paths
 */
class LuaStackGuard
{
  private:
    lua_State* L;
    int top;

  public:
    LuaStackGuard(lua_State* L) : L(L), top(lua_gettop(L))
    {
    }
    ~LuaStackGuard()
    {
        lua_settop(L, top);
    }

    // Disable copy constructor and assignment to prevent misuse
    LuaStackGuard(const LuaStackGuard&) = delete;
    LuaStackGuard& operator=(const LuaStackGuard&) = delete;
};

/**
 * Helper function to safely get plugin name from Lua global
 * Returns empty string if not found or error occurs
 */
static std::string GetPluginNameSafe(lua_State* L)
{
    LuaStackGuard guard(L);

    lua_getglobal(L, "MILLENNIUM_PLUGIN_SECRET_NAME");
    if (lua_isnil(L, -1)) {
        LOG_ERROR("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        return "";
    }

    const char* pluginNameStr = lua_tostring(L, -1);
    return pluginNameStr ? std::string(pluginNameStr) : std::string();
}

int Lua_GetUserSettings(lua_State* L)
{
    return luaL_error(L, "get_user_settings is not implemented yet. It will likely be removed in the future.");
}

int Lua_SetUserSettings(lua_State* L)
{
    return luaL_error(L, "set_user_settings_key is not implemented yet. It will likely be removed in the future.");
}

int Lua_CallFrontendMethod(lua_State* L)
{
    const char* methodName = luaL_checkstring(L, 1);
    std::vector<ipc_main::javascript_parameter> params;

    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        if (!lua_istable(L, 2)) {
            return luaL_error(L, "params must be a table");
        }

        int len = (int)lua_objlen(L, 2);

        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, 2, i);
            ipc_main::javascript_parameter param;

            if (lua_isstring(L, -1)) {
                param = lua_tostring(L, -1);
            } else if (lua_isboolean(L, -1)) {
                param = lua_toboolean(L, -1);
            } else if (lua_isnumber(L, -1)) {
                /**
                 * LuaJIT only has 1 numerical type, being a double; so we don't have to worry about overflow.
                 * https://bitop.luajit.org/semantics.html
                 */
                param = lua_tonumber(L, -1);
            } else {
                lua_pop(L, 1);
                return luaL_error(L, "Millennium's IPC can only handle [boolean, string, number]");
            }
            params.push_back(param);
            lua_pop(L, 1);
        }
    }

    const std::string pluginName = GetPluginNameSafe(L);
    if (pluginName.empty()) {
        lua_pushnil(L);
        return 1;
    }

    std::shared_ptr<plugin_loader> loader = backend_initializer::get_plugin_loader_from_lua_vm(L);
    if (!loader) {
        return luaL_error(L, "Failed to contact Millennium's plugin loader, it's likely shut down.");
    }

    std::shared_ptr<ipc_main> ipc = loader->get_ipc_main();
    if (!ipc) {
        return luaL_error(L, "Failed to contact Millennium's IPC, it's likely shut down.");
    }

    const std::string script = ipc->compile_javascript_expression(pluginName, methodName, params);
    return ipc->evaluate_javascript_expression(script).to_lua(L);
}

int Lua_GetVersionInfo(lua_State* L)
{
    lua_pushstring(L, MILLENNIUM_VERSION);
    return 1;
}

int Lua_GetSteamPath(lua_State* L)
{
    try {
        const std::string steamPath = platform::get_steam_path().string();
        lua_pushstring(L, steamPath.c_str());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get Steam path: {}", e.what());
        lua_pushstring(L, ""); // Return empty string on error
    }
    return 1;
}

int Lua_GetInstallPath(lua_State* L)
{
    try {
        const std::string installPath = platform::get_install_path().string();
        lua_pushstring(L, installPath.c_str());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get install path: {}", e.what());
        lua_pushstring(L, ""); // Return empty string on error
    }
    return 1;
}

int Lua_RemoveBrowserModule(lua_State* L)
{
    const lua_Integer hookId = luaL_checkinteger(L, 1);

    std::shared_ptr<plugin_loader> loader = backend_initializer::get_plugin_loader_from_lua_vm(L);
    if (!loader) {
        return luaL_error(L, "Failed to contact Millennium's plugin loader, it's likely shut down.");
    }

    std::shared_ptr<millennium_backend> backend = loader->get_millennium_backend();
    if (!backend) {
        return luaL_error(L, "Failed to contact Millennium's backend, it's likely shut down.");
    }

    std::weak_ptr<theme_webkit_mgr> webkit_mgr_weak = backend->get_theme_webkit_mgr();
    if (auto webkit_mgr = webkit_mgr_weak.lock()) {
        const bool success = webkit_mgr->remove_browser_hook(static_cast<unsigned long long>(hookId));
        lua_pushboolean(L, success);
        return 1;
    }

    LOG_ERROR("Failed to lock theme_webkit_mgr, it likely shutdown.");
    return 0;
}

unsigned long long Lua_AddBrowserModule(lua_State* L, network_hook_ctl::TagTypes type)
{
    const char* content = luaL_checkstring(L, 1);
    const char* pattern = luaL_optstring(L, 2, ".*");

    std::shared_ptr<plugin_loader> loader = backend_initializer::get_plugin_loader_from_lua_vm(L);
    if (!loader) {
        return luaL_error(L, "Failed to contact Millennium's plugin loader, it's likely shut down.");
    }

    std::shared_ptr<millennium_backend> backend = loader->get_millennium_backend();
    if (!backend) {
        return luaL_error(L, "Failed to contact Millennium's backend it's likely shut down.");
    }

    std::weak_ptr<theme_webkit_mgr> webkit_mgr_weak = backend->get_theme_webkit_mgr();
    if (auto webkit_mgr = webkit_mgr_weak.lock()) {
        return webkit_mgr->add_browser_hook(content, pattern, type);
    }

    LOG_ERROR("Failed to lock theme_webkit_mgr, it likely shutdown.");
    return -1;
}

int Lua_AddBrowserCss(lua_State* L)
{
    const unsigned long long result = Lua_AddBrowserModule(L, network_hook_ctl::TagTypes::STYLESHEET);
    lua_pushinteger(L, static_cast<lua_Integer>(result));
    return 1;
}

int Lua_AddBrowserJs(lua_State* L)
{
    const unsigned long long result = Lua_AddBrowserModule(L, network_hook_ctl::TagTypes::JAVASCRIPT);
    lua_pushinteger(L, static_cast<lua_Integer>(result));
    return 1;
}

int Lua_IsPluginEnable(lua_State* L)
{
    const char* pluginName = luaL_checkstring(L, 1);
    if (!pluginName) {
        lua_pushboolean(L, 0);
        return 1;
    }

    std::shared_ptr<plugin_loader> loader = backend_initializer::get_plugin_loader_from_lua_vm(L);
    if (!loader) {
        return luaL_error(L, "Failed to contact Millennium's plugin loader, it's likely shut down.");
    }

    std::shared_ptr<settings_store> settings = loader->get_settings_store();
    if (!settings) {
        return luaL_error(L, "Failed to contact Millennium's settings store, it's likely shut down.");
    }

    try {
        const bool isEnabled = settings->is_enabled(pluginName);
        lua_pushboolean(L, isEnabled);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to check plugin status for '{}': {}", pluginName, e.what());
        lua_pushboolean(L, 0); // Default to disabled on error
    }
    return 1;
}

int Lua_EmitReadyMessage(lua_State* L)
{
    const std::string pluginName = GetPluginNameSafe(L);
    if (pluginName.empty()) {
        lua_pushboolean(L, 0);
        return 1;
    }

    std::shared_ptr<plugin_loader> loader = backend_initializer::get_plugin_loader_from_lua_vm(L);
    if (!loader) {
        return luaL_error(L, "Failed to contact Millennium's plugin loader, it's likely shut down.");
    }

    std::shared_ptr<backend_event_dispatcher> backend_dispatcher = loader->get_backend_event_dispatcher();
    if (!backend_dispatcher) {
        return luaL_error(L, "Failed to contact Millennium's backend event dispatcher, it's likely shut down.");
    }

    try {
        backend_dispatcher->backend_loaded_event_hdlr({ pluginName, backend_event_dispatcher::backend_ready_event::BACKEND_LOAD_SUCCESS });
        lua_pushboolean(L, 1);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to emit ready message for plugin '{}': {}", pluginName, e.what());
        lua_pushboolean(L, 0);
    }

    return 1;
}

int Lua_CompareVersion(lua_State* L)
{
    int compare;
    const char* v1 = luaL_checkstring(L, 1);
    const char* v2 = luaL_checkstring(L, 2);

    /** strip the leading v prefix if provided */
    if (v1[0] == 'v' || v1[0] == 'V') v1++;

    if (v2[0] == 'v' || v2[0] == 'V') v2++;

    try {
        compare = semver::cmp(v1, v2);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to compare versions [{}, {}]: {}", v1, v2, e.what());
        compare = -2; /** signify there was an error when comparing */
    }
    lua_pushinteger(L, compare);
    return 1;
}

/**
 * Lua library registration function for the Millennium module
 */
static const luaL_Reg millennium_lib[] = {
    { "ready",                 Lua_EmitReadyMessage    },
    { "add_browser_css",       Lua_AddBrowserCss       },
    { "add_browser_js",        Lua_AddBrowserJs        },
    { "remove_browser_module", Lua_RemoveBrowserModule },
    { "get_user_settings",     Lua_GetUserSettings     },
    { "set_user_settings_key", Lua_SetUserSettings     },
    { "version",               Lua_GetVersionInfo      },
    { "steam_path",            Lua_GetSteamPath        },
    { "get_install_path",      Lua_GetInstallPath      },
    { "call_frontend_method",  Lua_CallFrontendMethod  },
    { "is_plugin_enabled",     Lua_IsPluginEnable      },
    { "cmp_version",           Lua_CompareVersion      },
    { NULL,                    NULL                    }  // Sentinel
};

/**
 * Register the Millennium module with Lua
 */
extern "C" int luaopen_millennium_lib(lua_State* L)
{
    luaL_newlib(L, millennium_lib);
    return 1;
}
