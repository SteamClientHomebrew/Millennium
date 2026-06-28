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

#include "mep/mep_hooks.h"
#include "mep/mep_message.h"
#include "mep/ffi_recorder.h"
#include "mep/console_capture.h"
#include "mep/exception_capture.h"
#include "mep/crash_event_bus.h"
#include "mep/sdk_ready_bus.h"

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

struct stream_sub_state
{
    std::atomic<bool> cancelled{ false };
    std::atomic<int> log_listener_id{ -1 };
    std::atomic<int> console_listener_id{ -1 };
};

json normalize_log_entry(const logger_base::log_entry& e)
{
    json obj = {
        { "source",       "backend"          },
        { "level",        level_str(e.level) },
        { "message",      e.message          },
        { "timestamp_us", e.timestamp_us     },
    };
    if (!e.file.empty()) {
        obj["file"] = e.file;
        obj["line"] = e.line;
    }
    return obj;
}

json normalize_console_entry(const console_entry& e)
{
    const json& r = e.raw;
    uint64_t ts = r.contains("millennium_ts") ? r["millennium_ts"].get<uint64_t>() : uint64_t(0);

    std::string level = "log";
    if (r.contains("level") && r["level"].is_string())
        level = r["level"].get<std::string>();
    else if (r.contains("type") && r["type"].is_string())
        level = r["type"].get<std::string>();

    std::string message;
    if (r.contains("message") && r["message"].is_string()) message = r["message"].get<std::string>();

    std::string src_file;
    int src_line = 0;
    if (r.contains("file") && r["file"].is_string()) {
        src_file = r["file"].get<std::string>();
        if (r.contains("line") && r["line"].is_number()) src_line = r["line"].get<int>();
    } else if (r.contains("stackTrace") && r["stackTrace"].contains("callFrames")) {
        for (const auto& frame : r["stackTrace"]["callFrames"]) {
            if (!frame.contains("url") || !frame["url"].is_string()) continue;
            const std::string url = frame["url"].get<std::string>();
            if (url.empty()) continue;
            if (url.rfind("chrome://", 0) == 0 || url.rfind("chrome-extension://", 0) == 0) continue;

            std::string path = url;
            for (const auto& prefix : { "file://", "https://", "http://" }) {
                if (path.rfind(prefix, 0) == 0) {
                    path = path.substr(strlen(prefix));
                    break;
                }
            }
            const size_t slash = path.rfind('/');
            src_file = (slash != std::string::npos) ? path.substr(slash + 1) : path;
            if (frame.contains("lineNumber") && frame["lineNumber"].is_number()) src_line = frame["lineNumber"].get<int>() + 1;
            break;
        }
    }

    std::string source = "frontend";
    if (r.contains("millennium_source") && r["millennium_source"].is_string()) source = r["millennium_source"].get<std::string>();

    json obj = {
        { "source",       source  },
        { "level",        level   },
        { "message",      message },
        { "timestamp_us", ts      },
    };
    if (!src_file.empty()) {
        obj["file"] = src_file;
        obj["line"] = src_line;
    }
    return obj;
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

        const json params = {
            { "name",    *name },
            { "enabled", true  }
        };
        return response_t::ok(req.id, params);
    });

    router.register_handler("plugin.disable", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        loader->set_plugin_enable(*name, false);
        const json params = {
            { "name",    *name },
            { "enabled", false }
        };
        return response_t::ok(req.id, params);
    });

    router.register_handler("plugin.start", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        auto bm = loader->get_backend_manager();
        if (bm->is_any_backend_running(*name)) return response_t::err(req.id, "plugin backend is already running: " + *name);

        loader->set_plugin_enable(*name, true);
        const json params = {
            { "name",    *name                             },
            { "running", bm->is_any_backend_running(*name) },
        };
        return response_t::ok(req.id, params);
    });

    router.register_handler("plugin.stop", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        loader->get_backend_manager()->destroy_plugin(*name);

        const json params = {
            { "name",    *name },
            { "running", false }
        };
        return response_t::ok(req.id, params);
    });

    router.register_handler("plugin.restart", [loader](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        const bool reload_ui = !req.params.is_null() && req.params.contains("reload_ui") && req.params["reload_ui"].is_boolean() && req.params["reload_ui"].get<bool>();

        auto bm = loader->get_backend_manager();
        bm->destroy_plugin(*name);
        loader->set_plugin_enable(*name, true, reload_ui);

        const json params = {
            { "name",      *name                             },
            { "running",   bm->is_any_backend_running(*name) },
            { "reload_ui", reload_ui                         },
        };
        return response_t::ok(req.id, params);
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
                        { "level",     level_str(e.level)                   },
                        { "message",   e.message                            },
                        { "timestamp", format_log_timestamp(e.timestamp_us) }
                    });
            }
        }

        if (!lgr) {
            const json params = {
                { "subscription_id", nullptr },
                { "logs",            initial }
            };
            return response_t::ok(req.id, params);
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
                      { "timestamp", format_log_timestamp(entry.timestamp_us) },
                  }                                  },
            });
        });

        listener_id_r->store(id);

        const json params = {
            { "subscription_id", sub_id  },
            { "logs",            initial }
        };
        return response_t::ok(req.id, params);
    });

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

        const json params = {
            { "subscription_id", sub_id }
        };
        return response_t::ok(req.id, params);
    });

    router.register_handler("plugin.crash.unsubscribe", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.crash.unsubscribe requires a live connection");

        auto sub_id = require_string(req, "subscription_id");
        if (!sub_id) return response_t::err(req.id, "missing required param: subscription_id");

        const bool ok = ctx->unsubscribe(*sub_id);

        const json params = {
            { "subscription_id", *sub_id }
        };
        return ok ? response_t::ok(req.id, params) : response_t::err(req.id, "unknown subscription_id: " + *sub_id);
    });

    router.register_handler("plugin.logs.unsubscribe", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.logs.unsubscribe requires a live connection");

        auto sub_id = require_string(req, "subscription_id");
        if (!sub_id) return response_t::err(req.id, "missing required param: subscription_id");

        const bool ok = ctx->unsubscribe(*sub_id);

        const json params = {
            { "subscription_id", *sub_id }
        };
        return ok ? response_t::ok(req.id, params) : response_t::err(req.id, "unknown subscription_id: " + *sub_id);
    });

    router.register_handler("millennium.version", [](const request_t& req, const std::shared_ptr<client_context>&)
    {
        const json params = {
            { "version",    MILLENNIUM_VERSION },
            { "commit",     GIT_COMMIT_HASH    },
#ifdef MILLENNIUM_DEBUG
            { "build_type", "Debug"            },
#else
            { "build_type", "Release" },
#endif
        };
        return response_t::ok(req.id, params);
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

        const json params = {
            { "plugin_count",  total         },
            { "enabled_count", enabled_count },
            { "running_count", running_count },
        };
        return response_t::ok(req.id, params);
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
            const json params = {
                { "name",        *name               },
                { "rss_bytes",   metrics.rss_bytes   },
                { "heap_bytes",  metrics.heap_bytes  },
                { "cpu_percent", metrics.cpu_percent },
            };
            return response_t::ok(req.id, params);
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
        const json params = {
            { "subscription_id", sub_id  },
            { "calls",           initial },
        };
        return response_t::ok(req.id, params);
    });

    router.register_handler("plugin.ffi.unsubscribe", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.ffi.unsubscribe requires a live connection");

        auto sub_id = require_string(req, "subscription_id");
        if (!sub_id) return response_t::err(req.id, "missing required param: subscription_id");

        const bool ok = ctx->unsubscribe(*sub_id);

        const json params = {
            { "subscription_id", *sub_id }
        };
        return ok ? response_t::ok(req.id, params) : response_t::err(req.id, "unknown subscription_id: " + *sub_id);
    });

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

        const json params = {
            { "subscription_id", sub_id  },
            { "messages",        initial },
        };
        return response_t::ok(req.id, params);
    });

    router.register_handler("plugin.console.unsubscribe", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.console.unsubscribe requires a live connection");

        auto sub_id = require_string(req, "subscription_id");
        if (!sub_id) return response_t::err(req.id, "missing required param: subscription_id");

        const bool ok = ctx->unsubscribe(*sub_id);

        const json params = {
            { "subscription_id", *sub_id }
        };
        return ok ? response_t::ok(req.id, params) : response_t::err(req.id, "unknown subscription_id: " + *sub_id);
    });

    router.register_handler("plugin.stream", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.stream requires a live connection");

        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        plugin_logger* backend_logger = nullptr;
        for (auto* l : get_plugin_logger_mgr()) {
            if (l->get_plugin_name(false) == *name) {
                backend_logger = l;
                break;
            }
        }

        /* make a new logger for the plugin so we can immediately connect to it */
        if (!backend_logger) {
            backend_logger = new plugin_logger(name.value());
            get_plugin_logger_mgr().push_back(backend_logger);
        }

        auto& capture = console_capture::instance();

        std::vector<json> raw_entries;
        for (const auto& e : backend_logger->collect_logs()) {
            raw_entries.push_back(normalize_log_entry(e));
        }
        for (const auto& e : capture.get_recent(*name, 200)) {
            raw_entries.push_back(normalize_console_entry(e));
        }

        std::sort(raw_entries.begin(), raw_entries.end(), [](const json& a, const json& b)
        {
            return a.value("timestamp_us", uint64_t(0)) < b.value("timestamp_us", uint64_t(0));
        });

        json snapshot = json::array();
        for (auto& e : raw_entries) {
            snapshot.push_back(std::move(e));
        }

        auto state = std::make_shared<stream_sub_state>();

        const auto ctx_weak = std::weak_ptr<client_context>(ctx);
        const std::string captured_name = *name;

        /* cancel hook. called to remove both listeners when the connection dies */
        const std::string sub_id = ctx->subscribe([backend_logger, &capture, state]()
        {
            state->cancelled.store(true);
            const int log_id = state->log_listener_id.load();
            if (backend_logger && log_id >= 0) backend_logger->remove_listener(log_id);
            const int con_id = state->console_listener_id.load();
            if (con_id >= 0) capture.remove_listener(con_id);
        });

        const std::string captured_sub_id = sub_id;

        const int log_id = backend_logger->add_listener([ctx_weak, state, captured_sub_id, captured_name](const logger_base::log_entry& entry)
        {
            if (state->cancelled.load()) return;
            auto ctx_s = ctx_weak.lock();
            if (!ctx_s) return;
            ctx_s->push({
                { "type",            "event"                    },
                { "subscription_id", captured_sub_id            },
                { "plugin",          captured_name              },
                { "data",            normalize_log_entry(entry) },
            });
        });
        state->log_listener_id.store(log_id);

        const int con_id = capture.add_listener(*name, [ctx_weak, state, captured_sub_id, captured_name](const console_entry& entry)
        {
            if (state->cancelled.load()) return;
            auto ctx_s = ctx_weak.lock();
            if (!ctx_s) return;
            ctx_s->push({
                { "type",            "event"                        },
                { "subscription_id", captured_sub_id                },
                { "plugin",          captured_name                  },
                { "data",            normalize_console_entry(entry) },
            });
        });
        state->console_listener_id.store(con_id);

        const json params = {
            { "subscription_id", sub_id   },
            { "entries",         snapshot },
        };
        return response_t::ok(req.id, params);
    });

    router.register_handler("plugin.stream.unsubscribe", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "plugin.stream.unsubscribe requires a live connection");

        auto sub_id = require_string(req, "subscription_id");
        if (!sub_id) return response_t::err(req.id, "missing required param: subscription_id");

        const bool ok = ctx->unsubscribe(*sub_id);

        const json params = {
            { "subscription_id", *sub_id }
        };
        return ok ? response_t::ok(req.id, params) : response_t::err(req.id, "unknown subscription_id: " + *sub_id);
    });

    router.register_handler("runtime.exceptions", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "runtime.exceptions requires a live connection");

        const std::string plugin =
            (!req.params.is_null() && req.params.contains("plugin") && req.params["plugin"].is_string()) ? req.params["plugin"].get<std::string>() : std::string{};

        auto& capture = exception_capture::instance();

        json initial = json::array();
        for (const auto& e : capture.get_recent(plugin, 200)) {
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
        const std::string captured_plugin = plugin;
        const auto ctx_weak = std::weak_ptr<client_context>(ctx);

        const int id = capture.add_listener(plugin, [ctx_weak, cancelled, captured_sub_id, captured_plugin](const exception_entry& entry)
        {
            if (cancelled->load()) return;

            auto ctx_s = ctx_weak.lock();
            if (!ctx_s) return;

            ctx_s->push({
                { "type",            "event"         },
                { "subscription_id", captured_sub_id },
                { "plugin",          entry.plugin    },
                { "data",            entry.raw       },
            });
        });

        listener_id_r->store(id);

        const json params = {
            { "subscription_id", sub_id  },
            { "exceptions",      initial },
        };
        return response_t::ok(req.id, params);
    });

    router.register_handler("sdk.ready", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "sdk.ready requires a live connection");

        auto& bus = sdk_ready_bus::instance();
        const auto last = bus.get_last();

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

        const int id = bus.add_listener([ctx_weak, cancelled, captured_sub_id](const sdk_ready_event& ev)
        {
            if (cancelled->load()) return;
            auto ctx_s = ctx_weak.lock();
            if (!ctx_s) return;

            ctx_s->push({
                { "type",            "event"                                                                                },
                { "subscription_id", captured_sub_id                                                                        },
                { "data",            { { "millennium_version", ev.millennium_version }, { "sdk_version", ev.sdk_version } } },
            });
        });

        listener_id_r->store(id);

        const json params = {
            { "subscription_id", sub_id                                                                                                                  },
            { "ready",           last ? json{ { "millennium_version", last->millennium_version }, { "sdk_version", last->sdk_version } } : json(nullptr) },
        };
        return response_t::ok(req.id, params);
    });

    router.register_handler("sdk.ready.unsubscribe", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "sdk.ready.unsubscribe requires a live connection");

        auto sub_id = require_string(req, "subscription_id");
        if (!sub_id) return response_t::err(req.id, "missing required param: subscription_id");

        const bool ok = ctx->unsubscribe(*sub_id);
        const json params = {
            { "subscription_id", *sub_id }
        };
        return ok ? response_t::ok(req.id, params) : response_t::err(req.id, "unknown subscription_id: " + *sub_id);
    });

    router.register_handler("runtime.exceptions.unsubscribe", [](const request_t& req, const std::shared_ptr<client_context>& ctx)
    {
        if (!ctx) return response_t::err(req.id, "runtime.exceptions.unsubscribe requires a live connection");

        auto sub_id = require_string(req, "subscription_id");
        if (!sub_id) return response_t::err(req.id, "missing required param: subscription_id");

        const bool ok = ctx->unsubscribe(*sub_id);
        const json params = {
            { "subscription_id", *sub_id }
        };
        return ok ? response_t::ok(req.id, params) : response_t::err(req.id, "unknown subscription_id: " + *sub_id);
    });

    router.register_handler("millennium.api_health", [](const request_t& req, const std::shared_ptr<client_context>&)
    {
        const auto last = sdk_ready_bus::instance().get_last();
        if (!last) {
            return response_t::err(req.id, "sdk.ready has not fired yet — call after subscribing to sdk.ready");
        }

        json missing_arr = json::array();
        for (const auto& k : last->api_missing)
            missing_arr.push_back(k);

        const json params = {
            { "healthy", last->api_missing.empty() },
            { "missing", std::move(missing_arr)    },
            { "total",   last->api_total           },
        };
        return response_t::ok(req.id, params);
    });

    auto make_mep_targets = [loader]() -> plugin_config::notify_targets
    {
        auto ipc = loader->get_ipc_main();
        plugin_config::notify_targets res = { plugin_config::eval_js_fn{}, loader->get_backend_manager() };

        if (!ipc) return res;

        res.eval_js = plugin_config::eval_js_fn([ipc](const std::string& s)
        {
            ipc->evaluate_javascript_expression(s);
        });
        return res;
    };

    router.register_handler("plugin.config.get", [](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");
        auto key = require_string(req, "key");
        if (!key) return response_t::err(req.id, "missing required param: key");

        auto r = plugin_config::get(*name, *key);

        const json params = {
            { "value", r.value }
        };
        return r.success ? response_t::ok(req.id, params) : response_t::err(req.id, r.value.get<std::string>());
    });

    router.register_handler("plugin.config.set", [make_mep_targets](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");
        auto key = require_string(req, "key");
        if (!key) return response_t::err(req.id, "missing required param: key");

        json value = (!req.params.is_null() && req.params.contains("value")) ? req.params["value"] : json(nullptr);
        auto r = plugin_config::set(make_mep_targets(), plugin_config::origin::external, *name, *key, value);
        return r.success ? response_t::ok(req.id, r.value) : response_t::err(req.id, r.value.get<std::string>());
    });

    router.register_handler("plugin.config.delete", [make_mep_targets](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");
        auto key = require_string(req, "key");
        if (!key) return response_t::err(req.id, "missing required param: key");

        auto r = plugin_config::del(make_mep_targets(), plugin_config::origin::external, *name, *key);
        return r.success ? response_t::ok(req.id, r.value) : response_t::err(req.id, r.value.get<std::string>());
    });

    router.register_handler("plugin.config.get_all", [](const request_t& req, const std::shared_ptr<client_context>&)
    {
        auto name = require_string(req, "name");
        if (!name) return response_t::err(req.id, "missing required param: name");

        auto r = plugin_config::get_all(*name);

        const json params = {
            { "config", r.value }
        };
        return response_t::ok(req.id, params);
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

        const json params = {
            { "content", std::move(content) }
        };
        return response_t::ok(req.id, params);
    });
}
} // namespace mep
