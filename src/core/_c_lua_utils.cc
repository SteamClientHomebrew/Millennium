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

#include <chrono>
#include <lua.hpp>
#include <thread>

int Lua_Sleep(lua_State* L)
{
    if (!lua_isnumber(L, 1)) {
        lua_pushstring(L, "Invalid argument to sleep. Expected number of milliseconds.");
        lua_error(L);
        return 0;
    }

    int milliseconds = static_cast<int>(lua_tointeger(L, 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    return 0;
}
static const luaL_Reg millennium_lib[] = {
    { "sleep", Lua_Sleep },
    { NULL,    NULL      }  // Sentinel
};

extern "C" int Lua_OpenUtilsLibrary(lua_State* L)
{
    luaL_newlib(L, millennium_lib);
    return 1;
}
