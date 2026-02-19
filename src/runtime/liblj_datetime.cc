/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2023 - 2026. Project Millennium
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

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <lua.hpp>
#include <sstream>

using namespace std::chrono;

static int datetime_now(lua_State* L)
{
    const auto now = system_clock::now();
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
    lua_pushinteger(L, ms);
    return 1;
}

static int datetime_unix(lua_State* L)
{
    const auto now = system_clock::now();
    const auto sec = duration_cast<seconds>(now.time_since_epoch()).count();
    lua_pushinteger(L, sec);
    return 1;
}

static int datetime_from_unix(lua_State* L)
{
    const lua_Integer sec = luaL_checkinteger(L, 1);
    lua_pushinteger(L, sec * 1000);
    return 1;
}

static int datetime_to_unix(lua_State* L)
{
    const lua_Integer ms = luaL_checkinteger(L, 1);
    lua_pushinteger(L, ms / 1000);
    return 1;
}

static int datetime_format(lua_State* L)
{
    const lua_Integer ms = luaL_checkinteger(L, 1);
    const char* fmt = luaL_optstring(L, 2, "%Y-%m-%d %H:%M:%S");
    const bool utc = lua_toboolean(L, 3);

    const auto tp = system_clock::time_point(milliseconds(ms));
    const auto t = system_clock::to_time_t(tp);

    std::stringstream ss;
    ss << std::put_time(utc ? std::gmtime(&t) : std::localtime(&t), fmt);

    lua_pushstring(L, ss.str().c_str());
    return 1;
}

static int datetime_parse(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    const char* fmt = luaL_optstring(L, 2, "%Y-%m-%d %H:%M:%S");

    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, fmt);

    if (ss.fail()) {
        lua_pushnil(L);
        lua_pushstring(L, "Parse failed");
        return 2;
    }

    const auto tp = system_clock::from_time_t(std::mktime(&tm));
    const auto ms = duration_cast<milliseconds>(tp.time_since_epoch()).count();

    lua_pushinteger(L, ms);
    return 1;
}

static int datetime_add(lua_State* L)
{
    const lua_Integer ms = luaL_checkinteger(L, 1);
    const lua_Integer delta = luaL_checkinteger(L, 2);
    const char* unit = luaL_optstring(L, 3, "seconds");

    lua_Integer mult = 1000;
    if (strcmp(unit, "minutes") == 0)
        mult = 60000;
    else if (strcmp(unit, "hours") == 0)
        mult = 3600000;
    else if (strcmp(unit, "days") == 0)
        mult = 86400000;
    else if (strcmp(unit, "seconds") == 0)
        mult = 1000;
    else if (strcmp(unit, "milliseconds") == 0)
        mult = 1;

    lua_pushinteger(L, ms + delta * mult);
    return 1;
}

static int datetime_diff(lua_State* L)
{
    const lua_Integer ms1 = luaL_checkinteger(L, 1);
    const lua_Integer ms2 = luaL_checkinteger(L, 2);
    const char* unit = luaL_optstring(L, 3, "seconds");

    const lua_Integer diff_ms = ms1 - ms2;
    lua_Integer result = diff_ms / 1000;

    if (strcmp(unit, "minutes") == 0)
        result = diff_ms / 60000;
    else if (strcmp(unit, "hours") == 0)
        result = diff_ms / 3600000;
    else if (strcmp(unit, "days") == 0)
        result = diff_ms / 86400000;
    else if (strcmp(unit, "milliseconds") == 0)
        result = diff_ms;

    lua_pushinteger(L, result);
    return 1;
}

static int datetime_components(lua_State* L)
{
    const lua_Integer ms = luaL_checkinteger(L, 1);
    const bool utc = lua_toboolean(L, 2);

    const auto tp = system_clock::time_point(milliseconds(ms));
    const auto t = system_clock::to_time_t(tp);
    const std::tm* tm = utc ? std::gmtime(&t) : std::localtime(&t);

    lua_createtable(L, 0, 7);
    lua_pushinteger(L, tm->tm_year + 1900);
    lua_setfield(L, -2, "year");
    lua_pushinteger(L, tm->tm_mon + 1);
    lua_setfield(L, -2, "month");
    lua_pushinteger(L, tm->tm_mday);
    lua_setfield(L, -2, "day");
    lua_pushinteger(L, tm->tm_hour);
    lua_setfield(L, -2, "hour");
    lua_pushinteger(L, tm->tm_min);
    lua_setfield(L, -2, "minute");
    lua_pushinteger(L, tm->tm_sec);
    lua_setfield(L, -2, "second");
    lua_pushinteger(L, tm->tm_wday);
    lua_setfield(L, -2, "weekday");

    return 1;
}

static int datetime_create(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    std::tm tm = {};

    lua_getfield(L, 1, "year");
    tm.tm_year = lua_tointeger(L, -1) - 1900;
    lua_pop(L, 1);

    lua_getfield(L, 1, "month");
    tm.tm_mon = lua_tointeger(L, -1) - 1;
    lua_pop(L, 1);

    lua_getfield(L, 1, "day");
    tm.tm_mday = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "hour");
    tm.tm_hour = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "minute");
    tm.tm_min = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "second");
    tm.tm_sec = lua_tointeger(L, -1);
    lua_pop(L, 1);

    const auto tp = system_clock::from_time_t(std::mktime(&tm));
    const auto ms = duration_cast<milliseconds>(tp.time_since_epoch()).count();

    lua_pushinteger(L, ms);
    return 1;
}

static const luaL_Reg datetime_funcs[] = {
    { "now",        datetime_now        },
    { "unix",       datetime_unix       },
    { "from_unix",  datetime_from_unix  },
    { "to_unix",    datetime_to_unix    },
    { "format",     datetime_format     },
    { "parse",      datetime_parse      },
    { "add",        datetime_add        },
    { "diff",       datetime_diff       },
    { "components", datetime_components },
    { "create",     datetime_create     },
    { nullptr,      nullptr             }
};

extern "C" int Lua_OpenDateTime(lua_State* L)
{
    luaL_newlib(L, datetime_funcs);
    return 1;
}
