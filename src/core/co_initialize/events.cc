#include "co_stub.h"
#include <sys/log.h>

bool EvaluateBackendStatus()
{
    static int backendReadyCount = 0;
    backendReadyCount++;

    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
    const std::size_t pluginCount = settingsStore->ParseAllPlugins().size();

    Logger.Log("Backend ready count {}, total plugins {}", backendReadyCount, pluginCount);

    if (backendReadyCount == pluginCount)
    {
        return true;
    }
    
    return false;
}

void CoInitializer::BackendCallbacks::Emit(const eEvents event_name)
{
    if (event_name == eEvents::CB_BACKENDS_READY)
    {
        Logger.Log("caught backends ready event");

        if (!EvaluateBackendStatus()) 
        {
            return;
        }
    }

    if (listeners.find(event_name) != listeners.end()) 
    {
        for (auto& callback : listeners[event_name]) 
        {
            Logger.Log("emitting event start event");
            callback();
        }
    }
    else
    {
        Logger.Log("no listeners found for event");
        missedEvents.push_back(event_name);
    }
}

void CoInitializer::BackendCallbacks::Listen(const eEvents event_name, EventCallback callback) 
{
    listeners[event_name].push_back(callback);

    if (std::find(missedEvents.begin(), missedEvents.end(), event_name) != missedEvents.end()) 
    {
        Logger.Log("found missed event, emitting");
        callback();

        // remove the event from the missed events list
        missedEvents.erase(std::remove(missedEvents.begin(), missedEvents.end(), event_name), missedEvents.end());
    }
}