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

#include "head/webkit.h"
#include "millennium/encode.h"
#include "millennium/http_hooks.h"
#include "millennium/logger.h"
#include "millennium/sysfs.h"
#include "millennium/env.h"
#include "nlohmann/json_fwd.hpp"

#include <unordered_set>

int Millennium::AddBrowserCss(const std::string& targetPath, const std::string& regex)
{
    g_hookedModuleId++;
    auto path = SystemIO::GetSteamPath() / "steamui" / targetPath;

    try {
        network_hook_ctl::GetInstance().add_hook({ path.generic_string(), std::regex(regex), network_hook_ctl::TagTypes::STYLESHEET, g_hookedModuleId });
    } catch (const std::regex_error& e) {
        LOG_ERROR("Attempted to add a browser module with invalid regex: {} ({})", ".*", e.what());
        return 0;
    }

    return g_hookedModuleId;
}

int Millennium::AddBrowserJs(const std::string& targetPath, const std::string& regex)
{
    g_hookedModuleId++;
    auto path = SystemIO::GetSteamPath() / "steamui" / targetPath;

    try {
        network_hook_ctl::GetInstance().add_hook({ path.generic_string(), std::regex(regex), network_hook_ctl::TagTypes::JAVASCRIPT, g_hookedModuleId });
    } catch (const std::regex_error& e) {
        LOG_ERROR("Attempted to add a browser module with invalid regex: {} ({})", ".*", e.what());
        return 0;
    }

    return g_hookedModuleId;
}

bool Millennium::RemoveBrowserModule(int id)
{
    return network_hook_ctl::GetInstance().remove_hook(id);
}

WebkitHookStore& WebkitHookStore::Instance()
{
    static WebkitHookStore instance;
    return instance;
}

void WebkitHookStore::Push(int moduleId)
{
    stack.push_back(moduleId);
}

void WebkitHookStore::UnregisterAll()
{
    for (int id : stack) {
        Millennium::RemoveBrowserModule(id);
    }
    stack.clear();
}

std::vector<WebkitItem> ParseConditionalPatches(const nlohmann::json& conditional_patches, const std::string& theme_name)
{
    std::vector<WebkitItem> webkit_items;

    nlohmann::json theme_conditions = CONFIG.GetNested("themes.conditions." + theme_name, nlohmann::json::object());

    if (conditional_patches.contains("Conditions")) {
        for (auto& [item, condition] : conditional_patches["Conditions"].items()) {
            if (!theme_conditions.contains(item)) continue;
            std::string current_value = theme_conditions[item].get<std::string>();

            if (condition.contains("values") && condition["values"].is_object()) {
                auto values = condition["values"];
                if (values.contains(current_value) && values[current_value].is_object()) {
                    for (auto& [value_key, control_flow] : values[current_value].items()) {
                        if (!control_flow.is_object()) continue;

                        if (control_flow.contains("affects") && control_flow["affects"].is_array()) {
                            for (auto& match_string : control_flow["affects"]) {
                                if (!match_string.is_string()) continue;

                                std::string target_path = control_flow.value("src", "");
                                webkit_items.push_back({ match_string.get<std::string>(), target_path, value_key });
                            }
                        }
                    }
                }
            }
        }
    }

    if (conditional_patches.contains("Patches") && conditional_patches["Patches"].is_array()) {
        for (auto& patch : conditional_patches["Patches"]) {
            if (!patch.contains("MatchRegexString")) continue;
            std::string regex = patch["MatchRegexString"].get<std::string>();

            for (auto& inject_type : { "TargetCss", "TargetJs" }) {
                if (patch.contains(inject_type)) {
                    nlohmann::json targets = patch[inject_type];
                    if (targets.is_string()) {
                        targets = nlohmann::json::array({ targets });
                    }
                    for (auto& target : targets) {
                        if (!target.is_string()) continue;
                        webkit_items.push_back({ regex, target.get<std::string>(), inject_type });
                    }
                }
            }
        }
    }

    std::unordered_set<std::string> seen;
    std::vector<WebkitItem> unique_items;

    for (auto& item : webkit_items) {
        std::string identifier = item.matchString + "|" + item.targetPath;
        if (seen.insert(identifier).second) {
            unique_items.push_back(item);
        }
    }

    return unique_items;
}

int AddBrowserCss(const std::string& css_path, const std::string& regex)
{
    int id = Millennium::AddBrowserCss(css_path, regex);
    WebkitHookStore::Instance().Push(id);
    return id;
}

int AddBrowserJs(const std::string& js_path, const std::string& regex)
{
    int id = Millennium::AddBrowserJs(js_path, regex);
    WebkitHookStore::Instance().Push(id);
    return id;
}

void AddConditionalData(const std::string& path, const nlohmann::json& data, const std::string& theme_name)
{
    try {
        auto parsed_patches = ParseConditionalPatches(data, theme_name);

        for (auto& patch : parsed_patches) {
            if (patch.fileType == "TargetCss" && !patch.targetPath.empty() && !patch.matchString.empty()) {
                std::string full_path = (std::filesystem::path(path) / patch.targetPath).generic_string();
                AddBrowserCss(full_path, patch.matchString);
            } else if (patch.fileType == "TargetJs" && !patch.targetPath.empty() && !patch.matchString.empty()) {
                std::string full_path = (std::filesystem::path(path) / patch.targetPath).generic_string();
                AddBrowserJs(full_path, patch.matchString);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error adding conditional data: {}", e.what());
    }
}

void IsolatedPluginWebkitStore::get()
{
    nlohmann::ordered_json response;

    for (const auto& item : m_webkitStore) {
        /** for performance, we bite the bullet and assume the file exists as its verified when its initially added. */
        Base64Encode(SystemIO::ReadFileBytesSync(item.absWebkitPath.string()));
    }
}
