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
#include <lua.hpp>
#include <string.h>

extern rpc_client* g_rpc;

/**
 * Builds an RPC params object from the Lua arguments and sends it to the parent
 * process which performs the actual curl request.
 */
static int Lua_HttpRequest(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);

    nlohmann::json params = {
        { "url", url }
    };

    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "method");
        if (lua_isstring(L, -1)) {
            params["method"] = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "data");
        if (lua_isstring(L, -1)) {
            params["data"] = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "timeout");
        if (lua_isnumber(L, -1)) {
            params["timeout"] = static_cast<long>(lua_tointeger(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "follow_redirects");
        if (lua_isboolean(L, -1)) {
            params["follow_redirects"] = static_cast<bool>(lua_toboolean(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "verify_ssl");
        if (lua_isboolean(L, -1)) {
            params["verify_ssl"] = static_cast<bool>(lua_toboolean(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "user_agent");
        if (lua_isstring(L, -1)) {
            params["user_agent"] = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "auth");
        if (lua_istable(L, -1)) {
            nlohmann::json auth;
            lua_getfield(L, -1, "user");
            if (lua_isstring(L, -1)) {
                auth["user"] = lua_tostring(L, -1);
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "pass");
            if (lua_isstring(L, -1)) {
                auth["pass"] = lua_tostring(L, -1);
            }
            lua_pop(L, 1);

            if (!auth.empty()) {
                params["auth"] = auth;
            }
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "proxy");
        if (lua_isstring(L, -1)) {
            params["proxy"] = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "headers");
        if (lua_istable(L, -1)) {
            nlohmann::json hdrs = nlohmann::json::object();
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                if (lua_isstring(L, -2) && lua_isstring(L, -1)) {
                    hdrs[lua_tostring(L, -2)] = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
            }
            if (!hdrs.empty()) {
                params["headers"] = hdrs;
            }
        }
        lua_pop(L, 1);
    }

    try {
        nlohmann::json result = g_rpc->call(plugin_ipc::child_method::HTTP_REQUEST, params);

        if (result.contains("error")) {
            lua_pushnil(L);
            lua_pushstring(L, result["error"].get<std::string>().c_str());
            return 2;
        }

        lua_newtable(L);

        lua_pushstring(L, "body");
        lua_pushstring(L, result.value("body", "").c_str());
        lua_settable(L, -3);

        lua_pushstring(L, "status");
        lua_pushinteger(L, result.value("status", 0));
        lua_settable(L, -3);

        lua_pushstring(L, "headers");
        lua_newtable(L);
        if (result.contains("headers") && result["headers"].is_object()) {
            for (auto& [key, val] : result["headers"].items()) {
                lua_pushstring(L, key.c_str());
                lua_pushstring(L, val.get<std::string>().c_str());
                lua_settable(L, -3);
            }
        }
        lua_settable(L, -3);

        return 1;
    } catch (const std::exception& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static int Lua_HttpGet(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);

    lua_pushstring(L, url);

    lua_newtable(L);
    lua_pushstring(L, "method");
    lua_pushstring(L, "GET");
    lua_settable(L, -3);

    if (lua_istable(L, 2)) {
        lua_pushnil(L);
        while (lua_next(L, 2) != 0) {
            lua_pushvalue(L, -2);
            lua_insert(L, -2);
            lua_settable(L, -4);
        }
    }

    return Lua_HttpRequest(L);
}

static int Lua_HttpPost(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);
    const char* data = NULL;

    if (lua_isstring(L, 2)) {
        data = lua_tostring(L, 2);
    }

    lua_pushstring(L, url);

    lua_newtable(L);
    lua_pushstring(L, "method");
    lua_pushstring(L, "POST");
    lua_settable(L, -3);

    if (data) {
        lua_pushstring(L, "data");
        lua_pushstring(L, data);
        lua_settable(L, -3);
    }

    if (lua_istable(L, 3)) {
        lua_pushnil(L);
        while (lua_next(L, 3) != 0) {
            lua_pushvalue(L, -2);
            lua_insert(L, -2);
            lua_settable(L, -4);
        }
    }

    return Lua_HttpRequest(L);
}

static int Lua_HttpPut(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);
    const char* data = NULL;

    if (lua_isstring(L, 2)) {
        data = lua_tostring(L, 2);
    }

    lua_pushstring(L, url);

    lua_newtable(L);
    lua_pushstring(L, "method");
    lua_pushstring(L, "PUT");
    lua_settable(L, -3);

    if (data) {
        lua_pushstring(L, "data");
        lua_pushstring(L, data);
        lua_settable(L, -3);
    }

    if (lua_istable(L, 3)) {
        lua_pushnil(L);
        while (lua_next(L, 3) != 0) {
            lua_pushvalue(L, -2);
            lua_insert(L, -2);
            lua_settable(L, -4);
        }
    }

    return Lua_HttpRequest(L);
}

static int Lua_HttpDelete(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);

    lua_pushstring(L, url);

    lua_newtable(L);
    lua_pushstring(L, "method");
    lua_pushstring(L, "DELETE");
    lua_settable(L, -3);

    if (lua_istable(L, 2)) {
        lua_pushnil(L);
        while (lua_next(L, 2) != 0) {
            lua_pushvalue(L, -2);
            lua_insert(L, -2);
            lua_settable(L, -4);
        }
    }

    return Lua_HttpRequest(L);
}

/**
 * http.download(url, path [, opts])
 * Streams a file to disk via RPC. No body size limit.
 * Returns { success, status, bytes_written } or nil, error.
 */
static int Lua_HttpDownload(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);
    const char* path = luaL_checkstring(L, 2);

    nlohmann::json params = {
        { "url",  url  },
        { "path", path }
    };

    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "timeout");
        if (lua_isnumber(L, -1)) {
            params["timeout"] = static_cast<long>(lua_tointeger(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "follow_redirects");
        if (lua_isboolean(L, -1)) {
            params["follow_redirects"] = static_cast<bool>(lua_toboolean(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "verify_ssl");
        if (lua_isboolean(L, -1)) {
            params["verify_ssl"] = static_cast<bool>(lua_toboolean(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "user_agent");
        if (lua_isstring(L, -1)) {
            params["user_agent"] = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "headers");
        if (lua_istable(L, -1)) {
            nlohmann::json hdrs = nlohmann::json::object();
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                if (lua_isstring(L, -2) && lua_isstring(L, -1)) {
                    hdrs[lua_tostring(L, -2)] = lua_tostring(L, -1);
                }
                lua_pop(L, 1);
            }
            if (!hdrs.empty()) {
                params["headers"] = hdrs;
            }
        }
        lua_pop(L, 1);
    }

    try {
        nlohmann::json result = g_rpc->call(plugin_ipc::child_method::HTTP_DOWNLOAD, params);

        if (result.contains("error")) {
            lua_pushnil(L);
            lua_pushstring(L, result["error"].get<std::string>().c_str());
            return 2;
        }

        lua_newtable(L);

        lua_pushstring(L, "success");
        lua_pushboolean(L, result.value("success", false));
        lua_settable(L, -3);

        lua_pushstring(L, "status");
        lua_pushinteger(L, result.value("status", 0));
        lua_settable(L, -3);

        lua_pushstring(L, "bytes_written");
        lua_pushinteger(L, result.value("bytes_written", 0));
        lua_settable(L, -3);

        return 1;
    } catch (const std::exception& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static const luaL_Reg httpFunctions[] = {
    { "request",  Lua_HttpRequest  },
    { "get",      Lua_HttpGet      },
    { "post",     Lua_HttpPost     },
    { "put",      Lua_HttpPut      },
    { "delete",   Lua_HttpDelete   },
    { "download", Lua_HttpDownload },
    { NULL,       NULL             }
};

extern "C" int luaopen_http_lib(lua_State* L)
{
    luaL_newlib(L, httpFunctions);
    return 1;
}
