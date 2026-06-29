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
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern rpc_client* g_rpc;
extern bool g_plugin_is_v2;

static uint64_t now_us()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

void log_runtime_error(const std::string& message)
{
    fprintf(stderr, "[lua-host] %s\n", message.c_str());
    if (!g_rpc) return;

    const nlohmann::json params = {
        { "level",        "error"        },
        { "message",      message        },
        { "v2",           g_plugin_is_v2 },
        { "timestamp_us", now_us()       }
    };
    g_rpc->notify(plugin_ipc::child_method::LOG, params);
}

static bool parse_lua_loc(const char* err, std::string& out_file, int& out_line)
{
    if (!err) return false;

    for (const char* p = err; *p; ++p) {
        if (*p != ':') continue;
        const char* q = p + 1;
        if (*q < '0' || *q > '9') continue;
        const char* r = q;
        while (*r >= '0' && *r <= '9')
            ++r;
        if (*r != ':') continue;

        const char* src_start = err;
        size_t src_len = static_cast<size_t>(p - err);

        if (src_len >= 2 && src_start[0] == '(' && src_start[src_len - 1] == ')') {
            src_start++;
            src_len -= 2;
        }

        const char* base = src_start;
        for (size_t i = 0; i < src_len; ++i) {
            if (src_start[i] == '/' || src_start[i] == '\\') base = src_start + i + 1;
        }
        size_t base_len = src_len - static_cast<size_t>(base - src_start);
        if (base_len == 0) return false;

        out_file = std::string(base, base_len);
        out_line = std::atoi(q);
        return out_line > 0;
    }
    return false;
}

void log_lua_error(const char* context, const char* lua_err)
{
    const char* safe_err = lua_err ? lua_err : "unknown";
    std::string message = std::string(context) + ": " + safe_err;
    fprintf(stderr, "[lua-host] %s\n", message.c_str());
    if (!g_rpc) return;

    std::string src_file;
    int src_line = 0;
    parse_lua_loc(safe_err, src_file, src_line);

    nlohmann::json params = {
        { "level",        "error"        },
        { "message",      message        },
        { "v2",           g_plugin_is_v2 },
        { "timestamp_us", now_us()       }
    };
    if (!src_file.empty()) {
        params["file"] = src_file;
        params["line"] = src_line;
    }
    g_rpc->notify(plugin_ipc::child_method::LOG, params);
}

static void send_log(lua_State* L, const char* level, const char* message)
{
    if (!g_rpc) return;

    std::string src_file;
    int src_line = 0;
    lua_Debug ar{};
    if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar)) {
        if (ar.source) {
            const char* raw = (*ar.source == '@' || *ar.source == '=') ? ar.source + 1 : nullptr;
            if (raw) {
                std::string name(raw);
                if (name.size() >= 2 && name.front() == '(' && name.back() == ')') name = name.substr(1, name.size() - 2);
                const size_t slash = name.rfind('/');
                const size_t bslash = name.rfind('\\');
                const size_t start =
                    (slash != std::string::npos ? slash + 1 : 0) > (bslash != std::string::npos ? bslash + 1 : 0) ? slash + 1 : (bslash != std::string::npos ? bslash + 1 : 0);
                src_file = name.substr(start);
            }
        }
        src_line = ar.currentline;
    }

    const nlohmann::json params = {
        { "level",        level          },
        { "message",      message        },
        { "v2",           g_plugin_is_v2 },
        { "timestamp_us", now_us()       },
        { "file",         src_file       },
        { "line",         src_line       }
    };

    g_rpc->notify(plugin_ipc::child_method::LOG, params);
}

static int LuaLogInfo(lua_State* L)
{
    const char* message = luaL_checkstring(L, 2);
    send_log(L, "info", message);
    return 0;
}

static int LuaLogWarn(lua_State* L)
{
    const char* message = luaL_checkstring(L, 2);
    send_log(L, "warn", message);
    return 0;
}

static int LuaLogError(lua_State* L)
{
    const char* message = luaL_checkstring(L, 2);
    send_log(L, "error", message);
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
