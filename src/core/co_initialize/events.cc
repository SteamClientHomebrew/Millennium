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
        if (!EvaluateBackendStatus()) 
        {
            return;
        }
    }

    if (listeners.find(event_name) != listeners.end()) 
    {
        for (auto& callback : listeners[event_name]) 
        {
            callback();
        }
    }
}

void CoInitializer::BackendCallbacks::Listen(const eEvents event_name, EventCallback callback) 
{
    listeners[event_name].push_back(callback);
}