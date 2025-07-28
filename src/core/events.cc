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
#include "internal_logger.h"
#include "co_spawn.h"


/**
 * Retrieves a comma-separated string of backend plugins that failed to load.
 *
 * @returns {std::string} - A string containing the names of backend plugins that failed to load, 
 *         formatted as a comma-separated list enclosed in square brackets. If no plugins failed 
 *         to load, the string "none" is returned.
 * 
 * This function iterates through a list of emitted plugins, checks if their loading event 
 * indicates failure (`BACKEND_LOAD_FAILED`), and appends their names to the result string.
 * If no plugins fail to load, it returns `"none"`.
 */
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

/**
 * Retrieves a comma-separated string of successfully loaded backend plugins.
 *
 * @returns {std::string} - A string containing the names of successfully loaded backend plugins, 
 *         formatted as a comma-separated list enclosed in square brackets. If no plugins were 
 *         successfully loaded, the string "none" is returned.
 * 
 * This function iterates through a list of emitted plugins, checks if their loading event 
 * indicates success (`BACKEND_LOAD_SUCCESS`), and appends their names to the result string.
 * If no plugins are successfully loaded, it returns `"none"`.
 */
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

/**
 * Evaluates the status of the backend loading process by comparing the number of enabled and loaded plugins.
 *
 * @returns {bool} - `true` if all enabled plugins have been processed (i.e., the number of loaded plugins matches
 *         the number of enabled plugins), otherwise `false`.
 * 
 */
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
        Logger.Log("Finished preparing backends: {} failed, {} successful", fmt::format(fmt::fg(color), failedBackends), fmt::format(fmt::fg(fmt::color::lime_green), successfulBackends));

        return true;
    }
    return false;
}

/**
 * Dispatches the backend status callbacks once the backend loading process is complete.
 * 
 * This function checks if the backend status evaluation is successful. If all enabled plugins 
 * are loaded, it proceeds to invoke and remove callbacks associated with the `ON_BACKEND_READY_EVENT`.
 * 
 * If there are no listeners for the `ON_BACKEND_READY_EVENT`, the event is recorded as missed and will
 * be attempted later. Once the callbacks are executed, the `isReadyForCallback` flag is set to `true`.
 * 
 * Error Handling:
 * - If the backend status is not ready, the function does nothing.
 */
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

/**
 * Handles the status update of a loaded backend plugin and dispatches the appropriate events.
 *
 * @param {PluginTypeSchema} plugin - The plugin whose loading status is being processed.
 */
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

    /** Check if its already emitted */
    if (std::find(this->emittedPlugins.begin(), this->emittedPlugins.end(), plugin) == this->emittedPlugins.end())
    {
        this->emittedPlugins.push_back(plugin);
        this->StatusDispatch();
    }
}

/**
 * Handles the status update of an unloaded backend plugin and dispatches the appropriate events.
 * 
 * @param {PluginTypeSchema} plugin - The plugin whose unloading status is being processed.
 */
void CoInitializer::BackendCallbacks::BackendUnLoaded(PluginTypeSchema plugin, bool isShuttingDown)
{
    assert(plugin.pluginName.empty() == false);

    // remove the plugin from the emitted list
    auto it = std::remove_if(this->emittedPlugins.begin(), this->emittedPlugins.end(), [&](const PluginTypeSchema& p) { 
        return p.pluginName == plugin.pluginName; 
    });

    if (it != this->emittedPlugins.end()) 
    {
        this->emittedPlugins.erase(it, this->emittedPlugins.end());
        Logger.Log("\033[1;35mSuccessfully unloaded {}\033[0m", plugin.pluginName);
    } 

    if (!isShuttingDown) 
    {
        Logger.Log("Running status dispatcher...");
        this->StatusDispatch();
    }
}

/**
 * Resets the backend callbacks by clearing all emitted plugins, listeners, and missed events.
 */
void CoInitializer::BackendCallbacks::Reset() 
{
    emittedPlugins.clear();
    listeners.clear();
    missedEvents.clear();
}

/**
 * Registers a callback for the backend load event.
 */
void CoInitializer::BackendCallbacks::RegisterForLoad(EventCallback callback) 
{
    Logger.Log("\033[1;35mRegistering for load event @ {}\033[0m", (void*)&callback);
    listeners[ON_BACKEND_READY_EVENT].push_back(callback);

    this->StatusDispatch();
}