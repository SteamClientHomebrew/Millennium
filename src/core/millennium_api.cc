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

#include "millennium/millennium_api.h"
#include "millennium/backend_init.h"
#include "millennium/backend_mgr.h"
#include "millennium/encode.h"
#include "millennium/http_hooks.h"
#include "millennium/init.h"
#include "millennium/plugin_logger.h"

void Millennium_TogglePluginStatus(const std::vector<PluginStatus>& plugins)
{
    BackendManager& manager = BackendManager::GetInstance();
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();

    std::unordered_map<std::string, bool> pluginStatusMap;
    for (const auto& plugin : plugins) {
        pluginStatusMap[plugin.pluginName] = plugin.enabled;
    }

    bool hasEnableRequests = false;
    std::vector<std::string> pluginsToDisable;

    for (const auto& entry : pluginStatusMap) {
        const std::string& pluginName = entry.first;
        const bool newStatus = entry.second;

        settingsStore->TogglePluginStatus(pluginName.c_str(), newStatus);

        if (newStatus) {
            hasEnableRequests = true;
            Logger.Log("requested to enable plugin [{}]", pluginName);
        } else {
            pluginsToDisable.push_back(pluginName);
            Logger.Log("requested to disable plugin [{}]", pluginName);
        }
    }

    if (hasEnableRequests) {
        g_pluginLoader->StartBackEnds(manager);
    }

    for (const auto& pluginName : pluginsToDisable) {
        const auto backendType = manager.GetPluginBackendType(pluginName);

        if (backendType == SettingsStore::PluginBackendType::Lua) {
            manager.DestroyLuaInstance(pluginName);
        } else if (backendType == SettingsStore::PluginBackendType::Python) {
            manager.DestroyPythonInstance(pluginName);
        }
    }

    CoInitializer::ReInjectFrontendShims(g_pluginLoader, true);
}

unsigned long long Millennium_AddBrowserModule(const char* moduleItem, const char* regexSelector, HttpHookManager::TagTypes type)
{
    g_hookedModuleId++;
    auto path = SystemIO::GetSteamPath() / "steamui" / moduleItem;

    try {
        HttpHookManager::GetInstance().AddHook({ path.generic_string(), std::regex(regexSelector), type, g_hookedModuleId });
    } catch (const std::regex_error& e) {
        LOG_ERROR("Attempted to add a browser module with invalid regex: {} ({})", regexSelector, e.what());
        ErrorToLogger("executor", fmt::format("Failed to add browser module with invalid regex: {} ({})", regexSelector, e.what()));
        return 0;
    }

    return g_hookedModuleId;
}

nlohmann::json Millennium_GetPluginLogs()
{
    nlohmann::json logData = nlohmann::json::array();
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();

    std::vector<SettingsStore::PluginTypeSchema> plugins = settingsStore->ParseAllPlugins();

    for (auto& logger : g_loggerList) {
        nlohmann::json logDataItem;

        for (auto [message, logLevel] : logger->CollectLogs()) {
            logDataItem.push_back({
                { "message", Base64Encode(message) },
                { "level",   logLevel              }
            });
        }

        std::string pluginName = logger->GetPluginName(false);

        for (auto& plugin : plugins) {
            if (plugin.pluginJson.contains("name") && plugin.pluginJson["name"] == logger->GetPluginName(false)) {
                pluginName = plugin.pluginJson.value("common_name", pluginName);
                break;
            }
        }

        // Handle package manager plugin
        if (pluginName == "pipx") pluginName = "Package Manager";

        logData.push_back({
            { "name", pluginName  },
            { "logs", logDataItem }
        });
    }

    return logData;
}