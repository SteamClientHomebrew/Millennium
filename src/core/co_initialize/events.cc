#include "co_stub.h"
#include <sys/log.h>

bool CoInitializer::BackendCallbacks::EvaluateBackendStatus()
{
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
    const std::size_t pluginCount = settingsStore->ParseAllPlugins().size();

    Logger.Log("failed: {}, success: {}, total plugins: {}", backendFailedCount, backendReadyCount, pluginCount);

    if ((backendReadyCount + backendFailedCount) == pluginCount)
    {
        return true;
    }
    return false;
}

void CoInitializer::BackendCallbacks::BackendLoaded(PluginTypeSchema plugin)
{
    Logger.Log("{} {}", plugin.pluginName, plugin.event == BACKEND_LOAD_SUCCESS ? "loaded" : "failed");

    switch (plugin.event)
    {
        case BACKEND_LOAD_SUCCESS: backendReadyCount++;  break;
        case BACKEND_LOAD_FAILED:  backendFailedCount++; break;
    }

    if (this->EvaluateBackendStatus())
    {
        if (listeners.find(ON_BACKEND_READY_EVENT) != listeners.end()) 
        {
            for (auto& callback : listeners[ON_BACKEND_READY_EVENT]) 
            {
                callback();
            }
        }
        else
        {
            missedEvents.push_back(ON_BACKEND_READY_EVENT);
        }
    }
}

void CoInitializer::BackendCallbacks::Reset() 
{
    backendReadyCount = 0;
    backendFailedCount = 0;
    listeners.clear();
    missedEvents.clear();
}

void CoInitializer::BackendCallbacks::RegisterForLoad(EventCallback callback) 
{
    listeners[ON_BACKEND_READY_EVENT].push_back(callback);

    if (std::find(missedEvents.begin(), missedEvents.end(), ON_BACKEND_READY_EVENT) != missedEvents.end()) 
    {
        Logger.Log("found missed event, emitting");
        callback();

        // remove the event from the missed events list
        missedEvents.erase(std::remove(missedEvents.begin(), missedEvents.end(), ON_BACKEND_READY_EVENT), missedEvents.end());
    }
}