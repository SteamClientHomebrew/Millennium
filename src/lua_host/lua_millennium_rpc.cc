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

#include "rpc.h"
#include "lua_config_rpc.h"
#include "millennium/types.h"
#include <lua.hpp>

extern rpc_client* g_rpc;

static int RPC_CallFrontendMethod(lua_State* L)
{
    const char* methodName = luaL_checkstring(L, 1);
    nlohmann::json params = nlohmann::json::array();

    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        if (!lua_istable(L, 2)) {
            return luaL_error(L, "params must be a table");
        }

        int len = static_cast<int>(lua_objlen(L, 2));
        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, 2, i);
            if (lua_isstring(L, -1)) {
                params.push_back(lua_tostring(L, -1));
            } else if (lua_isboolean(L, -1)) {
                params.push_back(static_cast<bool>(lua_toboolean(L, -1)));
            } else if (lua_isnumber(L, -1)) {
                params.push_back(lua_tonumber(L, -1));
            } else {
                lua_pop(L, 1);
                return luaL_error(L, "Millennium's IPC can only handle [boolean, string, number]");
            }
            lua_pop(L, 1);
        }
    }

    try {
        const json params = {
            { "methodName", methodName },
            { "params",     params     }
        };

        nlohmann::json result = g_rpc->call(plugin_ipc::child_method::CALL_FRONTEND_METHOD, params);

        if (!result.value("success", false)) {
            lua_pushnil(L);
            lua_pushstring(L, result.value("error", "unknown error").c_str());
            return 2;
        }

        const auto& response = result["returnJson"];
        std::string type = response.value("type", "");

        if (type == "string") {
            lua_pushstring(L, response["value"].get<std::string>().c_str());
            return 1;
        }
        if (type == "boolean") {
            lua_pushboolean(L, response["value"].get<bool>());
            return 1;
        }
        if (type == "number") {
            lua_pushnumber(L, response["value"].get<double>());
            return 1;
        }

        lua_pushnil(L);
        lua_pushstring(L, ("unaccepted return type: " + type).c_str());
        return 2;

    } catch (const std::exception& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static int RPC_EmitReadyMessage(lua_State* L)
{
    try {
        g_rpc->call(plugin_ipc::child_method::READY);
        lua_pushboolean(L, 1);
    } catch (const std::exception& e) {
        fprintf(stderr, "[lua-host] ready() failed: %s\n", e.what());
        lua_pushboolean(L, 0);
    }
    return 1;
}

static int RPC_AddBrowserCss(lua_State* L)
{
    const char* content = luaL_checkstring(L, 1);
    const char* pattern = luaL_optstring(L, 2, ".*");

    try {
        const json params = {
            { "content", content },
            { "pattern", pattern }
        };

        auto result = g_rpc->call(plugin_ipc::child_method::ADD_BROWSER_CSS, params);
        lua_pushinteger(L, result.value("hookId", -1));
    } catch (const std::exception& e) {
        return luaL_error(L, "add_browser_css failed: %s", e.what());
    }
    return 1;
}

static int RPC_AddBrowserJs(lua_State* L)
{
    const char* content = luaL_checkstring(L, 1);
    const char* pattern = luaL_optstring(L, 2, ".*");

    try {
        const json params = {
            { "content", content },
            { "pattern", pattern }
        };

        auto result = g_rpc->call(plugin_ipc::child_method::ADD_BROWSER_JS, params);
        lua_pushinteger(L, result.value("hookId", -1));
    } catch (const std::exception& e) {
        return luaL_error(L, "add_browser_js failed: %s", e.what());
    }
    return 1;
}

static int RPC_RemoveBrowserModule(lua_State* L)
{
    lua_Integer hookId = luaL_checkinteger(L, 1);

    try {
        const json params = {
            { "hookId", hookId }
        };

        auto result = g_rpc->call(plugin_ipc::child_method::REMOVE_BROWSER_MODULE, params);
        lua_pushboolean(L, result.value("success", false));
    } catch (const std::exception& e) {
        return luaL_error(L, "remove_browser_module failed: %s", e.what());
    }
    return 1;
}

static int RPC_GetVersionInfo(lua_State* L)
{
    try {
        auto result = g_rpc->call(plugin_ipc::child_method::VERSION);
        lua_pushstring(L, result.value("version", "").c_str());
    } catch (...) {
        lua_pushstring(L, "unknown");
    }
    return 1;
}

static int RPC_GetSteamPath(lua_State* L)
{
    try {
        auto result = g_rpc->call(plugin_ipc::child_method::STEAM_PATH);
        lua_pushstring(L, result.value("path", "").c_str());
    } catch (...) {
        lua_pushstring(L, "");
    }
    return 1;
}

static int RPC_GetInstallPath(lua_State* L)
{
    try {
        auto result = g_rpc->call(plugin_ipc::child_method::INSTALL_PATH);
        lua_pushstring(L, result.value("path", "").c_str());
    } catch (...) {
        lua_pushstring(L, "");
    }
    return 1;
}

static int RPC_IsPluginEnabled(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    try {
        const json params = {
            { "name", name }
        };

        auto result = g_rpc->call(plugin_ipc::child_method::IS_PLUGIN_ENABLED, params);
        lua_pushboolean(L, result.value("enabled", false));
    } catch (...) {
        lua_pushboolean(L, 0);
    }
    return 1;
}

static int RPC_CompareVersion(lua_State* L)
{
    const char* v1 = luaL_checkstring(L, 1);
    const char* v2 = luaL_checkstring(L, 2);

    try {
        const json params = {
            { "v1", v1 },
            { "v2", v2 }
        };

        auto result = g_rpc->call(plugin_ipc::child_method::CMP_VERSION, params);
        lua_pushinteger(L, result.value("result", -2));
    } catch (...) {
        lua_pushinteger(L, -2);
    }
    return 1;
}

static const luaL_Reg millennium_lib[] = {
    { "ready",                 RPC_EmitReadyMessage    },
    { "add_browser_css",       RPC_AddBrowserCss       },
    { "add_browser_js",        RPC_AddBrowserJs        },
    { "remove_browser_module", RPC_RemoveBrowserModule },
    { "version",               RPC_GetVersionInfo      },
    { "steam_path",            RPC_GetSteamPath        },
    { "get_install_path",      RPC_GetInstallPath      },
    { "call_frontend_method",  RPC_CallFrontendMethod  },
    { "is_plugin_enabled",     RPC_IsPluginEnabled     },
    { "cmp_version",           RPC_CompareVersion      },
    { NULL,                    NULL                    }
};

extern "C" int luaopen_millennium_lib(lua_State* L)
{
    luaL_newlib(L, millennium_lib);
    register_config_module(L);
    return 1;
}
