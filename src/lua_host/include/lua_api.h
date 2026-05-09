#pragma once

#include <lua.hpp>
#include <nlohmann/json.hpp>
#include <string>

extern "C" {
    int luaopen_cjson(lua_State* L);
    int luaopen_millennium_lib(lua_State* L);
    int luaopen_http_lib(lua_State* L);
    int luaopen_utils_lib(lua_State* L);
    int luaopen_logger_lib(lua_State* L);
    int luaopen_fs_lib(lua_State* L);
    int luaopen_regex_lib(lua_State* L);
    int luaopen_datetime_lib(lua_State* L);
}

void register_config_module(lua_State* L);
void dispatch_config_change(lua_State* L, const std::string& key, const nlohmann::json& value);
