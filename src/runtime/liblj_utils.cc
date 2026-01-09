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
#include <lua.hpp>
#include <thread>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <ctime>

int Lua_Sleep(lua_State* L)
{
    int milliseconds = static_cast<int>(luaL_checkinteger(L, 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    return 0;
}

int Lua_GetTime(lua_State* L)
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    lua_pushnumber(L, ms.count() / 1000.0);
    return 1;
}

int Lua_GetTimeMs(lua_State* L)
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    lua_pushinteger(L, ms.count());
    return 1;
}

int Lua_GetTimeMicro(lua_State* L)
{
    auto now = std::chrono::system_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    lua_pushinteger(L, us.count());
    return 1;
}

// ============= Math =============

int Lua_Round(lua_State* L)
{
    double num = luaL_checknumber(L, 1);
    lua_pushnumber(L, std::round(num));
    return 1;
}

int Lua_Clamp(lua_State* L)
{
    double value = luaL_checknumber(L, 1);
    double min = luaL_checknumber(L, 2);
    double max = luaL_checknumber(L, 3);
    lua_pushnumber(L, std::clamp(value, min, max));
    return 1;
}

int Lua_Sign(lua_State* L)
{
    double num = luaL_checknumber(L, 1);
    lua_pushinteger(L, (num > 0) - (num < 0));
    return 1;
}

int Lua_Lerp(lua_State* L)
{
    double a = luaL_checknumber(L, 1);
    double b = luaL_checknumber(L, 2);
    double t = luaL_checknumber(L, 3);
    lua_pushnumber(L, a + t * (b - a));
    return 1;
}

// ============= Random =============

static std::random_device rd;
static std::mt19937 gen(rd());

int Lua_RandomInt(lua_State* L)
{
    int min = static_cast<int>(luaL_checkinteger(L, 1));
    int max = static_cast<int>(luaL_checkinteger(L, 2));
    std::uniform_int_distribution<> dis(min, max);
    lua_pushinteger(L, dis(gen));
    return 1;
}

int Lua_RandomFloat(lua_State* L)
{
    double min = luaL_optnumber(L, 1, 0.0);
    double max = luaL_optnumber(L, 2, 1.0);
    std::uniform_real_distribution<> dis(min, max);
    lua_pushnumber(L, dis(gen));
    return 1;
}

int Lua_RandomSeed(lua_State* L)
{
    unsigned int seed = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    gen.seed(seed);
    return 0;
}

// ============= String Operations =============

int Lua_Split(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    const char* delim = luaL_checkstring(L, 2);

    lua_newtable(L);
    int index = 1;

    std::string s(str);
    std::string delimiter(delim);
    size_t pos = 0;

    while ((pos = s.find(delimiter)) != std::string::npos) {
        lua_pushstring(L, s.substr(0, pos).c_str());
        lua_rawseti(L, -2, index++);
        s.erase(0, pos + delimiter.length());
    }
    lua_pushstring(L, s.c_str());
    lua_rawseti(L, -2, index);

    return 1;
}

int Lua_Trim(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    std::string s(str);

    auto start = s.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) {
        lua_pushstring(L, "");
        return 1;
    }

    auto end = s.find_last_not_of(" \t\n\r\f\v");
    lua_pushstring(L, s.substr(start, end - start + 1).c_str());
    return 1;
}

int Lua_LTrim(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    std::string s(str);
    auto start = s.find_first_not_of(" \t\n\r\f\v");
    lua_pushstring(L, start == std::string::npos ? "" : s.substr(start).c_str());
    return 1;
}

int Lua_RTrim(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    std::string s(str);
    auto end = s.find_last_not_of(" \t\n\r\f\v");
    lua_pushstring(L, end == std::string::npos ? "" : s.substr(0, end + 1).c_str());
    return 1;
}

int Lua_StartsWith(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    const char* prefix = luaL_checkstring(L, 2);
    std::string s(str);
    std::string p(prefix);
    lua_pushboolean(L, s.rfind(p, 0) == 0);
    return 1;
}

int Lua_EndsWith(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    const char* suffix = luaL_checkstring(L, 2);
    std::string s(str);
    std::string suf(suffix);
    if (suf.length() > s.length()) {
        lua_pushboolean(L, false);
    } else {
        lua_pushboolean(L, s.compare(s.length() - suf.length(), suf.length(), suf) == 0);
    }
    return 1;
}

int Lua_Join(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    const char* delim = luaL_checkstring(L, 2);

    std::string result;
    int len = lua_objlen(L, 1);

    for (int i = 1; i <= len; i++) {
        lua_rawgeti(L, 1, i);
        const char* val = lua_tostring(L, -1);
        if (val) {
            if (i > 1) result += delim;
            result += val;
        }
        lua_pop(L, 1);
    }

    lua_pushstring(L, result.c_str());
    return 1;
}

int Lua_TableContains(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int len = lua_objlen(L, 1);

    for (int i = 1; i <= len; i++) {
        lua_rawgeti(L, 1, i);
        if (lua_equal(L, -1, 2)) {
            lua_pop(L, 1);
            lua_pushboolean(L, true);
            return 1;
        }
        lua_pop(L, 1);
    }

    lua_pushboolean(L, false);
    return 1;
}

int Lua_TableSlice(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int start = static_cast<int>(luaL_checkinteger(L, 2));
    int end = static_cast<int>(luaL_optinteger(L, 3, lua_objlen(L, 1)));

    lua_newtable(L);
    int index = 1;

    for (int i = start; i <= end; i++) {
        lua_rawgeti(L, 1, i);
        lua_rawseti(L, -2, index++);
    }

    return 1;
}

int Lua_TableKeys(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_newtable(L);
    int index = 1;

    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        lua_rawseti(L, -3, index++);
    }

    return 1;
}

int Lua_TableValues(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_newtable(L);
    int index = 1;

    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        lua_rawseti(L, -3, index++);
    }

    return 1;
}

int Lua_TableMerge(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_newtable(L);

    // Copy first table
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_settable(L, -4);
    }

    // Copy second table (overwrites)
    lua_pushnil(L);
    while (lua_next(L, 2) != 0) {
        lua_pushvalue(L, -2);
        lua_insert(L, -2);
        lua_settable(L, -4);
    }

    return 1;
}

// ============= System/Environment =============

int Lua_GetEnv(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const char* value = std::getenv(name);
    if (value) {
        lua_pushstring(L, value);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int Lua_SetEnv(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const char* value = luaL_checkstring(L, 2);

#ifdef _WIN32
    std::string env = std::string(name) + "=" + std::string(value);
    lua_pushboolean(L, _putenv(env.c_str()) == 0);
#else
    lua_pushboolean(L, setenv(name, value, 1) == 0);
#endif

    return 1;
}

int Lua_Exec(lua_State* L)
{
    const char* cmd = luaL_checkstring(L, 1);

    std::string result;
    char buffer[128];

#ifdef _WIN32
    FILE* pipe = _popen(cmd, "r");
#else
    FILE* pipe = popen(cmd, "r");
#endif

    if (!pipe) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to execute command");
        return 2;
    }

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

#ifdef _WIN32
    int status = _pclose(pipe);
#else
    int status = pclose(pipe);
#endif

    lua_pushstring(L, result.c_str());
    lua_pushinteger(L, status);
    return 2;
}

// ============= Encoding =============

int Lua_Base64Encode(lua_State* L)
{
    size_t len;
    const char* input = luaL_checklstring(L, 1, &len);

    static const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "abcdefghijklmnopqrstuvwxyz"
                                      "0123456789+/";

    std::string result;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (len--) {
        char_array_3[i++] = *(input++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                result += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (int j = 0; j < i + 1; j++)
            result += base64_chars[char_array_4[j]];

        while (i++ < 3)
            result += '=';
    }

    lua_pushstring(L, result.c_str());
    return 1;
}

int Lua_UrlEncode(lua_State* L)
{
    const char* input = luaL_checkstring(L, 1);
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (const char* c = input; *c != '\0'; c++) {
        if (isalnum(*c) || *c == '-' || *c == '_' || *c == '.' || *c == '~') {
            escaped << *c;
        } else {
            escaped << '%' << std::setw(2) << int((unsigned char)*c);
        }
    }

    lua_pushstring(L, escaped.str().c_str());
    return 1;
}

// ============= UUID =============

int Lua_UUID(lua_State* L)
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    uint64_t data1 = dis(gen);
    uint64_t data2 = dis(gen);

    // Set version 4 (random) and variant bits
    data1 = (data1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    data2 = (data2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    char uuid[37];
    snprintf(uuid, sizeof(uuid), "%08x-%04x-%04x-%04x-%012llx", (uint32_t)(data1 >> 32), (uint16_t)(data1 >> 16), (uint16_t)(data1), (uint16_t)(data2 >> 48),
             (unsigned long long)(data2 & 0x0000FFFFFFFFFFFFULL));

    lua_pushstring(L, uuid);
    return 1;
}

// ============= Hash Functions =============

int Lua_HashString(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    std::hash<std::string> hasher;
    size_t hash = hasher(std::string(str));
    lua_pushinteger(L, hash);
    return 1;
}

// ============= File I/O =============

int Lua_ReadFile(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to open file");
        return 2;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    lua_pushlstring(L, content.c_str(), content.size());
    return 1;
}

int Lua_WriteFile(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    size_t len;
    const char* content = luaL_checklstring(L, 2, &len);

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Failed to open file for writing");
        return 2;
    }

    file.write(content, len);
    lua_pushboolean(L, !file.fail());
    return 1;
}

int Lua_AppendFile(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    size_t len;
    const char* content = luaL_checklstring(L, 2, &len);

    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file) {
        lua_pushboolean(L, false);
        lua_pushstring(L, "Failed to open file for appending");
        return 2;
    }

    file.write(content, len);
    lua_pushboolean(L, !file.fail());
    return 1;
}

// ============= Millennium Specific =============

int Lua_GetBackendPath(lua_State* L)
{
    lua_getglobal(L, "MILLENNIUM_PLUGIN_SECRET_BACKEND_ABSOLUTE");
    if (lua_isstring(L, -1)) {
        return 1;
    }
    lua_pop(L, 1);
    lua_pushnil(L);
    return 1;
}

// ============= Additional Utilities =============

int Lua_HexEncode(lua_State* L)
{
    size_t len;
    const char* input = luaL_checklstring(L, 1, &len);

    std::ostringstream result;
    result << std::hex << std::setfill('0');

    for (size_t i = 0; i < len; i++) {
        result << std::setw(2) << (int)(unsigned char)input[i];
    }

    lua_pushstring(L, result.str().c_str());
    return 1;
}

int Lua_ToHex(lua_State* L)
{
    lua_Integer num = luaL_checkinteger(L, 1);
    std::ostringstream result;
    result << "0x" << std::hex << num;
    lua_pushstring(L, result.str().c_str());
    return 1;
}

int Lua_FromHex(lua_State* L)
{
    const char* hex = luaL_checkstring(L, 1);
    lua_Integer num;
    std::istringstream converter(hex);
    converter >> std::hex >> num;
    lua_pushinteger(L, num);
    return 1;
}

static const luaL_Reg utils_lib[] = {
    // Time/Sleep
    { "sleep",            Lua_Sleep          },
    { "time",             Lua_GetTime        },
    { "time_ms",          Lua_GetTimeMs      },
    { "time_micro",       Lua_GetTimeMicro   },

    // Math
    { "round",            Lua_Round          },
    { "clamp",            Lua_Clamp          },
    { "sign",             Lua_Sign           },
    { "lerp",             Lua_Lerp           },

    // Random
    { "random_int",       Lua_RandomInt      },
    { "random_float",     Lua_RandomFloat    },
    { "random_seed",      Lua_RandomSeed     },

    // String
    { "split",            Lua_Split          },
    { "trim",             Lua_Trim           },
    { "ltrim",            Lua_LTrim          },
    { "rtrim",            Lua_RTrim          },
    { "startswith",       Lua_StartsWith     },
    { "endswith",         Lua_EndsWith       },
    { "join",             Lua_Join           },

    // Table
    { "contains",         Lua_TableContains  },
    { "slice",            Lua_TableSlice     },
    { "keys",             Lua_TableKeys      },
    { "values",           Lua_TableValues    },
    { "merge",            Lua_TableMerge     },

    // System
    { "getenv",           Lua_GetEnv         },
    { "setenv",           Lua_SetEnv         },
    { "exec",             Lua_Exec           },

    // Encoding
    { "base64_encode",    Lua_Base64Encode   },
    { "url_encode",       Lua_UrlEncode      },
    { "hex_encode",       Lua_HexEncode      },
    { "to_hex",           Lua_ToHex          },
    { "from_hex",         Lua_FromHex        },

    // UUID & Hash
    { "uuid",             Lua_UUID           },
    { "hash",             Lua_HashString     },

    // File I/O
    { "read_file",        Lua_ReadFile       },
    { "write_file",       Lua_WriteFile      },
    { "append_file",      Lua_AppendFile     },

    // Millennium
    { "get_backend_path", Lua_GetBackendPath },

    { NULL,               NULL               }
};

extern "C" int Lua_OpenUtilsLibrary(lua_State* L)
{
    luaL_newlib(L, utils_lib);
    return 1;
}
