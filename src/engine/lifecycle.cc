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

#include "millennium/life_cycle.h"
#include "millennium/logger.h"

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
std::string backend_event_dispatcher::str_get_failed_backends()
{
    std::string failedBackends;

    for (const auto& plugin : this->emittedPlugins) {
        if (plugin.event == backend_ready_event::BACKEND_LOAD_FAILED) {
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
std::string backend_event_dispatcher::str_get_successful_backends()
{
    std::string successfulBackends;

    for (const auto& plugin : this->emittedPlugins) {
        if (plugin.event == backend_ready_event::BACKEND_LOAD_SUCCESS) {
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
bool backend_event_dispatcher::are_all_backends_ready()
{
    const std::size_t pluginCount = m_settings_store->get_enabled_backends().size();

    logger.log("\033[1;35mEnabled Plugins: {}, Loaded Plugins : {}\033[0m", pluginCount, emittedPlugins.size());

    if (this->emittedPlugins.size() == pluginCount) {
        const std::string failedBackends = str_get_failed_backends();
        const std::string successfulBackends = str_get_successful_backends();

        auto color = failedBackends == "none" ? fmt::color::lime_green : fmt::color::orange_red;
        logger.log("Finished preparing backends: {} failed, {} successful", fmt::format(fmt::fg(color), failedBackends),
                   fmt::format(fmt::fg(fmt::color::lime_green), successfulBackends));

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
void backend_event_dispatcher::update()
{
    if (this->are_all_backends_ready()) {
        std::unique_lock<std::mutex> lock(listenersMutex);

        // Check if listeners map contains our event
        auto it = listeners.find(ON_BACKEND_READY_EVENT);
        if (it != listeners.end()) {
            // Copy callbacks to avoid iterator invalidation if a callback modifies the vector
            std::vector<event_cb> callbacksCopy = it->second;
            it->second.clear();

            // Release the lock before executing callbacks to avoid deadlocks
            lock.unlock();

            for (auto& cb : callbacksCopy) {
                logger.log("\033[1;35mInvoking & removing on load event @ {}\033[0m", (void*)&cb);
                try {
                    cb();
                } catch (const std::exception& e) {
                    logger.warn("Exception during callback: {}", e.what());
                } catch (...) {
                    logger.warn("Unknown exception during callback");
                }
            }
            is_ready_for_cb = true;
        } else {
            missedEvents.push_back(ON_BACKEND_READY_EVENT);
        }
    }
}

/**
 * Handles the status update of a loaded backend plugin and dispatches the appropriate events.
 *
 * @param {PluginTypeSchema} plugin - The plugin whose loading status is being processed.
 */
void backend_event_dispatcher::backend_loaded_event_hdlr(plugin_t plugin)
{
    if (plugin.event == backend_ready_event::BACKEND_LOAD_FAILED) {
        logger.warn("Failed to load '{}'", plugin.pluginName);
    } else if (plugin.event == backend_ready_event::BACKEND_LOAD_SUCCESS) {
        logger.log("Successfully loaded '{}'", plugin.pluginName);
    }

    /** Check if its already emitted */
    if (std::find(this->emittedPlugins.begin(), this->emittedPlugins.end(), plugin) == this->emittedPlugins.end()) {
        this->emittedPlugins.push_back(plugin);
        this->update();
    }
}

/**
 * Handles the status update of an unloaded backend plugin and dispatches the appropriate events.
 *
 * @param {PluginTypeSchema} plugin - The plugin whose unloading status is being processed.
 */
void backend_event_dispatcher::backend_unloaded_event_hdlr(plugin_t plugin, bool isShuttingDown)
{
    assert(plugin.pluginName.empty() == false);

    // Use mutex to protect emittedPlugins access
    {
        std::unique_lock<std::mutex> lock(listenersMutex);

        // remove the plugin from the emitted list
        auto it = std::remove_if(this->emittedPlugins.begin(), this->emittedPlugins.end(), [&](const plugin_t& p) { return p.pluginName == plugin.pluginName; });

        if (it != this->emittedPlugins.end()) {
            this->emittedPlugins.erase(it, this->emittedPlugins.end());
            logger.log("\033[1;35mSuccessfully unloaded {}\033[0m", plugin.pluginName);
        }
    }

    if (!isShuttingDown) {
        logger.log("Running status dispatcher...");
        this->update();
    }
}

/**
 * Resets the backend callbacks by clearing all emitted plugins, listeners, and missed events.
 */
void backend_event_dispatcher::reset()
{
    std::unique_lock<std::mutex> lock(listenersMutex);
    emittedPlugins.clear();
    listeners.clear();
    missedEvents.clear();
}

/**
 * Registers a callback for the backend load event.
 */
void backend_event_dispatcher::on_all_backends_ready(event_cb callback)
{
    logger.log("\033[1;35mRegistering for load event @ {}\033[0m", (void*)&callback);
    {
        std::unique_lock<std::mutex> lock(listenersMutex);
        listeners[ON_BACKEND_READY_EVENT].push_back(callback);
    }

    this->update();
}
