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
#include "millennium/http_hooks.h"
#include "millennium/logger.h"
#include "millennium/filesystem.h"
#include "millennium/environment.h"
#include "nlohmann/json_fwd.hpp"

#include <unordered_set>

theme_webkit_mgr::theme_webkit_mgr(std::shared_ptr<plugin_manager> settings_store, std::shared_ptr<network_hook_ctl> network_hook_ctl)
    : m_settings_store(std::move(settings_store)), m_network_hook_ctl(std::move(network_hook_ctl))
{
}

void theme_webkit_mgr::unregister_all()
{
    for (auto& id : m_registered_hooks) {
        m_network_hook_ctl->remove_hook(id);
    }

    m_registered_hooks.clear();
}

std::vector<theme_webkit_mgr::webkit_item> theme_webkit_mgr::parse_conditional_data(const nlohmann::json& conditional_patches, const std::string& theme_name)
{
    std::vector<theme_webkit_mgr::webkit_item> webkit_items;

    nlohmann::json theme_conditions = CONFIG.get("themes.conditions." + theme_name, nlohmann::json::object());

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
    std::vector<theme_webkit_mgr::webkit_item> unique_items;

    for (auto& item : webkit_items) {
        std::string identifier = item.match_pattern + "|" + item.path;
        if (seen.insert(identifier).second) {
            unique_items.push_back(item);
        }
    }

    return unique_items;
}

unsigned long long theme_webkit_mgr::add_browser_hook(const std::string& path, const std::string& regex, network_hook_ctl::TagTypes type)
{
    unsigned long long hook_id = m_network_hook_ctl->add_hook({ (platform::get_steam_path() / "steamui" / path).generic_string(), std::regex(regex), type });
    m_registered_hooks.push_back(hook_id);
    return hook_id;
}

bool theme_webkit_mgr::remove_browser_hook(unsigned long long hookId)
{
    auto it = std::find(m_registered_hooks.begin(), m_registered_hooks.end(), hookId);
    if (it != m_registered_hooks.end()) {
        m_registered_hooks.erase(it);
        return m_network_hook_ctl->remove_hook(hookId);
    }
    return false;
}

void theme_webkit_mgr::add_conditional_data(const std::string& path, const nlohmann::json& data, const std::string& theme_name)
{
    try {
        auto parsed_patches = this->parse_conditional_data(data, theme_name);

        for (auto& patch : parsed_patches) {
            if (patch.fileType == "TargetCss" && !patch.path.empty() && !patch.match_pattern.empty()) {
                std::string full_path = (std::filesystem::path(path) / patch.path).generic_string();
                this->add_browser_hook(full_path, patch.match_pattern, network_hook_ctl::TagTypes::STYLESHEET);
            } else if (patch.fileType == "TargetJs" && !patch.path.empty() && !patch.match_pattern.empty()) {
                std::string full_path = (std::filesystem::path(path) / patch.path).generic_string();
                this->add_browser_hook(full_path, patch.match_pattern, network_hook_ctl::TagTypes::JAVASCRIPT);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error adding conditional data: {}", e.what());
    }
}
