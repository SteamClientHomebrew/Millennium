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
#include <co_stub.h>
#include <fmt/core.h>
#include <fstream>
#include <lua.hpp>
#include <nlohmann/json.hpp>

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

    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2))
    {
        if (!lua_istable(L, 2))
        {
            return luaL_error(L, "params must be a table");
        }

        // LuaJIT: use lua_objlen instead of lua_len
        int len = (int)lua_objlen(L, 2);

        for (int i = 1; i <= len; ++i)
        {
            // LuaJIT: use lua_rawgeti instead of lua_geti
            lua_rawgeti(L, 2, i);

            std::string valueType;
            const char* strValue = nullptr;

            if (lua_isstring(L, -1))
            {
                valueType = "string";
                strValue = lua_tostring(L, -1);
            }
            else if (lua_isboolean(L, -1))
            {
                valueType = "boolean";
                strValue = lua_toboolean(L, -1) ? "true" : "false";
            }
            else if (lua_isnumber(L, -1))
            {
                valueType = "number";
                static thread_local char numBuf[64];
                snprintf(numBuf, sizeof(numBuf), "%.17g", lua_tonumber(L, -1));
                strValue = numBuf;
            }
            else
            {
                lua_pop(L, 1);
                return luaL_error(L, "Millennium's IPC can only handle [boolean, string, number]");
            }

            try
            {
                params.push_back({strValue, typeMap[valueType]});
            }
            catch (const std::exception&)
            {
                lua_pop(L, 1);
                return luaL_error(L, "Millennium's IPC can only handle [boolean, string, number]");
            }

            lua_pop(L, 1);
        }
    }

    lua_getglobal(L, "MILLENNIUM_PLUGIN_SECRET_NAME");
    if (lua_isnil(L, -1))
    {
        LOG_ERROR("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        lua_pushnil(L);
        return 1;
    }

    const std::string pluginName = lua_tostring(L, -1);
    lua_pop(L, 1);

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
    lua_pushstring(L, SystemIO::GetSteamPath().string().c_str());
    return 1;
}

int Lua_GetInstallPath(lua_State* L)
{
    lua_pushstring(L, SystemIO::GetInstallPath().string().c_str());
    return 1;
}

int Lua_RemoveBrowserModule(lua_State* L)
{
    int moduleId = luaL_checkinteger(L, 1);

    const int success = HttpHookManager::get().RemoveHook(moduleId);
    lua_pushboolean(L, success);
    return 1;
}

unsigned long long Lua_AddBrowserModule(lua_State* L, HttpHookManager::TagTypes type)
{
    const char* moduleItem = luaL_checkstring(L, 1);
    const char* regexSelector = luaL_optstring(L, 2, ".*");

    g_hookedModuleId++;
    auto path = SystemIO::GetSteamPath() / "steamui" / moduleItem;

    try
    {
        HttpHookManager::get().AddHook({path.generic_string(), std::regex(regexSelector), type, g_hookedModuleId});
    }
    catch (const std::regex_error& e)
    {
        LOG_ERROR("Attempted to add a browser module with invalid regex: {} ({})", regexSelector, e.what());
        ErrorToLogger("executor", fmt::format("Failed to add browser module with invalid regex: {} ({})", regexSelector, e.what()));
        return 0;
    }

    return g_hookedModuleId;
}

int Lua_AddBrowserCss(lua_State* L)
{
    unsigned long long moduleId = Lua_AddBrowserModule(L, HttpHookManager::TagTypes::STYLESHEET);
    lua_pushinteger(L, moduleId);
    return 1;
}

int Lua_AddBrowserJs(lua_State* L)
{
    unsigned long long moduleId = Lua_AddBrowserModule(L, HttpHookManager::TagTypes::JAVASCRIPT);
    lua_pushinteger(L, moduleId);
    return 1;
}

int Lua_TogglePluginStatus(lua_State* L)
{
    throw std::runtime_error("TogglePluginStatus is not implemented yet.");
}

int Lua_IsPluginEnable(lua_State* L)
{
    const char* pluginName = luaL_checkstring(L, 1);

    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
    bool isEnabled = settingsStore->IsEnabledPlugin(pluginName);
    lua_pushboolean(L, isEnabled);
    return 1;
}

int Lua_EmitReadyMessage(lua_State* L)
{
    lua_getglobal(L, "MILLENNIUM_PLUGIN_SECRET_NAME");
    if (lua_isnil(L, -1))
    {
        LOG_ERROR("error getting plugin name, can't make IPC request. this is likely a millennium bug.");
        lua_pushboolean(L, 0);
        return 1;
    }

    const std::string pluginName = lua_tostring(L, -1);
    lua_pop(L, 1);

    CoInitializer::BackendCallbacks& backendHandler = CoInitializer::BackendCallbacks::getInstance();
    backendHandler.BackendLoaded({pluginName, CoInitializer::BackendCallbacks::BACKEND_LOAD_SUCCESS});

    lua_pushboolean(L, 1);
    return 1;
}

int Lua_GetPluginLogs(lua_State* L)
{
    nlohmann::json logData = nlohmann::json::array();
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();

    std::vector<SettingsStore::PluginTypeSchema> plugins = settingsStore->ParseAllPlugins();

    for (auto& logger : g_loggerList)
    {
        nlohmann::json logDataItem;

        for (auto [message, logLevel] : logger->CollectLogs())
        {
            logDataItem.push_back({
                {"message", Base64Encode(message)},
                {"level",   logLevel             }
            });
        }

        std::string pluginName = logger->GetPluginName(false);

        for (auto& plugin : plugins)
        {
            if (plugin.pluginJson.contains("name") && plugin.pluginJson["name"] == logger->GetPluginName(false))
            {
                pluginName = plugin.pluginJson.value("common_name", pluginName);
                break;
            }
        }

        // Handle package manager plugin
        if (pluginName == "pipx")
        {
            pluginName = "Package Manager";
        }

        logData.push_back({
            {"name", pluginName },
            {"logs", logDataItem}
        });
    }

    lua_pushstring(L, logData.dump().c_str());
    return 1;
}

int Lua_GetBuildDate(lua_State* L)
{
    lua_pushstring(L, getBuildTimestamp().c_str());
    return 1;
}

/**
 * Lua library registration function for the Millennium module
 */
static const luaL_Reg millennium_lib[] = {
    /** Called *one time* after your plugin has finished bootstrapping. Its used to let Millennium know what plugins crashes/loaded etc.  */
    {"ready",                     Lua_EmitReadyMessage   },

    /** Add a CSS file to the browser webkit hook list */
    {"add_browser_css",           Lua_AddBrowserCss      },
    /** Add a JavaScript file to the browser webkit hook list */
    {"add_browser_js",            Lua_AddBrowserJs       },
    /** Remove a CSS or JavaScript file, passing the ModuleID provided from add_browser_js/css */
    {"remove_browser_module",     Lua_RemoveBrowserModule},

    {"get_user_settings",         Lua_GetUserSettings    },
    {"set_user_settings_key",     Lua_SetUserSettings    },
    /** Get the version of Millennium. In semantic versioning format. */
    {"version",                   Lua_GetVersionInfo     },
    /** Get the path to the Steam directory */
    {"steam_path",                Lua_GetSteamPath       },
    /** Get the path to the Millennium install directory */
    {"get_install_path",          Lua_GetInstallPath     },
    /** Get all the current stored logs from all loaded and previously loaded plugins during this instance */
    {"get_plugin_logs",           Lua_GetPluginLogs      },

    /** Call a JavaScript method on the frontend. */
    {"call_frontend_method",      Lua_CallFrontendMethod },
    /**
     * @note Internal Use Only
     * Used to toggle the status of a plugin, used in the Millennium settings page.
     */
    {"change_plugin_status",      Lua_TogglePluginStatus },
    /**
     * @note Internal Use Only
     * Used to check if a plugin is enabled, used in the Millennium settings page.
     */
    {"is_plugin_enabled",         Lua_IsPluginEnable     },

    /** For internal use, but can be used if its useful */
    {"__internal_get_build_date", Lua_GetBuildDate       },
    {NULL,                        NULL                   }  // Sentinel
};

/**
 * Register the Millennium module with Lua
 */
extern "C" int Lua_OpenMillenniumLibrary(lua_State* L)
{
    luaL_newlib(L, millennium_lib);
    return 1;
}
