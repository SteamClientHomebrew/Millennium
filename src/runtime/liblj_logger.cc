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

#include "millennium/logger.h"
#include "millennium/plugin_logger.h"
#include <lua.hpp>
#include <vector>

extern std::vector<BackendLogger*> g_loggerList;

static BackendLogger* GetBackendLogger(lua_State* L)
{
    lua_getfield(L, 1, "__backend_logger");
    BackendLogger* backend = static_cast<BackendLogger*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return backend;
}

static int LuaLogInfo(lua_State* L)
{
    BackendLogger* backend = GetBackendLogger(L);
    const char* message = luaL_checkstring(L, 2);
    if (backend)
        backend->Log(message);
    return 0;
}

static int LuaLogWarn(lua_State* L)
{
    BackendLogger* backend = GetBackendLogger(L);
    const char* message = luaL_checkstring(L, 2);
    if (backend)
        backend->Warn(message);
    return 0;
}

static int LuaLogError(lua_State* L)
{
    BackendLogger* backend = GetBackendLogger(L);
    const char* message = luaL_checkstring(L, 2);
    if (backend)
        backend->Error(message);
    return 0;
}

static const luaL_Reg loggerFunctions[] = {
    { "info",  LuaLogInfo  },
    { "warn",  LuaLogWarn  },
    { "error", LuaLogError },
    { NULL,    NULL        }
};

extern "C" int Lua_OpenLoggerLibrary(lua_State* L)
{
    const char* pluginName = nullptr;
    lua_getglobal(L, "MILLENNIUM_PLUGIN_SECRET_NAME");
    if (lua_isstring(L, -1)) {
        pluginName = lua_tostring(L, -1);
    }
    lua_pop(L, 1);

    BackendLogger* backend = new BackendLogger(pluginName);
    g_loggerList.push_back(backend);

    luaL_newlib(L, loggerFunctions);

    lua_pushlightuserdata(L, backend);
    lua_setfield(L, -2, "__backend_logger");
    return 1;
}