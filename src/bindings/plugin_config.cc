/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

#include "millennium/config.h"
#include "millennium/encoding.h"
#include "millennium/plugin_config.h"

#include <format>

namespace plugin_config
{

bool is_reserved_name(const std::string& name)
{
    return name == "enabledPlugins";
}

std::string validate_limits(const std::string& plugin_name, const std::string& key, const nlohmann::json& value)
{
    constexpr size_t MAX_KEYS = 256;
    constexpr size_t MAX_VALUE_BYTES = 256 * 1024;
    constexpr size_t MAX_KEY_BYTES = 512;

    if (is_reserved_name(plugin_name)) return "reserved plugin name: " + plugin_name;
    if (key.size() > MAX_KEY_BYTES) return "config key too long (max 512 bytes)";

    nlohmann::json current = CONFIG.get({ "plugins", plugin_name, "config" }, nlohmann::json::object());
    if (!current.contains(key) && current.size() >= MAX_KEYS) return "plugin config key limit exceeded (max 256)";

    if (value.dump().size() > MAX_VALUE_BYTES) return "plugin config value too large (max 256KB)";

    return "";
}

void notify_change(const notify_targets& t, origin src, const std::string& plugin_name, const std::string& key, const nlohmann::json& value)
{
    const std::string value_json = value.is_null() ? "null" : value.dump();

    if (src != origin::frontend && t.eval_js) {
        std::string script = std::format(R"(window.__millennium_plugin_config_changed__ && window.__millennium_plugin_config_changed__(atob("{}"), atob("{}"), atob("{}")))",
                                         Base64Encode(plugin_name), Base64Encode(key), Base64Encode(value_json));
        t.eval_js(script);
    }
    if (src != origin::lua_child && t.bm) {
        const json params = {
            { "key",   key   },
            { "value", value }
        };
        t.bm->notify_child(plugin_name, plugin_ipc::parent_method::CONFIG_CHANGED, params);
    }
}

result get(const std::string& plugin_name, const std::string& key)
{
    if (is_reserved_name(plugin_name)) return { false, "reserved plugin name: " + plugin_name };
    if (key.empty()) return { false, "missing key" };
    nlohmann::json value = CONFIG.get({ "plugins", plugin_name, "config", key });
    return { true, value };
}

result get_all(const std::string& plugin_name)
{
    if (is_reserved_name(plugin_name)) return { false, "reserved plugin name: " + plugin_name };
    nlohmann::json config = CONFIG.get({ "plugins", plugin_name, "config" }, nlohmann::json::object());
    return { true, config };
}

result set(const notify_targets& t, origin src, const std::string& plugin_name, const std::string& key, const nlohmann::json& value)
{
    if (key.empty()) return { false, "missing key" };

    std::string err = validate_limits(plugin_name, key, value);
    if (!err.empty()) return { false, std::move(err) };

    CONFIG.set({ "plugins", plugin_name, "config", key }, value, true);
    CONFIG.save_to_disk();

    notify_change(t, src, plugin_name, key, value);
    return {
        true, nlohmann::json({ { "success", true } }
         )
    };
}

result del(const notify_targets& t, origin src, const std::string& plugin_name, const std::string& key)
{
    if (is_reserved_name(plugin_name)) return { false, "reserved plugin name: " + plugin_name };
    if (key.empty()) return { false, "missing key" };

    nlohmann::json config = CONFIG.get({ "plugins", plugin_name, "config" }, nlohmann::json::object());
    config.erase(key);
    CONFIG.set({ "plugins", plugin_name, "config" }, config, true);
    CONFIG.save_to_disk();

    notify_change(t, src, plugin_name, key, nullptr);
    return {
        true, nlohmann::json({ { "success", true } }
         )
    };
}

result delete_all(const std::string& plugin_name)
{
    if (plugin_name.empty()) return { false, "missing pluginName" };
    if (is_reserved_name(plugin_name)) return { false, "reserved plugin name: " + plugin_name };

    CONFIG.set({ "plugins", plugin_name, "config" }, nlohmann::json::object(), true);
    CONFIG.save_to_disk();
    return {
        true, nlohmann::json({ { "success", true } }
         )
    };
}

} // namespace plugin_config
