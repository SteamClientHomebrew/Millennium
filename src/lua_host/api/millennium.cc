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
#include "lua_api.h"
#include "millennium/types.h"
#include "millennium/star_parser.h"
#include <lua.hpp>
#include <algorithm>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

extern rpc_client* g_rpc;
extern std::string g_backend_dir;
extern std::string g_backend_file;
extern bool g_plugin_is_v2;
extern std::unordered_map<std::string, AssetEntry> g_asset_index;

static void push_json_to_lua(lua_State* L, const nlohmann::json& j)
{
    if (j.is_null()) {
        lua_pushnil(L);
    } else if (j.is_boolean()) {
        lua_pushboolean(L, j.get<bool>());
    } else if (j.is_number_integer()) {
        lua_pushinteger(L, j.get<lua_Integer>());
    } else if (j.is_number_float()) {
        lua_pushnumber(L, j.get<double>());
    } else if (j.is_string()) {
        lua_pushstring(L, j.get<std::string>().c_str());
    } else if (j.is_array()) {
        lua_newtable(L);
        int idx = 1;
        for (const auto& elem : j) {
            push_json_to_lua(L, elem);
            lua_rawseti(L, -2, idx++);
        }
    } else if (j.is_object()) {
        lua_newtable(L);
        for (auto it = j.begin(); it != j.end(); ++it) {
            lua_pushstring(L, it.key().c_str());
            push_json_to_lua(L, it.value());
            lua_rawset(L, -3);
        }
    } else {
        lua_pushnil(L);
    }
}

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
            switch (lua_type(L, -1)) {
                case LUA_TSTRING:
                    params.push_back(lua_tostring(L, -1));
                    break;
                case LUA_TBOOLEAN:
                    params.push_back(static_cast<bool>(lua_toboolean(L, -1)));
                    break;
                case LUA_TNUMBER:
                    params.push_back(lua_tonumber(L, -1));
                    break;
                default:
                    lua_pop(L, 1);
                    return luaL_error(L, "Millennium's IPC can only handle [boolean, string, number]");
            }
            lua_pop(L, 1);
        }
    }

    try {
        std::string caller;
        lua_Debug ar;
        if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar) && ar.currentline > 0) {
            const char* src = ar.source ? ar.source : "?";
            if (*src == '@') ++src;
            std::string path(src);
            const std::string prefix = g_backend_dir + "/";
            if (!prefix.empty() && path.substr(0, prefix.size()) == prefix) path = path.substr(prefix.size());
            caller = path + ":" + std::to_string(ar.currentline);
        }

        const json rpc_params = {
            { "methodName",      methodName     },
            { "params",          params         },
            { "caller",          caller         },
            { "return_by_value", g_plugin_is_v2 }
        };

        nlohmann::json result = g_rpc->call(plugin_ipc::child_method::CALL_FRONTEND_METHOD, rpc_params);

        if (!result.value("success", false)) {
            lua_pushnil(L);
            std::string err;
            if (result.contains("error") && result["error"].is_string()) {
                err = result["error"].get<std::string>();
            } else if (result.contains("returnJson") && result["returnJson"].is_string()) {
                err = result["returnJson"].get<std::string>();
            } else {
                err = "unknown error";
            }
            lua_pushstring(L, err.c_str());
            return 2;
        }

        const auto& response = result["returnJson"];
        std::string type = response.is_object() ? response.value("type", "") : "";

        if (type == "undefined" || type == "null") {
            lua_pushnil(L);
            return 1;
        }
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
        if (type == "object") {
            if (response.contains("value")) {
                push_json_to_lua(L, response["value"]);
                return 1;
            }
            lua_pushnil(L);
            lua_pushstring(L, "object return value is not JSON-serializable");
            return 2;
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
        lua_pushinteger(L, result.value("id", -1));
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
        lua_pushinteger(L, result.value("id", -1));
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
            { "hook_id", hookId }
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

static int RPC_StartCoroutine(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTHREAD);
    lua_State* co = lua_tothread(L, 1);
    g_rpc->enqueue_coroutine(co);
    return 0;
}

static int RPC_YieldReadable(lua_State* L)
{
    uintptr_t fd = static_cast<uintptr_t>(luaL_checkinteger(L, 1));
    if (lua_pushthread(L)) {
        lua_pop(L, 1);
        return luaL_error(L, "millennium.yield_readable() must be called from a coroutine, not the main thread");
    }
    lua_pop(L, 1);
    g_rpc->watch_fd(fd, L);
    return lua_yield(L, 0);
}

static int assets_read(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    auto it = g_asset_index.find(path);
    if (it == g_asset_index.end()) {
        lua_pushnil(L);
        return 1;
    }

    const AssetEntry& entry = it->second;
    std::ifstream f(g_backend_file, std::ios::binary);
    if (!f) {
        lua_pushnil(L);
        return 1;
    }

    f.seekg(static_cast<std::streamoff>(entry.file_offset));

    try {
        std::vector<uint8_t> compressed(entry.compressed_length);
        if (!f.read(reinterpret_cast<char*>(compressed.data()), static_cast<std::streamsize>(entry.compressed_length))) {
            lua_pushnil(L);
            return 1;
        }
        auto decompressed = star_decompress(compressed.data(), compressed.size());
        lua_pushlstring(L, reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
    } catch (const std::exception& e) {
        return luaL_error(L, "assets.read: decompress failed: %s", e.what());
    }
    return 1;
}

static int assets_size(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    auto it = g_asset_index.find(path);
    if (it == g_asset_index.end()) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(it->second.uncompressed_length));
    return 1;
}

static int assets_name(lua_State* L)
{
    const char* path_raw = luaL_checkstring(L, 1);
    std::string path(path_raw);

    while (!path.empty() && path.back() == '/')
        path.pop_back();

    const auto pos = path.rfind('/');
    const std::string name = (pos == std::string::npos) ? path : path.substr(pos + 1);
    lua_pushstring(L, name.c_str());
    return 1;
}

static int assets_type(lua_State* L)
{
    const char* path_raw = luaL_checkstring(L, 1);
    std::string path(path_raw);
    /* exact match -> file */
    if (g_asset_index.count(path)) {
        lua_pushstring(L, "file");
        return 1;
    }
    /* any key with path+"/" prefix -> directory */
    const std::string dir_prefix = path + "/";
    for (const auto& kv : g_asset_index) {
        if (kv.first.substr(0, dir_prefix.size()) == dir_prefix) {
            lua_pushstring(L, "directory");
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static int assets_list(lua_State* L)
{
    const char* dir_raw = luaL_optstring(L, 1, "");
    std::string dir(dir_raw);
    while (!dir.empty() && dir.back() == '/')
        dir.pop_back();

    const std::string prefix = dir.empty() ? "" : (dir + "/");

    std::vector<std::string> children;
    for (const auto& kv : g_asset_index) {
        const std::string& key = kv.first;
        if (!prefix.empty() && key.substr(0, prefix.size()) != prefix) continue;
        const std::string rel = key.substr(prefix.size());
        const auto slash = rel.find('/');
        if (slash == std::string::npos) {
            children.push_back(rel);
        } else {
            const std::string child_dir = rel.substr(0, slash + 1);
            if (std::find(children.begin(), children.end(), child_dir) == children.end()) children.push_back(child_dir);
        }
    }

    std::sort(children.begin(), children.end());

    lua_newtable(L);
    int idx = 1;
    for (const auto& name : children) {
        lua_pushstring(L, name.c_str());
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

static const luaL_Reg assets_lib[] = {
    { "read", assets_read },
    { "size", assets_size },
    { "name", assets_name },
    { "type", assets_type },
    { "list", assets_list },
    { NULL,   NULL        }
};

void register_assets_module(lua_State* L)
{
    lua_newtable(L);
    luaL_setfuncs(L, assets_lib, 0);
    lua_setfield(L, -2, "assets");
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
    { "start_coroutine",       RPC_StartCoroutine      },
    { "yield_readable",        RPC_YieldReadable       },
    { NULL,                    NULL                    }
};

extern "C" int luaopen_millennium_lib(lua_State* L)
{
    luaL_newlib(L, millennium_lib);
    register_config_module(L);
    register_assets_module(L);
    return 1;
}
