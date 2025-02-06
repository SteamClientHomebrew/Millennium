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

#include "co_stub.h"
#include "log.h"
#include "co_spawn.h"

std::string CoInitializer::BackendCallbacks::GetFailedBackendsStr()
{
    std::string failedBackends;

    for (const auto& plugin : this->emittedPlugins)
    {
        if (plugin.event == BACKEND_LOAD_FAILED)
        {
            failedBackends += plugin.pluginName + ", ";
        }
    }

    return failedBackends.empty() ? "none" : fmt::format("[{}]", failedBackends.substr(0, failedBackends.size() - 2));
}

std::string CoInitializer::BackendCallbacks::GetSuccessfulBackendsStr()
{
    std::string successfulBackends;

    for (const auto& plugin : this->emittedPlugins)
    {
        if (plugin.event == BACKEND_LOAD_SUCCESS)
        {
            successfulBackends += plugin.pluginName + ", ";
        }
    }

    return successfulBackends.empty() ? "none" : fmt::format("[{}]", successfulBackends.substr(0, successfulBackends.size() - 2));
}

bool CoInitializer::BackendCallbacks::EvaluateBackendStatus()
{
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
    const std::size_t pluginCount = settingsStore->GetEnabledBackends().size();

    Logger.Log("\033[1;35mEnabled Plugins: {}, Loaded Plugins : {}\033[0m", pluginCount, emittedPlugins.size());

    if (this->emittedPlugins.size() == pluginCount)
    {
        const std::string failedBackends = GetFailedBackendsStr();
        const std::string successfulBackends = GetSuccessfulBackendsStr();

        auto color = failedBackends == "none" ? fmt::color::lime_green : fmt::color::orange_red;
        Logger.Log("Finished preparing backends...");

        return true;
    }
    return false;
}

void CoInitializer::BackendCallbacks::StatusDispatch()
{
    if (this->EvaluateBackendStatus())
    {
        if (listeners.find(ON_BACKEND_READY_EVENT) != listeners.end()) 
        {
            auto& callbacks = listeners[ON_BACKEND_READY_EVENT];

            for (auto callbackIterator = callbacks.begin(); callbackIterator != callbacks.end(); )
            {
                Logger.Log("\033[1;35mInvoking & removing on load event @ {}\033[0m", (void*)&(*callbackIterator));
                (*callbackIterator)(); // Call the callback
                callbackIterator = callbacks.erase(callbackIterator); // Remove callback after calling
            }
            isReadyForCallback = true;
        }
        else
        {
            missedEvents.push_back(ON_BACKEND_READY_EVENT);
        }
    }
}

void CoInitializer::BackendCallbacks::BackendLoaded(PluginTypeSchema plugin)
{
    if (plugin.event == BACKEND_LOAD_FAILED)
    {
        Logger.Warn("Failed to load '{}'", plugin.pluginName);
    }
    else if (plugin.event == BACKEND_LOAD_SUCCESS)
    {
        Logger.Log("Successfully loaded '{}'", plugin.pluginName);
    }

    this->emittedPlugins.push_back(plugin);

    this->StatusDispatch();
}

void CoInitializer::BackendCallbacks::BackendUnLoaded(PluginTypeSchema plugin)
{
    // remove the plugin from the emitted list
    this->emittedPlugins.erase(std::remove_if(this->emittedPlugins.begin(), this->emittedPlugins.end(), 
        [&](const PluginTypeSchema& p) { return p.pluginName == plugin.pluginName; }), this->emittedPlugins.end());

    
    Logger.Log("\033[1;35mSuccessfully unloaded {}\033[0m", plugin.pluginName);

    this->StatusDispatch();
}

void CoInitializer::BackendCallbacks::Reset() 
{
    emittedPlugins.clear();
    listeners.clear();
    missedEvents.clear();
}

void CoInitializer::BackendCallbacks::RegisterForLoad(EventCallback callback) 
{
    Logger.Log("\033[1;35mRegistering for load event @ {}\033[0m", (void*)&callback);
    listeners[ON_BACKEND_READY_EVENT].push_back(callback);

    this->StatusDispatch();
}