#pragma once

#include <lua.hpp>
#include <nlohmann/json.hpp>
#include <string>

void register_config_module(lua_State* L);
void dispatch_config_change(lua_State* L, const std::string& key, const nlohmann::json& value);
