#pragma once

#include "millennium/config.h"
#include "millennium/encoding.h"
#include "millennium/backend_mgr.h"
#include "millennium/plugin_ipc.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <string>

namespace plugin_config
{

inline std::string config_path(const std::string& plugin_name, const std::string& key = "")
{
    if (key.empty()) return "plugins." + plugin_name + ".config";
    return "plugins." + plugin_name + ".config." + key;
}

inline bool is_reserved_name(const std::string& name)
{
    return name == "enabledPlugins";
}

inline std::string validate_limits(const std::string& plugin_name, const std::string& key, const nlohmann::json& value)
{
    constexpr size_t MAX_KEYS = 256;
    constexpr size_t MAX_VALUE_BYTES = 256 * 1024;
    constexpr size_t MAX_KEY_BYTES = 512;

    if (is_reserved_name(plugin_name)) return "reserved plugin name: " + plugin_name;
    if (key.size() > MAX_KEY_BYTES) return "config key too long (max 512 bytes)";

    nlohmann::json current = CONFIG.get({ "plugins", plugin_name, "config" }, nlohmann::json::object());
    if (!current.contains(key) && current.size() >= MAX_KEYS)
        return "plugin config key limit exceeded (max 256)";

    if (value.dump().size() > MAX_VALUE_BYTES)
        return "plugin config value too large (max 256KB)";

    return "";
}

/* who wrote the change — skip echo-back to the originator */
enum class origin { frontend, lua_child, external };

using eval_js_fn = std::function<void(const std::string& script)>;

struct notify_targets
{
    eval_js_fn eval_js;
    std::shared_ptr<backend_manager> bm;
};

inline void notify_change(const notify_targets& t, origin src,
                          const std::string& plugin_name, const std::string& key,
                          const nlohmann::json& value)
{
    const std::string value_json = value.is_null() ? "null" : value.dump();

    if (src != origin::frontend && t.eval_js) {
        std::string script = fmt::format(
            R"(window.__millennium_plugin_config_changed__ && window.__millennium_plugin_config_changed__(atob("{}"), atob("{}"), atob("{}")))",
            Base64Encode(plugin_name), Base64Encode(key), Base64Encode(value_json));
        t.eval_js(script);
    }
    if (src != origin::lua_child && t.bm) {
        t.bm->notify_child(plugin_name, plugin_ipc::parent_method::CONFIG_CHANGED,
                           { { "key", key }, { "value", value } });
    }
}

struct result
{
    bool success;
    nlohmann::json value;
};

inline result get(const std::string& plugin_name, const std::string& key)
{
    if (is_reserved_name(plugin_name)) return { false, "reserved plugin name: " + plugin_name };
    if (key.empty()) return { false, "missing key" };
    nlohmann::json value = CONFIG.get_path(config_path(plugin_name, key));
    return { true, value };
}

inline result get_all(const std::string& plugin_name)
{
    if (is_reserved_name(plugin_name)) return { false, "reserved plugin name: " + plugin_name };
    nlohmann::json config = CONFIG.get({ "plugins", plugin_name, "config" }, nlohmann::json::object());
    return { true, config };
}

inline result set(const notify_targets& t, origin src,
                  const std::string& plugin_name, const std::string& key, const nlohmann::json& value)
{
    if (key.empty()) return { false, "missing key" };

    std::string err = validate_limits(plugin_name, key, value);
    if (!err.empty()) return { false, std::move(err) };

    CONFIG.set_path(config_path(plugin_name, key), value, true);
    CONFIG.save_to_disk();

    notify_change(t, src, plugin_name, key, value);
    return { true, nlohmann::json({ { "success", true } }) };
}

inline result del(const notify_targets& t, origin src,
                  const std::string& plugin_name, const std::string& key)
{
    if (is_reserved_name(plugin_name)) return { false, "reserved plugin name: " + plugin_name };
    if (key.empty()) return { false, "missing key" };

    nlohmann::json config = CONFIG.get({ "plugins", plugin_name, "config" }, nlohmann::json::object());
    config.erase(key);
    CONFIG.set({ "plugins", plugin_name, "config" }, config, true);
    CONFIG.save_to_disk();

    notify_change(t, src, plugin_name, key, nullptr);
    return { true, nlohmann::json({ { "success", true } }) };
}

inline result delete_all(const std::string& plugin_name)
{
    if (plugin_name.empty()) return { false, "missing pluginName" };
    if (is_reserved_name(plugin_name)) return { false, "reserved plugin name: " + plugin_name };

    CONFIG.set({ "plugins", plugin_name, "config" }, nlohmann::json::object(), true);
    CONFIG.save_to_disk();
    return { true, nlohmann::json({ { "success", true } }) };
}

} // namespace plugin_config
