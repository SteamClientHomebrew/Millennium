#include "co_stub.h"
#include <sys/log.h>
#include <core/py_controller/co_spawn.h>

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
    const std::size_t pluginCount = settingsStore->GetEnabledBackends().size();

    if (this->emittedPlugins.size() == pluginCount)
    {
        const std::string failedBackends = GetFailedBackendsStr();
        const std::string successfulBackends = GetSuccessfulBackendsStr();

        auto color = failedBackends == "none" ? fmt::color::lime_green : fmt::color::orange_red;

        Logger.LogHead("All backends are ready!", fg(color));
        Logger.LogItem("Failed", failedBackends, false, fg(color));
        Logger.LogItem("Success", successfulBackends, false, fg(color));
        Logger.LogItem("Total", std::to_string(pluginCount), true, fg(color));
        std::cout << std::endl;

        return true;
    }
    return false;
}

void CoInitializer::BackendCallbacks::StatusDipatch()
{
    if (this->EvaluateBackendStatus())
    {
        if (listeners.find(ON_BACKEND_READY_EVENT) != listeners.end()) 
        {
            for (auto& callback : listeners[ON_BACKEND_READY_EVENT]) 
            {
                callback();
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
    Logger.Log("{} {}", plugin.pluginName, plugin.event == BACKEND_LOAD_SUCCESS ? "loaded" : "failed");
    this->emittedPlugins.push_back(plugin);

    this->StatusDipatch();
}

void CoInitializer::BackendCallbacks::Reset() 
{
    emittedPlugins.clear();
    listeners.clear();
    missedEvents.clear();
}

void CoInitializer::BackendCallbacks::RegisterForLoad(EventCallback callback) 
{
    Logger.Log("Registering for load event @ {}", (void*)&callback);
    listeners[ON_BACKEND_READY_EVENT].push_back(callback);

    if (isReadyForCallback) 
    {
        Logger.Log("Force firing [{}] callback as the target event was already called.", __FUNCTION__);

        callback();
        return;
    }

    if (std::find(missedEvents.begin(), missedEvents.end(), ON_BACKEND_READY_EVENT) != missedEvents.end()) 
    {
        callback();
        missedEvents.erase(std::remove(missedEvents.begin(), missedEvents.end(), ON_BACKEND_READY_EVENT), missedEvents.end());
    }
}