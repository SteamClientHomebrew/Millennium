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

#include <lua.hpp>
#include <regex>
#include <string>

int Lua_RegexMatch(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    const char* pattern = luaL_checkstring(L, 2);

    try {
        std::regex re(pattern);
        bool matches = std::regex_match(str, re);
        lua_pushboolean(L, matches);
        return 1;
    } catch (const std::regex_error& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

int Lua_RegexSearch(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    const char* pattern = luaL_checkstring(L, 2);

    try {
        std::regex re(pattern);
        std::smatch match;
        std::string s(str);

        if (std::regex_search(s, match, re)) {
            lua_newtable(L);

            lua_pushstring(L, match[0].str().c_str());
            lua_rawseti(L, -2, 0);

            for (size_t i = 1; i < match.size(); i++) {
                lua_pushstring(L, match[i].str().c_str());
                lua_rawseti(L, -2, i);
            }

            lua_pushinteger(L, match.position(0) + 1);
            lua_setfield(L, -2, "pos");

            lua_pushinteger(L, match.length(0));
            lua_setfield(L, -2, "len");

            return 1;
        }

        lua_pushnil(L);
        return 1;
    } catch (const std::regex_error& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

int Lua_RegexFindAll(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    const char* pattern = luaL_checkstring(L, 2);

    try {
        std::regex re(pattern);
        std::string s(str);
        std::sregex_iterator iter(s.begin(), s.end(), re);
        std::sregex_iterator end;

        lua_newtable(L);
        int index = 1;

        while (iter != end) {
            lua_newtable(L);

            lua_pushstring(L, iter->str(0).c_str());
            lua_rawseti(L, -2, 0);

            for (size_t i = 1; i < iter->size(); i++) {
                lua_pushstring(L, (*iter)[i].str().c_str());
                lua_rawseti(L, -2, i);
            }

            lua_pushinteger(L, iter->position(0) + 1);
            lua_setfield(L, -2, "pos");

            lua_pushinteger(L, iter->length(0));
            lua_setfield(L, -2, "len");

            lua_rawseti(L, -2, index++);
            ++iter;
        }

        return 1;
    } catch (const std::regex_error& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

int Lua_RegexReplace(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    const char* pattern = luaL_checkstring(L, 2);
    const char* replacement = luaL_checkstring(L, 3);

    try {
        std::regex re(pattern);
        std::string s(str);
        std::string result = std::regex_replace(s, re, replacement);

        lua_pushstring(L, result.c_str());
        return 1;
    } catch (const std::regex_error& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

int Lua_RegexReplaceFirst(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    const char* pattern = luaL_checkstring(L, 2);
    const char* replacement = luaL_checkstring(L, 3);

    try {
        std::regex re(pattern);
        std::string s(str);
        std::smatch match;

        if (std::regex_search(s, match, re)) {
            std::string result = match.prefix().str() + replacement + match.suffix().str();
            lua_pushstring(L, result.c_str());
            return 1;
        }

        lua_pushstring(L, str);
        return 1;
    } catch (const std::regex_error& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

int Lua_RegexSplit(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    const char* pattern = luaL_checkstring(L, 2);

    try {
        std::regex re(pattern);
        std::string s(str);
        std::sregex_token_iterator iter(s.begin(), s.end(), re, -1);
        std::sregex_token_iterator end;

        lua_newtable(L);
        int index = 1;

        while (iter != end) {
            lua_pushstring(L, iter->str().c_str());
            lua_rawseti(L, -2, index++);
            ++iter;
        }

        return 1;
    } catch (const std::regex_error& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

int Lua_RegexEscape(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    std::string result;

    for (const char* p = str; *p != '\0'; p++) {
        switch (*p) {
            case '.':
            case '*':
            case '+':
            case '?':
            case '|':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '^':
            case '$':
            case '\\':
                result += '\\';
                // fallthrough
            default:
                result += *p;
        }
    }

    lua_pushstring(L, result.c_str());
    return 1;
}

int Lua_RegexTest(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    const char* pattern = luaL_checkstring(L, 2);
    const char* flags = luaL_optstring(L, 3, "");

    try {
        std::regex::flag_type flags_type = std::regex::ECMAScript;

        for (const char* p = flags; *p != '\0'; p++) {
            switch (*p) {
                case 'i':
                    flags_type |= std::regex::icase;
                    break;
                case 'g':
                    break; // Global flag handled elsewhere
            }
        }

        std::regex re(pattern, flags_type);
        bool matches = std::regex_search(str, re);
        lua_pushboolean(L, matches);
        return 1;
    } catch (const std::regex_error& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static const luaL_Reg regex_lib[] = {
    { "match",         Lua_RegexMatch        },
    { "search",        Lua_RegexSearch       },
    { "find_all",      Lua_RegexFindAll      },
    { "replace",       Lua_RegexReplace      },
    { "replace_first", Lua_RegexReplaceFirst },
    { "split",         Lua_RegexSplit        },
    { "escape",        Lua_RegexEscape       },
    { "test",          Lua_RegexTest         },
    { NULL,            NULL                  }
};

extern "C" int luaopen_regex_lib(lua_State* L)
{
    luaL_newlib(L, regex_lib);
    return 1;
}
