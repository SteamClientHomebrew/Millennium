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

#include "hhx64/smem.h"
#include <lua.hpp>

void load_patches(lua_State* L, lb_shm_arena_t* map)
{
    lua_getglobal(L, "MILLENNIUM_PLUGIN_SECRET_NAME");
    if (!lua_isstring(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    const char* plugin_name = lua_tostring(L, -1);
    lua_pop(L, 1);

    lb_patch_list_shm_t* patch_list = hashmap_add_key(map, plugin_name);

    lua_getfield(L, -1, "patches");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    size_t patch_count = lua_objlen(L, -1);
    for (size_t i = 1; i <= patch_count; i++) {
        lua_rawgeti(L, -1, i);

        const char* file = nullptr;
        const char* find = nullptr;

        lua_getfield(L, -1, "file");
        if (lua_isstring(L, -1)) {
            file = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "find");
        if (lua_isstring(L, -1)) {
            find = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lb_patch_shm_t* patch = patchlist_add(g_lb_patch_arena, patch_list, file ? file : "", find ? find : "");

        lua_getfield(L, -1, "transforms");
        if (lua_istable(L, -1)) {
            size_t transform_count = lua_objlen(L, -1);
            for (size_t j = 1; j <= transform_count; j++) {
                lua_rawgeti(L, -1, j);

                const char* match = nullptr;
                const char* replace = nullptr;

                lua_getfield(L, -1, "match");
                if (lua_isstring(L, -1)) {
                    match = lua_tostring(L, -1);
                }
                lua_pop(L, 1);

                lua_getfield(L, -1, "replace");
                if (lua_isstring(L, -1)) {
                    replace = lua_tostring(L, -1);
                }
                lua_pop(L, 1);

                patch_add_transform(g_lb_patch_arena, patch, match ? match : "", replace ? replace : "");
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}
