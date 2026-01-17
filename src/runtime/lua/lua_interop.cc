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

#include "millennium/core_ipc.h"
#include "millennium/logger.h"
#include <variant>

#define LUA_IS_LOCKED_ERROR_MESSAGE                                                                                                                                                \
    "Lua state is currently locked and FFI call couldn't be made safely. "                                                                                                         \
    "You likely are IO blocking the main thread of your backend. "                                                                                                                 \
    "All backend functions should be non-blocking and finish shortly after calling."

std::string ipc_main::get_vm_call_result_error(vm_call_result result)
{
    nlohmann::json j;
    std::visit([&j](auto&& arg)
    {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            j = nullptr;
        } else {
            j = arg;
        }
    }, result.value);
    return j.dump();
}

ipc_main::vm_call_result ipc_main::lua_evaluate(std::string pluginName, nlohmann::json script)
{
    const auto shared_backend_mgr = m_backend_manager.lock();
    if (!shared_backend_mgr) {
        LOG_ERROR("Failed to lock backend manager");
    }

    auto luaStateOpt = shared_backend_mgr->lua_thread_state_from_plugin_name(pluginName);
    if (!luaStateOpt.has_value()) {
        return { false, "Lua state not found for plugin: " + pluginName };
    }

    lua_State* L = luaStateOpt.value();
    shared_backend_mgr->lua_lock(L);

    if (!script.contains("methodName") || !script["methodName"].is_string()) {
        return { false, std::string("Missing or invalid methodName in script") };
    }

    std::string methodName = script["methodName"];

    std::vector<nlohmann::json> argValues;
    if (script.contains("argumentList") && script["argumentList"].is_object()) {
        for (auto it = script["argumentList"].begin(); it != script["argumentList"].end(); ++it) {
            argValues.push_back(it.value());
        }
    }

    /** Support for class methods using "something:function" */
    size_t colonPos = methodName.find(':');
    bool isMethod = (colonPos != std::string::npos);
    int numArgs = static_cast<int>(argValues.size());

    if (isMethod) {
        std::string tableName = methodName.substr(0, colonPos);
        std::string funcName = methodName.substr(colonPos + 1);

        lua_getglobal(L, tableName.c_str()); // push table
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            shared_backend_mgr->lua_unlock(L);
            return { false, "Table not found in Lua: " + tableName };
        }
        lua_getfield(L, -1, funcName.c_str()); // push function
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            shared_backend_mgr->lua_unlock(L);
            return { false, "Method not found in Lua: " + methodName };
        }
        lua_pushvalue(L, -2); /** push table as self */
        /** Now stack: table, function, self */
        /** Remove table below function and self */
        lua_remove(L, -3); /** remove table, now stack: function, self */

        /** push args to fn */
        for (const auto& arg : argValues) {
            if (arg.is_string()) {
                lua_pushstring(L, arg.get<std::string>().c_str());
            } else if (arg.is_boolean()) {
                lua_pushboolean(L, arg.get<bool>());
            } else if (arg.is_number_integer()) {
                lua_pushinteger(L, arg.get<lua_Integer>());
            } else if (arg.is_number_float()) {
                lua_pushnumber(L, arg.get<lua_Number>());
            } else {
                lua_pushnil(L);
            }
        }
        numArgs += 1; /** self */
    } else {
        lua_getglobal(L, methodName.c_str());
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            shared_backend_mgr->lua_unlock(L);
            return { false, "Function not found in Lua: " + methodName };
        }
        for (const auto& arg : argValues) {
            if (arg.is_string()) {
                lua_pushstring(L, arg.get<std::string>().c_str());
            } else if (arg.is_boolean()) {
                lua_pushboolean(L, arg.get<bool>());
            } else if (arg.is_number_integer()) {
                lua_pushinteger(L, arg.get<lua_Integer>());
            } else if (arg.is_number_float()) {
                lua_pushnumber(L, arg.get<lua_Number>());
            } else {
                lua_pushnil(L);
            }
        }
    }

    if (lua_pcall(L, numArgs, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::string errMsg = err ? err : "Unknown Lua error";
        lua_pop(L, 1);

        shared_backend_mgr->lua_unlock(L);
        return { false, errMsg };
    }

    ipc_main::vm_call_result result;

    if (lua_isstring(L, -1)) {
        std::string res = std::string(lua_tostring(L, -1));
    } else if (lua_isboolean(L, -1)) {
        result.value = bool(lua_toboolean(L, -1));
    } else if (lua_isnumber(L, -1)) {
        result.value = lua_tonumber(L, -1);
    } else if (lua_isnil(L, -1)) {
        result.value = std::monostate{};
    } else {
        result.value = std::monostate{};
    }
    lua_pop(L, 1);
    result.success = true;

    shared_backend_mgr->lua_unlock(L);
    return result;
}

void ipc_main::lua_call_frontend_loaded(std::string pluginName)
{
    const auto shared_backend_mgr = m_backend_manager.lock();
    if (!shared_backend_mgr) {
        LOG_ERROR("Failed to lock backend manager");
    }

    auto luaStateOpt = shared_backend_mgr->lua_thread_state_from_plugin_name(pluginName);
    if (!luaStateOpt.has_value()) {
        LOG_ERROR("Lua state not found for plugin: {}", pluginName);
        return;
    }

    lua_State* L = luaStateOpt.value();
    shared_backend_mgr->lua_lock(L);

    lua_getglobal(L, "MILLENNIUM_PLUGIN_DEFINITION");
    if (!lua_istable(L, -1)) {
        LOG_ERROR("MILLENNIUM_PLUGIN_DEFINITION not found or not a table for plugin: {}", pluginName);
        lua_pop(L, 1);
        shared_backend_mgr->lua_unlock(L);
        return;
    }

    lua_getfield(L, -1, "on_frontend_loaded");
    if (!lua_isfunction(L, -1)) {
        Logger.Warn("on_frontend_loaded not found or not a function for plugin: {}", pluginName);
        lua_pop(L, 2);
        shared_backend_mgr->lua_unlock(L);
        return;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        LOG_ERROR("Error calling on_frontend_loaded: {}", err ? err : "unknown error");
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    shared_backend_mgr->lua_unlock(L);
}
