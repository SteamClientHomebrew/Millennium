#include "lua_config_rpc.h"
#include "rpc.h"

#include "millennium/plugin_ipc.h"
#include "millennium/types.h"

#include <unordered_map>
#include <string>

extern rpc_client* g_rpc;

struct config_callback
{
    int lua_ref;
    std::string key; /* empty = wildcard */
};

static int g_next_cb_id = 0;
static std::unordered_map<int, config_callback> g_config_callbacks;

/* json <-> lua conversion */

static void push_json_value(lua_State* L, const json& val)
{
    if (val.is_null()) {
        lua_pushnil(L);
    } else if (val.is_boolean()) {
        lua_pushboolean(L, val.get<bool>());
    } else if (val.is_number_integer()) {
        lua_pushinteger(L, val.get<lua_Integer>());
    } else if (val.is_number_float()) {
        lua_pushnumber(L, val.get<lua_Number>());
    } else if (val.is_string()) {
        const auto& s = val.get_ref<const std::string&>();
        lua_pushlstring(L, s.data(), s.size());
    } else if (val.is_array()) {
        lua_createtable(L, static_cast<int>(val.size()), 0);
        int i = 1;
        for (const auto& elem : val) {
            push_json_value(L, elem);
            lua_rawseti(L, -2, i++);
        }
    } else if (val.is_object()) {
        lua_createtable(L, 0, static_cast<int>(val.size()));
        for (auto it = val.begin(); it != val.end(); ++it) {
            lua_pushlstring(L, it.key().data(), it.key().size());
            push_json_value(L, it.value());
            lua_rawset(L, -3);
        }
    } else {
        lua_pushnil(L);
    }
}

static json lua_to_json(lua_State* L, int idx)
{
    switch (lua_type(L, idx)) {
        case LUA_TNIL:
            return nullptr;
        case LUA_TBOOLEAN:
            return static_cast<bool>(lua_toboolean(L, idx));
        case LUA_TNUMBER:
        {
            lua_Number n = lua_tonumber(L, idx);
            lua_Integer i = lua_tointeger(L, idx);
            if (static_cast<lua_Number>(i) == n) return i;
            return n;
        }
        case LUA_TSTRING:
            return std::string(lua_tostring(L, idx));
        case LUA_TTABLE:
        {
            /* check if it's an array (sequential integer keys starting at 1) */
            int len = static_cast<int>(lua_objlen(L, idx));
            bool is_array = (len > 0);
            if (is_array) {
                /* verify all keys 1..len exist */
                for (int k = 1; k <= len; ++k) {
                    lua_rawgeti(L, idx, k);
                    if (lua_isnil(L, -1)) {
                        is_array = false;
                        lua_pop(L, 1);
                        break;
                    }
                    lua_pop(L, 1);
                }
            }

            if (is_array) {
                json arr = json::array();
                for (int k = 1; k <= len; ++k) {
                    lua_rawgeti(L, idx, k);
                    arr.push_back(lua_to_json(L, lua_gettop(L)));
                    lua_pop(L, 1);
                }
                return arr;
            }

            /* object */
            json obj = json::object();
            int abs_idx = (idx > 0) ? idx : lua_gettop(L) + idx + 1;
            lua_pushnil(L);
            while (lua_next(L, abs_idx)) {
                std::string k;
                if (lua_type(L, -2) == LUA_TSTRING) {
                    k = lua_tostring(L, -2);
                } else if (lua_type(L, -2) == LUA_TNUMBER) {
                    k = std::to_string(static_cast<int>(lua_tonumber(L, -2)));
                } else {
                    lua_pop(L, 1);
                    continue;
                }
                obj[k] = lua_to_json(L, lua_gettop(L));
                lua_pop(L, 1);
            }
            return obj;
        }
        default:
            return nullptr;
    }
}

static int config_get(lua_State* L)
{
    const char* key = luaL_checkstring(L, 1);

    try {
        json result = g_rpc->call(plugin_ipc::child_method::CONFIG_GET, {
                                                                            { "key", key }
        });

        if (result.contains("error")) {
            lua_pushnil(L);
            lua_pushstring(L, result["error"].get<std::string>().c_str());
            return 2;
        }

        push_json_value(L, result.value("value", json(nullptr)));
        return 1;
    } catch (const std::exception& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static int config_set(lua_State* L)
{
    const char* key = luaL_checkstring(L, 1);
    luaL_checkany(L, 2);
    json value = lua_to_json(L, 2);

    try {
        json result = g_rpc->call(plugin_ipc::child_method::CONFIG_SET, {
                                                                            { "key",   key   },
                                                                            { "value", value }
        });

        if (result.contains("error")) {
            lua_pushboolean(L, 0);
            lua_pushstring(L, result["error"].get<std::string>().c_str());
            return 2;
        }

        lua_pushboolean(L, 1);
        return 1;
    } catch (const std::exception& e) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static int config_delete(lua_State* L)
{
    const char* key = luaL_checkstring(L, 1);

    try {
        json result = g_rpc->call(plugin_ipc::child_method::CONFIG_DELETE, {
                                                                               { "key", key }
        });

        if (result.contains("error")) {
            lua_pushboolean(L, 0);
            lua_pushstring(L, result["error"].get<std::string>().c_str());
            return 2;
        }

        lua_pushboolean(L, 1);
        return 1;
    } catch (const std::exception& e) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, e.what());
        return 2;
    }
}

static int config_get_all(lua_State* L)
{
    try {
        json result = g_rpc->call(plugin_ipc::child_method::CONFIG_GET_ALL);

        if (result.contains("error")) {
            lua_pushnil(L);
            lua_pushstring(L, result["error"].get<std::string>().c_str());
            return 2;
        }

        push_json_value(L, result.value("config", json::object()));
        return 1;
    } catch (const std::exception& e) {
        lua_pushnil(L);
        lua_pushstring(L, e.what());
        return 2;
    }
}

/* config.on_change([key,] callback) -> unsubscribe() */
static int config_on_change(lua_State* L)
{
    std::string key;
    int cb_idx;

    if (lua_gettop(L) >= 2 && lua_isstring(L, 1)) {
        key = lua_tostring(L, 1);
        luaL_checktype(L, 2, LUA_TFUNCTION);
        cb_idx = 2;
    } else {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        cb_idx = 1;
        /* key stays empty = wildcard */
    }

    lua_pushvalue(L, cb_idx);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    int id = g_next_cb_id++;
    g_config_callbacks[id] = { ref, key };

    /* return an unsubscribe closure */
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    lua_pushcclosure(L, [](lua_State* Ls) -> int
    {
        int cb_id = static_cast<int>(lua_tointeger(Ls, lua_upvalueindex(1)));
        auto it = g_config_callbacks.find(cb_id);
        if (it != g_config_callbacks.end()) {
            luaL_unref(Ls, LUA_REGISTRYINDEX, it->second.lua_ref);
            g_config_callbacks.erase(it);
        }
        return 0;
    }, 1);

    return 1;
}

static const luaL_Reg config_lib[] = {
    { "get",       config_get       },
    { "set",       config_set       },
    { "delete",    config_delete    },
    { "get_all",   config_get_all   },
    { "on_change", config_on_change },
    { NULL,        NULL             }
};

void register_config_module(lua_State* L)
{
    /* expects the millennium table at the top of the stack */
    lua_newtable(L);
    luaL_setfuncs(L, config_lib, 0);
    lua_setfield(L, -2, "config");
}

void dispatch_config_change(lua_State* L, const std::string& key, const json& value)
{
    for (const auto& [id, cb] : g_config_callbacks) {
        if (!cb.key.empty() && cb.key != key) continue;

        lua_rawgeti(L, LUA_REGISTRYINDEX, cb.lua_ref);
        lua_pushstring(L, key.c_str());
        push_json_value(L, value);

        if (lua_pcall(L, 2, 0, 0) != 0) {
            const char* err = lua_tostring(L, -1);
            fprintf(stderr, "[lua-host] config on_change callback error: %s\n", err ? err : "unknown");
            lua_pop(L, 1);
        }
    }
}
