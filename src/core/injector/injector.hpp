#pragma once
#include <string>
#include <core/steam/cef_manager.hpp>
#include <nlohmann/json.hpp>

struct item {
    std::string filePath;
    steam_cef_manager::script_type type;
};

namespace injector
{
    std::vector<item> find_patches(const nlohmann::json& patch, std::string cefctx_title);
}