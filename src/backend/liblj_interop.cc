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

#include "millennium/backend_mgr.h"
#include "millennium/ffi.h"

#define LUA_IS_LOCKED_ERROR_MESSAGE                                                                                                                                                \
    "Lua state is currently locked and FFI call couldn't be made safely. "                                                                                                         \
    "You likely are IO blocking the main thread of your backend. "                                                                                                                 \
    "All backend functions should be non-blocking and finish shortly after calling."

EvalResult Lua::LockAndInvokeMethod(std::string pluginName, nlohmann::json script)
{
    BackendManager& backendManager = BackendManager::GetInstance();

    auto luaStateOpt = backendManager.GetLuaThreadStateFromName(pluginName);
    if (!luaStateOpt.has_value()) {
        return { FFI_Type::Error, "Lua state not found for plugin: " + pluginName };
    }

    lua_State* L = luaStateOpt.value();

    if (backendManager.Lua_IsMutexLocked(L)) {
        LOG_ERROR("Lua state is currently locked for plugin: {}", pluginName);
        return { FFI_Type::Error, LUA_IS_LOCKED_ERROR_MESSAGE };
    }

    if (!script.contains("methodName") || !script["methodName"].is_string()) {
        return { FFI_Type::Error, "Missing or invalid methodName in script" };
    }
    std::string methodName = script["methodName"];

    std::vector<nlohmann::json> argValues;
    if (script.contains("argumentList") && script["argumentList"].is_object()) {
        for (auto it = script["argumentList"].begin(); it != script["argumentList"].end(); ++it) {
            argValues.push_back(it.value());
        }
    }

    backendManager.Lua_LockLua(L);

    lua_getglobal(L, methodName.c_str());
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        backendManager.Lua_UnlockLua(L);
        return { FFI_Type::Error, "Function not found in Lua: " + methodName };
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

    int numArgs = static_cast<int>(argValues.size());
    if (lua_pcall(L, numArgs, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::string errMsg = err ? err : "Unknown Lua error";
        lua_pop(L, 1);

        backendManager.Lua_UnlockLua(L);
        return { FFI_Type::Error, errMsg };
    }

    std::string result;
    FFI_Type resultType = FFI_Type::UnknownType;

    if (lua_isstring(L, -1)) {
        result = lua_tostring(L, -1);
        resultType = FFI_Type::String;
    } else if (lua_isboolean(L, -1)) {
        result = lua_toboolean(L, -1) ? "true" : "false";
        resultType = FFI_Type::Boolean;
    } else if (lua_isnumber(L, -1)) {
        result = std::to_string(lua_tonumber(L, -1));
        resultType = FFI_Type::Integer;
    } else if (lua_isnil(L, -1)) {
        result = "nil";
        resultType = FFI_Type::Integer;
    } else {
        result = "Unsupported return type";
        resultType = FFI_Type::UnknownType;
    }
    lua_pop(L, 1);

    backendManager.Lua_UnlockLua(L);
    return { resultType, result };
}

void Lua::CallFrontEndLoaded(std::string pluginName)
{
    BackendManager& backendManager = BackendManager::GetInstance();

    auto luaStateOpt = backendManager.GetLuaThreadStateFromName(pluginName);
    if (!luaStateOpt.has_value()) {
        LOG_ERROR("Lua state not found for plugin: {}", pluginName);
        return;
    }

    lua_State* L = luaStateOpt.value();

    if (backendManager.Lua_IsMutexLocked(L)) {
        LOG_ERROR("Failed to call frontend loaded on plugin: {}. Message: {}", pluginName, LUA_IS_LOCKED_ERROR_MESSAGE);
        return;
    }

    backendManager.Lua_LockLua(L);

    lua_getglobal(L, "MILLENNIUM_PLUGIN_DEFINITION");
    if (!lua_istable(L, -1)) {
        LOG_ERROR("MILLENNIUM_PLUGIN_DEFINITION not found or not a table for plugin: {}", pluginName);
        lua_pop(L, 1);
        backendManager.Lua_UnlockLua(L);
        return;
    }

    lua_getfield(L, -1, "on_frontend_loaded");
    if (!lua_isfunction(L, -1)) {
        Logger.Warn("on_frontend_loaded not found or not a function for plugin: {}", pluginName);
        lua_pop(L, 2);
        backendManager.Lua_UnlockLua(L);
        return;
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        LOG_ERROR("Error calling on_frontend_loaded: {}", err ? err : "unknown error");
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    backendManager.Lua_UnlockLua(L);
}