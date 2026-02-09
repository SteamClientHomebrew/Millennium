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

#include <filesystem>
#include <lua.hpp>
#include <system_error>

namespace fs = std::filesystem;

int Lua_Exists(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    lua_pushboolean(L, fs::exists(path, ec) && !ec);
    return 1;
}

int Lua_IsDirectory(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    lua_pushboolean(L, fs::is_directory(path, ec) && !ec);
    return 1;
}

int Lua_IsFile(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    lua_pushboolean(L, fs::is_regular_file(path, ec) && !ec);
    return 1;
}

int Lua_IsSymlink(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    lua_pushboolean(L, fs::is_symlink(path, ec) && !ec);
    return 1;
}

int Lua_IsEmpty(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    lua_pushboolean(L, fs::is_empty(path, ec) && !ec);
    return 1;
}

int Lua_CreateDirectory(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    const bool result = fs::create_directory(path, ec);
    lua_pushboolean(L, result);
    if (ec) {
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    return 1;
}

int Lua_CreateDirectories(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    const bool result = fs::create_directories(path, ec);
    lua_pushboolean(L, result);
    if (ec) {
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    return 1;
}

int Lua_Remove(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    const bool result = fs::remove(path, ec);
    lua_pushboolean(L, result);
    if (ec) {
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    return 1;
}

int Lua_RemoveAll(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    const uintmax_t count = fs::remove_all(path, ec);
    lua_pushinteger(L, count);
    if (ec) {
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    return 1;
}

int Lua_ListDirectory(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;

    lua_newtable(L);
    int index = 1;

    for (const auto& entry : fs::directory_iterator(path, ec)) {
        lua_newtable(L);

        lua_pushstring(L, entry.path().filename().string().c_str());
        lua_setfield(L, -2, "name");

        lua_pushstring(L, entry.path().string().c_str());
        lua_setfield(L, -2, "path");

        lua_pushboolean(L, entry.is_directory());
        lua_setfield(L, -2, "is_directory");

        lua_pushboolean(L, entry.is_regular_file());
        lua_setfield(L, -2, "is_file");

        lua_pushboolean(L, entry.is_symlink());
        lua_setfield(L, -2, "is_symlink");

        if (entry.is_regular_file()) {
            std::error_code size_ec;
            const auto size = entry.file_size(size_ec);
            if (!size_ec) {
                lua_pushinteger(L, size);
                lua_setfield(L, -2, "size");
            }
        }

        lua_rawseti(L, -2, index++);
    }

    if (ec) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }

    return 1;
}

int Lua_ListDirectoryRecursive(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;

    lua_newtable(L);
    int index = 1;

    fs::recursive_directory_iterator it(path, ec);
    const fs::recursive_directory_iterator end;
    for (; it != end; ++it) {
        const auto& entry = *it;
        lua_newtable(L);

        lua_pushstring(L, entry.path().filename().string().c_str());
        lua_setfield(L, -2, "name");

        lua_pushstring(L, entry.path().string().c_str());
        lua_setfield(L, -2, "path");

        lua_pushboolean(L, entry.is_directory());
        lua_setfield(L, -2, "is_directory");

        lua_pushboolean(L, entry.is_regular_file());
        lua_setfield(L, -2, "is_file");

        lua_pushinteger(L, it.depth());
        lua_setfield(L, -2, "depth");

        lua_rawseti(L, -2, index++);
    }

    if (ec) {
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }

    return 1;
}

int Lua_Copy(lua_State* L)
{
    const char* from = luaL_checkstring(L, 1);
    const char* to = luaL_checkstring(L, 2);
    const bool throw_error = lua_toboolean(L, 3);
    std::error_code ec;
    fs::copy(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        if (throw_error) {
            lua_pushstring(L, ec.message().c_str());
            lua_error(L);
        }
        lua_pushnil(L);
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    lua_pushboolean(L, true);
    return 1;
}

int Lua_CopyRecursive(lua_State* L)
{
    const char* from = luaL_checkstring(L, 1);
    const char* to = luaL_checkstring(L, 2);
    const bool throw_error = lua_toboolean(L, 3);
    std::error_code ec;
    fs::copy(from, to, fs::copy_options::overwrite_existing | fs::copy_options::recursive, ec);
    if (ec) {
        if (throw_error) {
            lua_pushstring(L, ec.message().c_str());
            lua_error(L);
        }
        lua_pushnil(L);
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    lua_pushboolean(L, true);
    return 1;
}

int Lua_Rename(lua_State* L)
{
    const char* from = luaL_checkstring(L, 1);
    const char* to = luaL_checkstring(L, 2);
    const bool throw_error = lua_toboolean(L, 3);
    std::error_code ec;
    fs::rename(from, to, ec);
    if (ec) {
        if (throw_error) {
            lua_pushstring(L, ec.message().c_str());
            lua_error(L);
        }
        lua_pushnil(L);
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    lua_pushboolean(L, true);
    return 1;
}

int Lua_FileSize(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec) {
        lua_pushnil(L);
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    lua_pushinteger(L, size);
    return 1;
}

int Lua_LastWriteTime(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    const auto ftime = fs::last_write_time(path, ec);
    if (ec) {
        lua_pushnil(L);
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    const auto time = std::chrono::system_clock::to_time_t(sctp);
    lua_pushinteger(L, time);
    return 1;
}

int Lua_CurrentPath(lua_State* L)
{
    std::error_code ec;
    const auto path = fs::current_path(ec);
    if (ec) {
        lua_pushnil(L);
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    lua_pushstring(L, path.string().c_str());
    return 1;
}

int Lua_SetCurrentPath(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    fs::current_path(path, ec);
    lua_pushboolean(L, !ec);
    if (ec) {
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    return 1;
}

int Lua_Absolute(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    const auto abs = fs::absolute(path, ec);
    if (ec) {
        lua_pushnil(L);
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    lua_pushstring(L, abs.string().c_str());
    return 1;
}

int Lua_Canonical(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    const auto canonical = fs::canonical(path, ec);
    if (ec) {
        lua_pushnil(L);
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    lua_pushstring(L, canonical.string().c_str());
    return 1;
}

int Lua_Relative(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const char* base = lua_isstring(L, 2) ? lua_tostring(L, 2) : nullptr;
    std::error_code ec;
    const auto rel = base ? fs::relative(path, base, ec) : fs::relative(path, ec);
    if (ec) {
        lua_pushnil(L);
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    lua_pushstring(L, rel.string().c_str());
    return 1;
}

int Lua_Filename(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const fs::path p(path);
    lua_pushstring(L, p.filename().string().c_str());
    return 1;
}

int Lua_Extension(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const fs::path p(path);
    lua_pushstring(L, p.extension().string().c_str());
    return 1;
}

int Lua_Stem(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const fs::path p(path);
    lua_pushstring(L, p.stem().string().c_str());
    return 1;
}

int Lua_ParentPath(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const fs::path p(path);
    lua_pushstring(L, p.parent_path().string().c_str());
    return 1;
}

int Lua_JoinPath(lua_State* L)
{
    const int n = lua_gettop(L);
    fs::path result;
    for (int i = 1; i <= n; i++) {
        result /= luaL_checkstring(L, i);
    }
    lua_pushstring(L, result.string().c_str());
    return 1;
}

int Lua_SpaceInfo(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::error_code ec;
    const auto [capacity, free, available] = fs::space(path, ec);
    if (ec) {
        lua_pushnil(L);
        lua_pushstring(L, ec.message().c_str());
        return 2;
    }
    lua_newtable(L);
    lua_pushinteger(L, capacity);
    lua_setfield(L, -2, "capacity");
    lua_pushinteger(L, free);
    lua_setfield(L, -2, "free");
    lua_pushinteger(L, available);
    lua_setfield(L, -2, "available");
    return 1;
}

static const luaL_Reg fs_lib[] = {
    // Path queries
    { "exists",             Lua_Exists                 },
    { "is_directory",       Lua_IsDirectory            },
    { "is_file",            Lua_IsFile                 },
    { "is_symlink",         Lua_IsSymlink              },
    { "is_empty",           Lua_IsEmpty                },

    // Directory operations
    { "create_directory",   Lua_CreateDirectory        },
    { "create_directories", Lua_CreateDirectories      },
    { "remove",             Lua_Remove                 },
    { "remove_all",         Lua_RemoveAll              },
    { "list",               Lua_ListDirectory          },
    { "list_recursive",     Lua_ListDirectoryRecursive },

    // File operations
    { "copy",               Lua_Copy                   },
    { "copy_recursive",     Lua_CopyRecursive          },
    { "rename",             Lua_Rename                 },
    { "file_size",          Lua_FileSize               },
    { "last_write_time",    Lua_LastWriteTime          },

    // Path operations
    { "current_path",       Lua_CurrentPath            },
    { "set_current_path",   Lua_SetCurrentPath         },
    { "absolute",           Lua_Absolute               },
    { "canonical",          Lua_Canonical              },
    { "relative",           Lua_Relative               },
    { "filename",           Lua_Filename               },
    { "extension",          Lua_Extension              },
    { "stem",               Lua_Stem                   },
    { "parent_path",        Lua_ParentPath             },
    { "join",               Lua_JoinPath               },

    // Space info
    { "space_info",         Lua_SpaceInfo              },

    { nullptr,                 nullptr                       }
};

extern "C" int Lua_OpenFS(lua_State* L)
{
    luaL_newlib(L, fs_lib);
    return 1;
}
