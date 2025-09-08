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

#include "co_spawn.h"
#include "encoding.h"
#include "executor.h"
#include "ffi.h"
#include "http_hooks.h"
#include "internal_logger.h"
#include "locals.h"
#include "plugin_logger.h"
#include <_millennium_api.h>
#include <co_stub.h>
#include <fmt/core.h>
#include <fstream>
#include <lua.hpp>
#include <nlohmann/json.hpp>

/**
 * RAII-style Lua stack guard to ensure stack cleanup in all code paths
 */
class LuaStackGuard
{
  private:
    lua_State* L;
    int top;

  public:
    LuaStackGuard(lua_State* L) : L(L), top(lua_gettop(L))
    {
    }
    ~LuaStackGuard()
    {
        lua_settop(L, top);
    }

    // Disable copy constructor and assignment to prevent misuse
    LuaStackGuard(const LuaStackGuard&) = delete;
    LuaStackGuard& operator=(const LuaStackGuard&) = delete;
};

/**
 * Helper function to safely get plugin name from Lua global
 * Returns empty string if not found or error occurs
 */
static std::string GetPluginNameSafe(lua_State* L)
{
    LuaStackGuard guard(L);

    lua_getglobal(L, "MILLENNIUM_PLUGIN_SECRET_NAME");
    if (lua_isnil(L, -1)) {
        LOG_ERROR("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        return "";
    }

    const char* pluginNameStr = lua_tostring(L, -1);
    return pluginNameStr ? std::string(pluginNameStr) : std::string();
}

/**
 * Convert number to string with same precision as original
 */
static std::string NumberToString(double number)
{
    return fmt::format("{:.17g}", number);
}

int Lua_GetUserSettings(lua_State* L)
{
    return luaL_error(L, "get_user_settings is not implemented yet. It will likely be removed in the future.");
}

int Lua_SetUserSettings(lua_State* L)
{
    return luaL_error(L, "set_user_settings_key is not implemented yet. It will likely be removed in the future.");
}

int Lua_CallFrontendMethod(lua_State* L)
{
    const char* methodName = luaL_checkstring(L, 1);
    std::vector<JavaScript::JsFunctionConstructTypes> params;

    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        if (!lua_istable(L, 2)) {
            return luaL_error(L, "params must be a table");
        }

#ifdef _WIN32
        int len = (int)lua_objlen(L, 2);
#else
        int len = (int)lua_rawlen(L, 2);
#endif
        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, 2, i);

            std::string valueType;
            std::string strValue;

            if (lua_isstring(L, -1)) {
                valueType = "string";
                const char* temp = lua_tostring(L, -1);
                strValue = temp ? temp : "";
            } else if (lua_isboolean(L, -1)) {
                valueType = "boolean";
                strValue = lua_toboolean(L, -1) ? "true" : "false";
            } else if (lua_isnumber(L, -1)) {
                valueType = "number";
                strValue = NumberToString(lua_tonumber(L, -1));
            } else {
                lua_pop(L, 1);
                return luaL_error(L, "Millennium's IPC can only handle [boolean, string, number]");
            }

            try {
                if (typeMap.find(valueType) == typeMap.end()) {
                    lua_pop(L, 1);
                    return luaL_error(L, "Millennium's IPC can only handle [boolean, string, number]");
                }

                params.push_back({ strValue, typeMap.at(valueType) });
            } catch (const std::exception&) {
                lua_pop(L, 1);
                return luaL_error(L, "Millennium's IPC can only handle [boolean, string, number]");
            }

            lua_pop(L, 1);
        }
    }

    const std::string pluginName = GetPluginNameSafe(L);
    if (pluginName.empty()) {
        lua_pushnil(L);
        return 1;
    }

    const std::string script = JavaScript::ConstructFunctionCall(pluginName.c_str(), methodName, params);

    int result = JavaScript::Lua_EvaluateFromSocket(
        fmt::format("if (typeof window !== 'undefined' && typeof window.MillenniumFrontEndError === 'undefined') {{ window.MillenniumFrontEndError = class MillenniumFrontEndError "
                    "extends Error {{ constructor(message) {{ super(message); this.name = 'MillenniumFrontEndError'; }} }} }}"
                    "if (typeof PLUGIN_LIST === 'undefined' || !PLUGIN_LIST?.['{}']) throw new window.MillenniumFrontEndError('frontend not loaded yet!');\n\n{}",
                    pluginName, script),
        L);

    return result;
}

int Lua_GetVersionInfo(lua_State* L)
{
    lua_pushstring(L, MILLENNIUM_VERSION);
    return 1;
}

int Lua_GetSteamPath(lua_State* L)
{
    try {
        const std::string steamPath = SystemIO::GetSteamPath().string();
        lua_pushstring(L, steamPath.c_str());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get Steam path: {}", e.what());
        lua_pushstring(L, ""); // Return empty string on error
    }
    return 1;
}

int Lua_GetInstallPath(lua_State* L)
{
    try {
        const std::string installPath = SystemIO::GetInstallPath().string();
        lua_pushstring(L, installPath.c_str());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get install path: {}", e.what());
        lua_pushstring(L, ""); // Return empty string on error
    }
    return 1;
}

int Lua_RemoveBrowserModule(lua_State* L)
{
    const lua_Integer hookId = luaL_checkinteger(L, 1);
    const bool success = HttpHookManager::get().RemoveHook(hookId);
    lua_pushboolean(L, success);
    return 1;
}

unsigned long long Lua_AddBrowserModule(lua_State* L, HttpHookManager::TagTypes type)
{
    const char* content = luaL_checkstring(L, 1);
    const char* pattern = luaL_optstring(L, 2, ".*");
    return Millennium_AddBrowserModule(content, pattern, type);
}

int Lua_AddBrowserCss(lua_State* L)
{
    const unsigned long long result = Lua_AddBrowserModule(L, HttpHookManager::TagTypes::STYLESHEET);
    lua_pushinteger(L, static_cast<lua_Integer>(result));
    return 1;
}

int Lua_AddBrowserJs(lua_State* L)
{
    const unsigned long long result = Lua_AddBrowserModule(L, HttpHookManager::TagTypes::JAVASCRIPT);
    lua_pushinteger(L, static_cast<lua_Integer>(result));
    return 1;
}

int Lua_IsPluginEnable(lua_State* L)
{
    const char* pluginName = luaL_checkstring(L, 1);
    if (!pluginName) {
        lua_pushboolean(L, 0);
        return 1;
    }

    try {
        std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
        const bool isEnabled = settingsStore->IsEnabledPlugin(pluginName);
        lua_pushboolean(L, isEnabled);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to check plugin status for '{}': {}", pluginName, e.what());
        lua_pushboolean(L, 0); // Default to disabled on error
    }
    return 1;
}

int Lua_EmitReadyMessage(lua_State* L)
{
    const std::string pluginName = GetPluginNameSafe(L);
    if (pluginName.empty()) {
        lua_pushboolean(L, 0);
        return 1;
    }

    try {
        CoInitializer::BackendCallbacks& backendHandler = CoInitializer::BackendCallbacks::getInstance();
        backendHandler.BackendLoaded({ pluginName, CoInitializer::BackendCallbacks::BACKEND_LOAD_SUCCESS });
        lua_pushboolean(L, 1);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to emit ready message for plugin '{}': {}", pluginName, e.what());
        lua_pushboolean(L, 0);
    }

    return 1;
}

/**
 * Lua library registration function for the Millennium module
 */
static const luaL_Reg millennium_lib[] = {
    { "ready",                 Lua_EmitReadyMessage    },
    { "add_browser_css",       Lua_AddBrowserCss       },
    { "add_browser_js",        Lua_AddBrowserJs        },
    { "remove_browser_module", Lua_RemoveBrowserModule },
    { "get_user_settings",     Lua_GetUserSettings     },
    { "set_user_settings_key", Lua_SetUserSettings     },
    { "version",               Lua_GetVersionInfo      },
    { "steam_path",            Lua_GetSteamPath        },
    { "get_install_path",      Lua_GetInstallPath      },
    { "call_frontend_method",  Lua_CallFrontendMethod  },
    { "is_plugin_enabled",     Lua_IsPluginEnable      },
    { NULL,                    NULL                    }  // Sentinel
};

/**
 * Register the Millennium module with Lua
 */
extern "C" int Lua_OpenMillenniumLibrary(lua_State* L)
{
    luaL_newlib(L, millennium_lib);
    return 1;
}