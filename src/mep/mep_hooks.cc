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

#include "mep_hooks.h"
#include "mep_message.h"
#include "ffi_recorder.h"
#include "console_capture.h"
#include "crash_event_bus.h"

#include "millennium/plugin_loader.h"
#include "millennium/plugin_manager.h"
#include "millennium/backend_mgr.h"
#include "millennium/logger.h"
#include "millennium/plugin_config.h"
#include "state/shared_memory.h"

#include <fstream>

namespace mep
{
namespace
{
json plugin_to_json(plugin_manager& pm, backend_manager& bm, const plugin_manager::plugin_t& p)
{
    const json& pj = p.plugin_json;
    return {
        { "name", p.plugin_name },
        { "common_name", pj.value("common_name", p.plugin_name) },
        { "description", pj.value("description", std::string{}) },
        { "enabled", pm.is_enabled(p.plugin_name) },
        { "running", bm.is_any_backend_running(p.plugin_name) },
        { "version", pj.contains("version") ? json(pj["version"]) : json(nullptr) },
        { "author", pj.value("author", std::string{}) },
        { "config", pj },
        { "path", p.plugin_base_dir.generic_string() },
    };
}

const plugin_manager::plugin_t* find_plugin(const std::vector<plugin_manager::plugin_t>& plugins, const std::string& name)
{
    for (const auto& p : plugins) {
        if (p.plugin_name == name) return &p;
    }
    return nullptr;
}

std::optional<std::string> require_string(const request_t& req, const std::string& key)
{
    if (req.params.is_null() || !req.params.contains(key)) return std::nullopt;
    if (!req.params[key].is_string()) return std::nullopt;
    return req.params[key].get<std::string>();
}

const char* level_str(logger_base::log_level lv)
{
    switch (lv) {
        case logger_base::log_level::warn:
            return "warn";
        case logger_base::log_level::error:
            return "error";
        default:
            return "info";
    }
}
} // namespace

void register_mep_handlers(router& router, std::shared_ptr<plugin_loader> loader)
{
    router.register_handler("plugin.list", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto pm = loader->get_plugin_manager();
        auto bm = loader->get_backend_manager();
        json list = json::array();
        for (const auto& p : pm->get_all_plugins()) {
            if (!p.is_internal) list.push_back(plugin_to_json(*pm, *bm, p));
        }
        return response_t::ok(req.id, list);
    });

    router.register_handler("plugin.get", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        auto pm = loader->get_plugin_manager();
        auto bm = loader->get_backend_manager();
        const auto plugins = pm->get_all_plugins();
        const auto* p = find_plugin(plugins, *name);
        if (!p) return response_t::err(req.id, "plugin not found: " + *name);

        return response_t::ok(req.id, plugin_to_json(*pm, *bm, *p));
    });

    router.register_handler("plugin.enable", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        loader->set_plugin_enable(*name, true);
        return response_t::ok(req.id, {
                                          { "name",    *name },
                                          { "enabled", true  }
        });
    });

    router.register_handler("plugin.disable", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        loader->set_plugin_enable(*name, false);
        return response_t::ok(req.id, {
                                          { "name",    *name },
                                          { "enabled", false }
        });
    });

    router.register_handler("plugin.start", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        auto bm = loader->get_backend_manager();
        if (bm->is_any_backend_running(*name)) return response_t::err(req.id, "plugin backend is already running: " + *name);

        loader->set_plugin_enable(*name, true);
        return response_t::ok(req.id, {
                                          { "name",    *name                             },
                                          { "running", bm->is_any_backend_running(*name) },
        });
    });

    router.register_handler("plugin.stop", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        loader->get_backend_manager()->destroy_plugin(*name);
        return response_t::ok(req.id, {
                                          { "name",    *name },
                                          { "running", false }
        });
    });

    router.register_handler("plugin.restart", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        auto bm = loader->get_backend_manager();
        bm->destroy_plugin(*name);
        loader->set_plugin_enable(*name, true);
        return response_t::ok(req.id, {
                                          { "name",    *name                             },
                                          { "running", bm->is_any_backend_running(*name) },
        });
    });

    router.register_handler("plugin.status", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        auto pm = loader->get_plugin_manager();
        auto bm = loader->get_backend_manager();
        const auto plugins = pm->get_all_plugins();
        const auto* p = find_plugin(plugins, *name);
        if (!p) return response_t::err(req.id, "plugin not found: " + *name);

        const json result = {
            { "name",    *name                             },
            { "running", bm->is_any_backend_running(*name) },
            { "enabled", pm->is_enabled(*name)             },
        };

        return response_t::ok(req.id, result);
    });

    // plugin.logs — subscribe to a plugin's log stream.
    //
    // initial response:
    //   { "subscription_id": "<id>", "logs": [ { "level", "message" }, ... ] }
    //
    // subsequent push events (server → client, unsolicited):
    //   { "type": "event", "subscription_id": "<id>",
    //     "plugin": "<name>", "data": { "level", "message" } }
    //
    // params: { name, min_level? }
    router.register_handler("plugin.logs", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.logs requires a live connection");

        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        const std::string min_lv_str =
            (!req.params.is_null() && req.params.contains("min_level") && req.params["min_level"].is_string()) ? req.params["min_level"].get<std::string>() : "info";

        const logger_base::log_level min_lv = (min_lv_str == "error")  ? logger_base::log_level::error
                                              : (min_lv_str == "warn") ? logger_base::log_level::warn
                                                                       : logger_base::log_level::info;
        plugin_logger* lgr = nullptr;
        for (auto* l : get_plugin_logger_mgr()) {
            if (l->get_plugin_name(false) == *name) {
                lgr = l;
                break;
            }
        }

        json initial = json::array();
        if (lgr) {
            for (const auto& e : lgr->collect_logs()) {
                if (e.level >= min_lv)
                    initial.push_back({
                        { "level",     level_str(e.level) },
                        { "message",   e.message          },
                        { "timestamp", e.timestamp        }
                    });
            }
        }

        if (!lgr) {
            return response_t::ok(req.id, {
                                              { "subscription_id", nullptr },
                                              { "logs",            initial }
            });
        }

        auto cancelled = std::make_shared<std::atomic<bool>>(false);
        auto listener_id_r = std::make_shared<std::atomic<int>>(-1);

        const std::string sub_id = ctx->subscribe([lgr, listener_id_r, cancelled]()
        {
            cancelled->store(true);
            const int id = listener_id_r->load();
            if (id >= 0) lgr->remove_listener(id);
        });

        const std::string captured_sub_id = sub_id;
        const std::string captured_name = *name;
        const auto ctx_weak = std::weak_ptr<client_context>(ctx);

        const int id = lgr->add_listener([ctx_weak, cancelled, captured_sub_id, captured_name, min_lv](const logger_base::log_entry& entry)
        {
            if (cancelled->load()) return;
            if (entry.level < min_lv) return;

            auto ctx_s = ctx_weak.lock();
            if (!ctx_s) return;

            ctx_s->push({
                { "type",            "event"         },
                { "subscription_id", captured_sub_id },
                { "plugin",          captured_name   },
                { "data",
                 {
                      { "level", level_str(entry.level) },
                      { "message", entry.message },
                      { "timestamp", entry.timestamp },
                  }                                  },
            });
        });

        listener_id_r->store(id);

        return response_t::ok(req.id, {
                                          { "subscription_id", sub_id  },
                                          { "logs",            initial }
        });
    });

    // plugin.crash — subscribe to plugin crash events.
    //
    // initial response:
    //   { "subscription_id": "<id>" }
    //
    // subsequent push events (server → client, unsolicited):
    //   { "type": "event", "subscription_id": "<id>",
    //     "data": { "plugin": "<name>", "exit_code": <n>, "crash_dump_dir": "<path>" } }
    router.register_handler("plugin.crash", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.crash requires a live connection");

        auto& bus = crash_event_bus::instance();
        auto cancelled = std::make_shared<std::atomic<bool>>(false);
        auto listener_id_r = std::make_shared<std::atomic<int>>(-1);

        const std::string sub_id = ctx->subscribe([&bus, listener_id_r, cancelled]()
        {
            cancelled->store(true);
            const int id = listener_id_r->load();
            if (id >= 0) bus.remove_listener(id);
        });

        const std::string captured_sub_id = sub_id;
        const auto ctx_weak = std::weak_ptr<client_context>(ctx);

        const int id = bus.add_listener([ctx_weak, cancelled, captured_sub_id](const crash_event& ev)
        {
            if (cancelled->load()) return;
            auto ctx_s = ctx_weak.lock();
            if (!ctx_s) return;

            ctx_s->push({
                { "type",            "event"         },
                { "subscription_id", captured_sub_id },
                { "data",
                 {
                      { "plugin", ev.plugin_name },
                      { "exit_code", ev.exit_code },
                      { "crash_dump_dir", ev.crash_dump_dir },
                  }                                  },
            });
        });

        listener_id_r->store(id);

        return response_t::ok(req.id, {
                                          { "subscription_id", sub_id }
        });
    });

    router.register_handler("plugin.crash.unsubscribe", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.crash.unsubscribe requires a live connection");

        auto sub_id = require_string(req, "subscription_id");
        if (!sub_id) return response_t::err(req.id, "missing required param: subscription_id");

        const bool ok = ctx->unsubscribe(*sub_id);
        return ok ? response_t::ok(req.id,
                                   {
                                       { "subscription_id", *sub_id }
        })
                  : response_t::err(req.id, "unknown subscription_id: " + *sub_id);
    });

    router.register_handler("plugin.logs.unsubscribe", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.logs.unsubscribe requires a live connection");

        auto sub_id = require_string(req, "subscription_id");
        if (!sub_id) return response_t::err(req.id, "missing required param: subscription_id");

        const bool ok = ctx->unsubscribe(*sub_id);
        return ok ? response_t::ok(req.id,
                                   {
                                       { "subscription_id", *sub_id }
        })
                  : response_t::err(req.id, "unknown subscription_id: " + *sub_id);
    });

    router.register_handler("millennium.version", [](const request_t& req, const std::shared_ptr<client_context>&)
    {
        return response_t::ok(req.id, {
                                          { "version",    MILLENNIUM_VERSION },
                                          { "commit",     GIT_COMMIT_HASH    },
#ifdef MILLENNIUM_DEBUG
                                          { "build_type", "Debug"            },
#else
                                          { "build_type", "Release" },
#endif
        });
    });

    router.register_handler("millennium.status", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto pm = loader->get_plugin_manager();
        auto bm = loader->get_backend_manager();

        const auto all = pm->get_all_plugins();
        int total = 0, enabled_count = 0, running_count = 0;

        for (const auto& p : all) {
            if (p.is_internal) continue;
            ++total;
            if (pm->is_enabled(p.plugin_name)) ++enabled_count;
            if (bm->is_any_backend_running(p.plugin_name)) ++running_count;
        }

        return response_t::ok(req.id, {
                                          { "plugin_count",  total         },
                                          { "enabled_count", enabled_count },
                                          { "running_count", running_count },
        });
    });

    router.register_handler("plugin.memory", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        auto bm = loader->get_backend_manager();

        if (name) {
            auto pm = loader->get_plugin_manager();
            const auto plugins = pm->get_all_plugins();
            const auto* p = find_plugin(plugins, *name);
            if (!p) return response_t::err(req.id, "plugin not found: " + *name);

            auto metrics = bm->get_plugin_metrics(*name);
            return response_t::ok(req.id, {
                                              { "name",        *name               },
                                              { "rss_bytes",   metrics.rss_bytes   },
                                              { "heap_bytes",  metrics.heap_bytes  },
                                              { "cpu_percent", metrics.cpu_percent },
            });
        }

        auto all_metrics = bm->get_all_plugin_metrics();
        auto pm = loader->get_plugin_manager();
        json list = json::array();

        for (const auto& p : pm->get_all_plugins()) {
            if (p.is_internal) continue;
            size_t rss = 0;
            size_t heap = 0;
            double cpu = 0.0;
            auto it = all_metrics.find(p.plugin_name);
            if (it != all_metrics.end()) {
                rss = it->second.rss_bytes;
                heap = it->second.heap_bytes;
                cpu = it->second.cpu_percent;
            }

            list.push_back({
                { "name",        p.plugin_name },
                { "rss_bytes",   rss           },
                { "heap_bytes",  heap          },
                { "cpu_percent", cpu           },
            });
        }

        return response_t::ok(req.id, list);
    });

    router.register_handler("plugin.ffi", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.ffi requires a live connection");

        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        auto& recorder = ffi_recorder::instance();

        json initial = json::array();
        for (const auto& e : recorder.get_recent(*name, 200)) {
            initial.push_back({
                { "direction",   e.direction                                                                                   },
                { "method",      e.method                                                                                      },
                { "args",        e.args                                                                                        },
                { "result",      e.result                                                                                      },
                { "duration_ms", e.duration_ms                                                                                 },
                { "timestamp",   std::chrono::duration_cast<std::chrono::milliseconds>(e.timestamp.time_since_epoch()).count() },
                { "caller",      e.caller                                                                                      },
            });
        }

        auto cancelled = std::make_shared<std::atomic<bool>>(false);
        auto listener_id_r = std::make_shared<std::atomic<int>>(-1);

        const std::string sub_id = ctx->subscribe([&recorder, listener_id_r, cancelled]()
        {
            cancelled->store(true);
            const int id = listener_id_r->load();
            if (id >= 0) recorder.remove_listener(id);
        });

        const std::string captured_sub_id = sub_id;
        const std::string captured_name = *name;
        const auto ctx_weak = std::weak_ptr<client_context>(ctx);

        const int id = recorder.add_listener(*name, [ctx_weak, cancelled, captured_sub_id, captured_name](const ffi_call_entry& entry)
        {
            if (cancelled->load()) return;

            auto ctx_s = ctx_weak.lock();
            if (!ctx_s) return;

            ctx_s->push({
                { "type",            "event"         },
                { "subscription_id", captured_sub_id },
                { "plugin",          captured_name   },
                { "data",
                 {
                      { "direction", entry.direction },
                      { "method", entry.method },
                      { "args", entry.args },
                      { "result", entry.result },
                      { "duration_ms", entry.duration_ms },
                      { "timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(entry.timestamp.time_since_epoch()).count() },
                      { "caller", entry.caller },
                  }                                  },
            });
        });

        listener_id_r->store(id);

        return response_t::ok(req.id, {
                                          { "subscription_id", sub_id  },
                                          { "calls",           initial },
        });
    });

    router.register_handler("plugin.ffi.unsubscribe", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.ffi.unsubscribe requires a live connection");

        auto sub_id = require_string(req, "subscription_id");
        if (!sub_id) return response_t::err(req.id, "missing required param: subscription_id");

        const bool ok = ctx->unsubscribe(*sub_id);
        return ok ? response_t::ok(req.id,
                                   {
                                       { "subscription_id", *sub_id }
        })
                  : response_t::err(req.id, "unknown subscription_id: " + *sub_id);
    });

    // plugin.console — subscribe to a plugin's console (CDP) stream.
    //
    // initial response:
    //   { "subscription_id": "<id>", "messages": [ { ... }, ... ] }
    //
    // subsequent push events:
    //   { "type": "event", "subscription_id": "<id>",
    //     "plugin": "<name>", "data": <full CDP consoleAPICalled params> }
    //
    // params: { name }
    router.register_handler("plugin.console", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.console requires a live connection");

        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        auto& capture = console_capture::instance();

        json initial = json::array();
        for (const auto& e : capture.get_recent(*name, 200)) {
            initial.push_back(e.raw);
        }

        auto cancelled = std::make_shared<std::atomic<bool>>(false);
        auto listener_id_r = std::make_shared<std::atomic<int>>(-1);

        const std::string sub_id = ctx->subscribe([&capture, listener_id_r, cancelled]()
        {
            cancelled->store(true);
            const int id = listener_id_r->load();
            if (id >= 0) capture.remove_listener(id);
        });

        const std::string captured_sub_id = sub_id;
        const std::string captured_name = *name;
        const auto ctx_weak = std::weak_ptr<client_context>(ctx);

        const int id = capture.add_listener(*name, [ctx_weak, cancelled, captured_sub_id, captured_name](const console_entry& entry)
        {
            if (cancelled->load()) return;

            auto ctx_s = ctx_weak.lock();
            if (!ctx_s) return;

            ctx_s->push({
                { "type",            "event"         },
                { "subscription_id", captured_sub_id },
                { "plugin",          captured_name   },
                { "data",            entry.raw       },
            });
        });

        listener_id_r->store(id);

        return response_t::ok(req.id, {
                                          { "subscription_id", sub_id  },
                                          { "messages",        initial },
        });
    });

    router.register_handler("plugin.console.unsubscribe", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.console.unsubscribe requires a live connection");

        auto sub_id = require_string(req, "subscription_id");
        if (!sub_id) return response_t::err(req.id, "missing required param: subscription_id");

        const bool ok = ctx->unsubscribe(*sub_id);
        return ok ? response_t::ok(req.id,
                                   {
                                       { "subscription_id", *sub_id }
        })
                  : response_t::err(req.id, "unknown subscription_id: " + *sub_id);
    });

    router.register_handler("cdp.getProperties", [](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto object_id = require_string(req, "objectId");
        if (!object_id) return response_t::err(req.id, "missing required param: objectId");

        auto session_id = require_string(req, "sessionId");
        if (!session_id) return response_t::err(req.id, "missing required param: sessionId");

        auto result = console_capture::instance().get_properties(*object_id, *session_id);
        if (result.contains("error") && !result.contains("result")) {
            return response_t::err(req.id, result["error"].get<std::string>());
        }

        return response_t::ok(req.id, result);
    });

    auto make_mep_targets = [loader]() -> plugin_config::notify_targets {
        auto ipc = loader->get_ipc_main();
        return {
            ipc ? plugin_config::eval_js_fn([ipc](const std::string& s) { ipc->evaluate_javascript_expression(s); })
                : plugin_config::eval_js_fn{},
            loader->get_backend_manager()
        };
    };

    router.register_handler("plugin.config.get", [](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");
        auto key = require_string(req, "key");
        if (!key) return response_t::err(req.id, "missing required param: key");

        auto r = plugin_config::get(*name, *key);
        return r.success ? response_t::ok(req.id, { { "value", r.value } })
                         : response_t::err(req.id, r.value.get<std::string>());
    });

    router.register_handler("plugin.config.set", [make_mep_targets](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");
        auto key = require_string(req, "key");
        if (!key) return response_t::err(req.id, "missing required param: key");

        json value = (!req.params.is_null() && req.params.contains("value")) ? req.params["value"] : json(nullptr);
        auto r = plugin_config::set(make_mep_targets(), plugin_config::origin::external,
                                    *name, *key, value);
        return r.success ? response_t::ok(req.id, r.value) : response_t::err(req.id, r.value.get<std::string>());
    });

    router.register_handler("plugin.config.delete", [make_mep_targets](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");
        auto key = require_string(req, "key");
        if (!key) return response_t::err(req.id, "missing required param: key");

        auto r = plugin_config::del(make_mep_targets(), plugin_config::origin::external,
                                    *name, *key);
        return r.success ? response_t::ok(req.id, r.value) : response_t::err(req.id, r.value.get<std::string>());
    });

    router.register_handler("plugin.config.get_all", [](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        auto r = plugin_config::get_all(*name);
        return response_t::ok(req.id, { { "config", r.value } });
    });

    router.register_handler("file.list", [](const request_t& req, const std::shared_ptr<client_context>&)
    {
        json list = json::array();

        if (g_lb_patch_arena) {
            using namespace platform::shared_memory;
            const auto& map = g_lb_patch_arena->map;

            const uint32_t* keys = SHM_PTR(g_lb_patch_arena, map.keys_off, uint32_t);
            const lb_patch_list_shm_t* values = SHM_PTR(g_lb_patch_arena, map.values_off, lb_patch_list_shm_t);

            for (uint32_t i = 0; i < map.count; ++i) {
                const char* plugin_name = SHM_PTR(g_lb_patch_arena, keys[i], char);
                const auto& plist = values[i];
                const lb_patch_shm_t* patches = SHM_PTR(g_lb_patch_arena, plist.patches_off, lb_patch_shm_t);

                for (uint32_t j = 0; j < plist.patch_count; ++j) {
                    const char* file_pattern = SHM_PTR(g_lb_patch_arena, patches[j].file_off, char);
                    list.push_back({
                        { "plugin",  std::string(plugin_name)  },
                        { "pattern", std::string(file_pattern) },
                    });
                }
            }
        }

        return response_t::ok(req.id, list);
    });

    router.register_handler("file.content", [](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto path = require_string(req, "path");
        if (!path) return response_t::err(req.id, "missing required param: path");

        std::ifstream file(*path, std::ios::binary);
        if (!file.is_open()) return response_t::err(req.id, "could not open file: " + *path);

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return response_t::ok(req.id, {
                                          { "content", std::move(content) }
        });
    });
}
} // namespace mep
