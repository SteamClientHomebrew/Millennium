#pragma once

#include "millennium/backend_mgr.h"

#include <nlohmann/json.hpp>
#include <functional>
#include <string>

namespace plugin_config
{

bool is_reserved_name(const std::string& name);
std::string validate_limits(const std::string& plugin_name, const std::string& key, const nlohmann::json& value);

/* who wrote the change — skip echo-back to the originator */
enum class origin
{
    frontend,
    lua_child,
    external
};

using eval_js_fn = std::function<void(const std::string& script)>;

struct notify_targets
{
    eval_js_fn eval_js;
    std::shared_ptr<backend_manager> bm;
};

void notify_change(const notify_targets& t, origin src, const std::string& plugin_name, const std::string& key, const nlohmann::json& value);

struct result
{
    bool success;
    nlohmann::json value;
};

result get(const std::string& plugin_name, const std::string& key);
result get_all(const std::string& plugin_name);
result set(const notify_targets& t, origin src, const std::string& plugin_name, const std::string& key, const nlohmann::json& value);
result del(const notify_targets& t, origin src, const std::string& plugin_name, const std::string& key);
result delete_all(const std::string& plugin_name);

} // namespace plugin_config
