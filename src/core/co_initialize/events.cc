#include "co_stub.h"
#include <sys/log.h>

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

    return failedBackends.empty() ? 
        "none" : 
        fmt::format("[{}]", failedBackends.substr(0, failedBackends.size() - 2));
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

    return successfulBackends.empty() ? 
        "none" : 
        fmt::format("[{}]", successfulBackends.substr(0, successfulBackends.size() - 2));
}

bool CoInitializer::BackendCallbacks::EvaluateBackendStatus()
{
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
    const std::size_t pluginCount = settingsStore->ParseAllPlugins().size();

    if (this->emittedPlugins.size() == pluginCount)
    {
        Logger.LogHead("all backends are ready!", fg(fmt::color::lime_green));
        Logger.LogItem("failed", GetFailedBackendsStr(), false, fg(fmt::color::lime_green));
        Logger.LogItem("success", GetSuccessfulBackendsStr(), false, fg(fmt::color::lime_green));
        Logger.LogItem("total", std::to_string(pluginCount), true, fg(fmt::color::lime_green));

        return true;
    }
    return false;
}

void CoInitializer::BackendCallbacks::BackendLoaded(PluginTypeSchema plugin)
{
    Logger.Log("{} {}", plugin.pluginName, plugin.event == BACKEND_LOAD_SUCCESS ? "loaded" : "failed");
    this->emittedPlugins.push_back(plugin);

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
    emittedPlugins.clear();
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