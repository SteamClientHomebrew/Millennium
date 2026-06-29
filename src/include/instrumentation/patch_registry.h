#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct PatchTransform
{
    std::string match;
    std::string replace;
};

struct PatchEntry
{
    std::string plugin;
    std::string file;
    std::string find;
    std::vector<PatchTransform> transforms;
};

void patch_registry_set(const std::string& plugin, std::vector<PatchEntry> patches);
void patch_registry_remove(const std::string& plugin);
nlohmann::json patch_registry_to_json();
