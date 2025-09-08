#pragma once
#include "millennium/http_hooks.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct PluginStatus
{
    std::string pluginName;
    bool enabled;
};

void Millennium_TogglePluginStatus(const std::vector<PluginStatus>& plugins);
unsigned long long Millennium_AddBrowserModule(const char* moduleItem, const char* regexSelector, HttpHookManager::TagTypes type);
nlohmann::json Millennium_GetPluginLogs();