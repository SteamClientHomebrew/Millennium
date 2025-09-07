#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct PluginStatus
{
    std::string pluginName;
    bool enabled;
};

void Millennium_TogglePluginStatus(const std::vector<PluginStatus>& plugins);
nlohmann::json Millennium_GetPluginLogs();