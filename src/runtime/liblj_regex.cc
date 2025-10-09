#include <lua.hpp>
#include <regex>
#include <string>
#include <vector>

// ============= Regex Match =============

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

// ============= Regex Search =============

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

            // Full match
            lua_pushstring(L, match[0].str().c_str());
            lua_rawseti(L, -2, 0);

            // Capture groups
            for (size_t i = 1; i < match.size(); i++) {
                lua_pushstring(L, match[i].str().c_str());
                lua_rawseti(L, -2, i);
            }

            // Add position info
            lua_pushinteger(L, match.position(0) + 1); // 1-based indexing
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

// ============= Regex Find All =============

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

            // Full match
            lua_pushstring(L, iter->str(0).c_str());
            lua_rawseti(L, -2, 0);

            // Capture groups
            for (size_t i = 1; i < iter->size(); i++) {
                lua_pushstring(L, (*iter)[i].str().c_str());
                lua_rawseti(L, -2, i);
            }

            // Position info
            lua_pushinteger(L, iter->position(0) + 1); // 1-based indexing
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

// ============= Regex Replace =============

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

// ============= Regex Replace First =============

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

// ============= Regex Split =============

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

// ============= Regex Escape =============

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

// ============= Regex Test (with flags) =============

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
                case 'm':
                    flags_type |= std::regex::multiline;
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

extern "C" int Lua_OpenRegex(lua_State* L)
{
    luaL_newlib(L, regex_lib);
    return 1;
}