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

extern rpc_client* g_rpc;

static void send_log(const char* level, const char* message)
{
    if (!g_rpc) return;

    g_rpc->notify(plugin_ipc::child_method::LOG, {
                                                     { "level",   level   },
                                                     { "message", message }
    });
}

static int LuaLogInfo(lua_State* L)
{
    const char* message = luaL_checkstring(L, 2);
    send_log("info", message);
    return 0;
}

static int LuaLogWarn(lua_State* L)
{
    const char* message = luaL_checkstring(L, 2);
    send_log("warn", message);
    return 0;
}

static int LuaLogError(lua_State* L)
{
    const char* message = luaL_checkstring(L, 2);
    send_log("error", message);
    return 0;
}

static const luaL_Reg loggerFunctions[] = {
    { "info",  LuaLogInfo  },
    { "warn",  LuaLogWarn  },
    { "error", LuaLogError },
    { NULL,    NULL        }
};

extern "C" int luaopen_logger_lib(lua_State* L)
{
    luaL_newlib(L, loggerFunctions);
    return 1;
}
