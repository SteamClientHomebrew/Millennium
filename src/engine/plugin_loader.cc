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

#include "millennium/millennium_lifecycle.h"
#include "millennium/core_ipc.h"
#include "millennium/plugin_config.h"
#include "millennium/life_cycle.h"
#include "millennium/millennium_updater.h"
#include "millennium/filesystem.h"
#include "millennium/plugin_manager.h"
#include "millennium/steam_hooks.h"
#include "millennium/types.h"
#include "millennium/plugin_loader.h"
#include "millennium/plugin_ipc.h"
#include "millennium/url_parser.h"
#include "millennium/backend_init.h"
#include "millennium/backend_mgr.h"
#include "millennium/environment.h"
#include "millennium/http_hooks.h"
#include "millennium/star_parser.h"
#include "millennium/logger.h"
#include "millennium/plugin_webkit_store.h"
#include "millennium/semver.h"
#include "millennium/auth.h"

#include "instrumentation/patch_registry.h"

#include "mep/console_capture.h"
#include "mep/patch_update_notifier.h"
#include "mep/exception_capture.h"
#include "mep/crash_event_bus.h"

#include "head/entry_point.h"

#include <curl/curl.h>
#include <format>

namespace
{
template <typename Range> std::string join_strings(const Range& items, std::string_view sep)
{
    std::string out;
    bool first = true;
    for (const auto& item : items) {
        if (!first) out += sep;
        out += item;
        first = false;
    }
    return out;
}
} // namespace

#ifdef __linux__
#include <mutex>
#include <condition_variable>
extern std::mutex g_cdp_pipe_mutex;
extern std::condition_variable g_cdp_pipe_cv;
extern int g_cdp_pipe_generation;
#endif

using namespace std::placeholders;
using namespace std::chrono;

plugin_loader::plugin_loader(std::shared_ptr<plugin_manager> plugin_manager, std::shared_ptr<millennium_updater> millennium_updater)
    : m_thread_pool(std::make_unique<thread_pool>(2)), m_plugin_manager(std::move(plugin_manager)), m_plugin_ptr(nullptr), m_enabledPluginsPtr(nullptr),
      m_millennium_updater(std::move(millennium_updater))
{
    logger.log("Initializing plugin_loader...");
    this->init();
}

plugin_loader::~plugin_loader() = default;

void plugin_loader::shutdown()
{
    if (m_crash_listener_id >= 0) {
        mep::crash_event_bus::instance().remove_listener(m_crash_listener_id);
        m_crash_listener_id = -1;
    }
    if (m_backend_manager) {
        m_backend_manager->shutdown();
    }
    logger.log("Successfully shut down plugin_loader...");
}

static std::string make_crash_js(const mep::crash_event& ev)
{
    const json detail = {
        { "plugin",       ev.plugin_name                                             },
        { "displayName",  ev.display_name.empty() ? ev.plugin_name : ev.display_name },
        { "exitCode",     (uint32_t)ev.exit_code                                     },
        { "crashDumpDir", ev.crash_dump_dir                                          }
    };
    return std::format("window.dispatchEvent(new CustomEvent('millennium-plugin-crash',{{detail:{}}}));", detail.dump());
}

void plugin_loader::devtools_connection_hdlr(std::shared_ptr<cdp_client> cdp)
{
    m_cdp = cdp;
    m_socket_con_time = std::chrono::system_clock::now();

    m_network_hook_ctl->set_cdp_client(m_cdp);
    m_network_hook_ctl->init();
    m_ipc_main = std::make_shared<ipc_main>(m_millennium_backend, m_plugin_manager, m_cdp, m_backend_manager);

    m_millennium_updater->set_ipc_main(m_ipc_main);
    m_millennium_backend->set_ipc_main(m_ipc_main);

    mep::console_capture::instance().start(m_cdp, m_plugin_manager);
    mep::exception_capture::instance().start(m_cdp, m_plugin_manager);

    m_thread_pool->enqueue([this]()
    {
        logger.log("Starting webkit world manager...");

        this->m_ffi_binder = std::make_unique<ffi_binder>(m_cdp, m_plugin_manager, m_ipc_main);
        this->world_mgr = std::make_unique<webkit_world_mgr>(m_cdp, m_plugin_manager, m_network_hook_ctl, m_plugin_webkit_store);
    });

    m_thread_pool->enqueue([this]()
    {
        logger.log("Connected to Steam devtools protocol...");
        this->init_devtools();

        if (m_crash_listener_id >= 0) {
            mep::crash_event_bus::instance().remove_listener(m_crash_listener_id);
        }

        std::weak_ptr<ipc_main> weak_ipc = m_ipc_main;
        std::weak_ptr<plugin_manager> weak_pm = m_plugin_manager;
        m_crash_listener_id = mep::crash_event_bus::instance().add_listener([weak_ipc, weak_pm](mep::crash_event ev)
        {
            if (ev.display_name.empty()) {
                if (auto pm = weak_pm.lock()) {
                    for (const auto& p : pm->get_all_plugins()) {
                        if (p.plugin_name == ev.plugin_name) {
                            ev.display_name = p.plugin_json.value("common_name", ev.plugin_name);
                            break;
                        }
                    }
                }
                if (ev.display_name.empty()) ev.display_name = ev.plugin_name;
            }

            auto ipc = weak_ipc.lock();
            if (!ipc) return;
            ipc->evaluate_javascript_expression(make_crash_js(ev));
        });
    });
}

void plugin_loader::init()
{
    m_network_hook_ctl = std::make_shared<network_hook_ctl>(m_plugin_manager);

    m_millennium_backend = std::make_shared<head::millennium_backend>(m_network_hook_ctl, m_plugin_manager, m_millennium_updater);
    m_millennium_backend->init(); /** manual ctor, shared_from_this requires a ref holder before its valid */

    m_backend_event_dispatcher = std::make_shared<backend_event_dispatcher>(m_plugin_manager);
    m_backend_manager = std::make_shared<backend_manager>(m_plugin_manager, m_backend_event_dispatcher);
    m_backend_initializer = std::make_shared<backend_initializer>(m_plugin_manager, m_backend_manager, m_backend_event_dispatcher);

    m_plugin_webkit_store = std::make_shared<plugin_webkit_store>(m_plugin_manager);
    m_plugin_ptr = std::make_shared<std::vector<plugin_manager::plugin_t>>(m_plugin_manager->get_all_plugins());
    m_enabledPluginsPtr = std::make_shared<std::vector<plugin_manager::plugin_t>>(m_plugin_manager->get_enabled_backends());
    register_packed_plugin_resources(*m_plugin_ptr);

    m_plugin_manager->init();

    /** setup steam hooks once backends have loaded */
    m_backend_event_dispatcher->on_all_backends_ready(platform::wait_for_backend_load);
}
void plugin_loader::init_devtools()
{
    constexpr auto targetFrame = "SharedJSContext";
    int attempts = 0;

    while (true) {
        auto targets = m_cdp->send_host("Target.getTargets").get();

        if (targets.contains("targetInfos") && targets["targetInfos"].is_array() && !targets["targetInfos"].empty()) {
            auto it = std::find_if(targets["targetInfos"].begin(), targets["targetInfos"].end(), [&](const auto& target)
            {
                return target.value("title", "") == targetFrame;
            });

            if (it != targets["targetInfos"].end()) {
                auto targetId = it->at("targetId").get<std::string>();
                m_shared_js_target_id = targetId;

                const json attach_params = {
                    { "targetId", targetId },
                    { "flatten",  true     }
                };

                auto result = m_cdp->send_host("Target.attachToTarget", attach_params).get();
                /** set the session ID of the SharedJSContext */
                m_cdp->set_shared_js_session_id(result.at("sessionId").get<std::string>());

                break;
            }
        }

        if (attempts++ % 20 == 0) {
            logger.warn("SharedJSContext not found yet, waiting... (attempt {})", attempts);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    m_thread_pool->enqueue([this]()
    {
        logger.log("Connected to SharedJSContext in {} ms", duration_cast<milliseconds>(system_clock::now() - m_socket_con_time).count());
        const bool should_reload = !m_skip_next_inject_reload.exchange(false, std::memory_order_acq_rel);
        this->inject_frontend_shims(should_reload);
    });
}

std::shared_ptr<std::thread> plugin_loader::connect_steam_socket(std::shared_ptr<socket_utils> socketHelpers)
{
    std::shared_ptr<socket_utils::socket_t> browserProps = std::make_shared<socket_utils::socket_t>();

    browserProps->name = "CEFBrowser";
    browserProps->on_connect = [this](std::shared_ptr<cdp_client> cdp)
    {
        this->devtools_connection_hdlr(std::move(cdp));
    };
    return std::make_shared<std::thread>(std::thread([ptrSocketHelpers = socketHelpers, browserProps]
    {
        ptrSocketHelpers->connect_socket(browserProps);
    }));
}

void plugin_loader::register_packed_plugin_resources(const std::vector<plugin_manager::plugin_t>& plugins)
{
    for (const auto& plugin : plugins) {
        if (plugin.format != plugin_manager::plugin_format::star) continue;

        const std::string base = std::string(m_network_hook_ctl->get_ftp_url()) + "star/" + plugin.plugin_name + "/";

        if (!m_plugin_manager->is_enabled(plugin.plugin_name)) {
            if (plugin.has_frontend_js) m_network_hook_ctl->unregister_virtual_resource(base + "bundle.js");
            if (plugin.has_webkit_js) m_network_hook_ctl->unregister_virtual_resource(base + "webkit.js");
            continue;
        }

        if (plugin.has_frontend_js) {
            auto cache = std::make_shared<std::string>();
            auto mtx = std::make_shared<std::mutex>();
            m_network_hook_ctl->register_virtual_resource(base + "bundle.js", [path = plugin.plugin_base_dir, cache, mtx]()
            {
                std::lock_guard lock(*mtx);
                if (cache->empty()) *cache = star_read_javascript(path, star_js_section::frontend);
                return *cache;
            });
        }
        if (plugin.has_webkit_js) {
            auto cache = std::make_shared<std::string>();
            auto mtx = std::make_shared<std::mutex>();
            m_network_hook_ctl->register_virtual_resource(base + "webkit.js", [path = plugin.plugin_base_dir, cache, mtx]()
            {
                std::lock_guard lock(*mtx);
                if (cache->empty()) *cache = star_read_javascript(path, star_js_section::webkit);
                return *cache;
            });
        }
    }
}

void plugin_loader::setup_webkit_shims()
{
    logger.log("Injecting webkit shims...");
    m_plugin_webkit_store->clear();

    const auto plugins = this->m_plugin_manager->get_all_plugins();

    for (auto& plugin : plugins) {
        if (!m_plugin_manager->is_enabled(plugin.plugin_name)) continue;

        if (plugin.format == plugin_manager::plugin_format::star) {
            if (plugin.has_webkit_js) {
                const auto vpath = std::filesystem::path("/star") / plugin.plugin_name / "webkit.js";
                m_plugin_webkit_store->add({ plugin.plugin_name, vpath, plugin_manager::plugin_format::star });
            }
            continue;
        }

        const auto abs_path = std::filesystem::path(platform::environment::get("MILLENNIUM__PLUGINS_PATH")) / plugin.plugin_webkit_path;
        if (std::filesystem::exists(abs_path)) {
            m_plugin_webkit_store->add({ plugin.plugin_name, abs_path, plugin_manager::plugin_format::loose_files });
        }
    }
}

std::string plugin_loader::cdp_generate_bootstrap_module(const std::vector<std::string>& modules)
{
    const std::string preload_path = platform::get_millennium_preload_path();
    const std::string ftp_path = m_network_hook_ctl->get_ftp_url() + preload_path;
    const std::string module_list = std::format(R"("{}")", join_strings(modules, R"(", ")"));

    return std::format("import('{}').then(m => (new m.default).startClient('{}', [{}]))", ftp_path, MILLENNIUM_VERSION, module_list);
}

std::string plugin_loader::cdp_generate_shim_module()
{
    std::vector<std::string> script_list;
    std::vector<plugin_manager::plugin_t> plugins = m_plugin_manager->get_all_plugins();

    /** Add the builtin Millennium plugin first so it starts loading before all others */
    script_list.push_back(std::format("{}{}/millennium-frontend.js", m_network_hook_ctl->get_ftp_url(), GetScrambledApiPathToken()));

    for (auto& plugin : plugins) {
        if (!m_plugin_manager->is_enabled(plugin.plugin_name)) {
            continue;
        }

        if (plugin.format == plugin_manager::plugin_format::star) {
            if (plugin.has_frontend_js) script_list.push_back(std::format("{}star/{}/bundle.js", m_network_hook_ctl->get_ftp_url(), plugin.plugin_name));
            continue;
        }

        const auto frontEndAbs = plugin.plugin_frontend_dir.generic_string();
        script_list.push_back(utils::url::get_url_from_path(m_network_hook_ctl->get_ftp_url(), frontEndAbs));
    }
    return this->cdp_generate_bootstrap_module(script_list);
}

void plugin_loader::inject_frontend_shims(bool reload_frontend)
{
    this->setup_webkit_shims();
    auto self = this->shared_from_this();

    auto cdp = m_cdp;
    if (!cdp) {
        logger.warn("inject_frontend_shims: no CDP connection, skipping frontend injection");
        return;
    }

    /**
     * add the initial binding for the shared js context.
     * the rest is handled by the ffi_binder.
     */
    const auto insert_ipc = std::make_shared<std::function<void()>>([self, cdp, reload_frontend]()
    {
        cdp->send("Runtime.enable").get();

        const json add_binding_params = {
            { "name", ffi_constants::binding_name }
        };
        cdp->send("Runtime.addBinding", add_binding_params).get();

        const json add_extension_binding_params = {
            { "name", ffi_constants::extension_binding_name }
        };
        cdp->send("Runtime.addBinding", add_extension_binding_params).get();

        const json add_cdp_proxy_params = {
            { "name", ffi_constants::cdp_proxy_binding_name }
        };
        cdp->send("Runtime.addBinding", add_cdp_proxy_params).get();

        if (reload_frontend) {
            logger.log("Frontend notifier skipped isolated world injection (page reload pending)");
            return;
        }

        /** create an isolated world per enabled plugin and inject the CDP context script */
        const std::string isolated_ctx_script = get_cdp_isolated_ctx_script();
        const std::string shared_js_session = cdp->get_shared_js_session_id();

        if (!isolated_ctx_script.empty()) {
            const auto plugins = self->m_plugin_manager->get_enabled_plugins();
            for (const auto& plugin : plugins) {
                try {
                    auto frame_result = cdp->send("Page.getFrameTree").get();
                    const std::string frame_id = frame_result["frameTree"]["frame"]["id"].get<std::string>();

                    const json create_world_params = {
                        { "frameId", frame_id },
                        { "worldName", std::format("millennium-{}", plugin.plugin_name) },
                        { "grantUniversalAccess", false }
                    };
                    auto world_result = cdp->send("Page.createIsolatedWorld", create_world_params).get();
                    const int ctx_id = world_result["executionContextId"].get<int>();

                    const json eval_params = {
                        { "expression", isolated_ctx_script },
                        { "contextId",  ctx_id              }
                    };
                    cdp->send("Runtime.evaluate", eval_params).get();

                    if (self->m_ffi_binder) {
                        self->m_ffi_binder->register_isolated_ctx(plugin.plugin_name, ctx_id, shared_js_session);
                    }

                    logger.log("Created isolated CDP world for plugin '{}' (ctx {})", plugin.plugin_name, ctx_id);
                } catch (const std::exception& e) {
                    LOG_ERROR("Failed to create isolated world for plugin '{}': {}", plugin.plugin_name, e.what());
                }
            }
        }

        logger.log("Frontend notifier finished!");
    });

    /**
     * reload the frontend of Steam if need be.
     * sometimes we need to queue a change instead of instantly applying it.
     */
    const auto reload = std::make_shared<std::function<void()>>([reload_frontend, self, cdp]()
    {
        json reload_params = {
            { "ignoreCache", true }
        };

        if (reload_frontend) {
            /*
             * mark that we are about to reload so the reconnect handler that follows
             * (init_devtools) skips its own reload and does not create an infinite loop.
             */
            self->m_skip_next_inject_reload.store(true, std::memory_order_release);

            try {
                json result;
                do {
                    result = cdp->send("Page.reload", reload_params).get();
                }
                /** empty means it was successful */
                while (!result.empty());
            } catch (...) {
                /* CDP died before/during reload — clear the flag so the next genuine
                   reconnect still gets its own reload rather than silently skipping it. */
                self->m_skip_next_inject_reload.store(false, std::memory_order_release);
            }
        }
    });

    /**
     * tell chromium to run Millennium's frontend every time a new document is loaded.
     */
    const auto insert_millennium = std::make_shared<std::function<void()>>([self, cdp]()
    {
        cdp->send("Page.enable").get();

        if (!self->document_script_id.empty()) {
            const json params = {
                { "identifier", self->document_script_id }
            };

            cdp->send("Page.removeScriptToEvaluateOnNewDocument", params).get();
            self->document_script_id.clear();
        }

        json params = {
            { "source", self->cdp_generate_shim_module() }
        };

        constexpr int max_retries = 3;
        for (int attempt = 0; attempt < max_retries; ++attempt) {
            try {
                auto result = cdp->send("Page.addScriptToEvaluateOnNewDocument", params).get();

                if (!result.contains("identifier") || !result["identifier"].is_string()) {
                    LOG_ERROR("Page.addScriptToEvaluateOnNewDocument returned unexpected result: {}", result.dump());
                    continue;
                }

                self->document_script_id = result["identifier"].get<std::string>();
                return;
            } catch (const std::exception& e) {
                LOG_ERROR("Page.addScriptToEvaluateOnNewDocument failed (attempt {}/{}): {}", attempt + 1, max_retries, e.what());
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        LOG_ERROR("Failed to register SDK script after {} attempts — frontend will not load", max_retries);
    });

    /**
     * wait for millennium to finish starting the plugin backends
     * we garunetee the backends will be loaded before the plugin, so
     * we need to enforce that here.
     */
    m_backend_event_dispatcher->on_all_backends_ready([insert_millennium, reload, insert_ipc, self]()
    {
        self->m_thread_pool->enqueue([insert_millennium, reload, insert_ipc, self]()
        {
            logger.log("Notifying frontend of backend load...");
            self->m_backend_initializer->compat_restore_shared_js_context();

            (*insert_ipc)();
            (*insert_millennium)();
            (*reload)();
        });
    });
}

void plugin_loader::start_plugin_frontends()
{
    if (millennium_lifecycle::get().terminate.flag.load()) {
        logger.log("Terminating frontend thread pool...");
        return;
    }

#ifdef __linux__
    /** snapshot generation before connecting so we can detect a new pipe on reconnect. */
    int generation_before_connect;
    {
        std::lock_guard<std::mutex> lock(g_cdp_pipe_mutex);
        generation_before_connect = g_cdp_pipe_generation;
    }
#endif

    std::shared_ptr<socket_utils> helper = std::make_shared<socket_utils>();
    logger.log("Starting frontend socket...");
    std::shared_ptr<std::thread> socket_thread = this->connect_steam_socket(helper);

    if (socket_thread->joinable()) {
        socket_thread->join();
    }

    if (millennium_lifecycle::get().terminate.flag.load()) {
        logger.log("Steam is shutting down, terminating frontend thread pool...");
        return;
    }

    logger.warn("Unexpectedly Disconnected from Steam, attempting to reconnect...");

#ifdef _WIN32
    {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
            if (millennium_lifecycle::get().disconnect_frontend.load()) {
                logger.log("Steam is shutting down, terminating frontend thread pool...");
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
#elif __linux__
    {
        std::unique_lock<std::mutex> lock(g_cdp_pipe_mutex);
        g_cdp_pipe_cv.wait(lock, [&generation_before_connect]
        {
            return g_cdp_pipe_generation > generation_before_connect || millennium_lifecycle::get().terminate.flag.load();
        });
    }

    if (millennium_lifecycle::get().terminate.flag.load()) {
        logger.log("Steam is shutting down, terminating frontend thread pool...");
        return;
    }
#endif

    logger.warn("Reconnecting to Steam...");
    this->start_plugin_frontends();
}

/* debug function, just for developers */
void plugin_loader::log_enabled_plugins()
{
    std::vector<std::string> statuses;
    statuses.reserve(m_plugin_ptr->size());

    for (const auto& plugin : *m_plugin_ptr) {
        statuses.push_back(std::format("{}: {}", plugin.plugin_name, m_plugin_manager->is_enabled(plugin.plugin_name) ? "enabled" : "disabled"));
    }

    logger.log("Plugins: {{ {} }}", join_strings(statuses, ", "));
}

void plugin_loader::setup_child_request_handler()
{
    std::weak_ptr<plugin_loader> weak_self = weak_from_this();

    m_backend_manager->set_child_request_handler([weak_self](const std::string& plugin_name, const std::string& method, const nlohmann::json& params) -> nlohmann::json
    {
        auto self = weak_self.lock();
        if (!self) {
            return {
                { "error", "plugin_loader shutting down" }
            };
        }

        if (method == plugin_ipc::child_method::CALL_FRONTEND_METHOD) {
            auto ipc = self->m_ipc_main;
            if (!ipc) {
                return {
                    { "success", false                        },
                    { "error",   "frontend not connected yet" }
                };
            }

            /* child sends {methodName, params}, but process_message expects
               {pluginName, methodName, argumentList} under "data" */
            nlohmann::json data = {
                { "pluginName", plugin_name },
                { "methodName", params.value("methodName", "") },
                { "argumentList", params.contains("params") ? params["params"] : nlohmann::json::array() },
                { "caller", params.value("caller", std::string{}) },
                { "return_by_value", params.value("return_by_value", false) }
            };

            nlohmann::json call = {
                { "id",   ipc_main::ipc_method::CALL_FRONTEND_METHOD },
                { "data", data                                       }
            };
            return ipc->process_message(call);
        }

        if (method == plugin_ipc::child_method::READY) {
            auto dispatcher = self->m_backend_event_dispatcher;
            if (dispatcher) {
                dispatcher->backend_loaded_event_hdlr({ plugin_name, backend_event_dispatcher::backend_ready_event::BACKEND_LOAD_SUCCESS });
            }
            return {
                { "success", true }
            };
        }

        if (method == plugin_ipc::child_method::ADD_BROWSER_CSS || method == plugin_ipc::child_method::ADD_BROWSER_JS) {
            auto backend = self->m_millennium_backend;
            if (!backend) {
                return {
                    { "error", "backend not available" }
                };
            }
            auto webkit_mgr = backend->get_theme_webkit_mgr().lock();
            if (!webkit_mgr) {
                return {
                    { "error", "webkit_mgr not available" }
                };
            }
            const std::string content = params.value("content", "");
            const std::string pattern = params.value("pattern", ".*");
            auto type = (method == plugin_ipc::child_method::ADD_BROWSER_CSS) ? network_hook_ctl::TagTypes::STYLESHEET : network_hook_ctl::TagTypes::JAVASCRIPT;
            auto id = webkit_mgr->add_browser_hook(content, pattern, type);
            return {
                { "id", id }
            };
        }

        if (method == plugin_ipc::child_method::REMOVE_BROWSER_MODULE) {
            auto backend = self->m_millennium_backend;
            if (!backend) {
                return {
                    { "error", "backend not available" }
                };
            }
            auto webkit_mgr = backend->get_theme_webkit_mgr().lock();
            if (!webkit_mgr) {
                return {
                    { "error", "webkit_mgr not available" }
                };
            }
            const auto hook_id = params.value("hook_id", 0ULL);
            const bool ok = webkit_mgr->remove_browser_hook(hook_id);
            return {
                { "success", ok }
            };
        }

        if (method == plugin_ipc::child_method::LOG) {
            const std::string level = params.is_object() ? params.value("level", "info") : "info";
            const std::string message = params.is_object() ? params.value("message", "") : "";
            const bool v2 = params.is_object() ? params.value("v2", false) : false;
            const uint64_t timestamp_us = params.is_object() ? params.value("timestamp_us", uint64_t(0)) : 0;
            const std::string src_file = (params.is_object() && v2) ? params.value("file", "") : "";
            const int src_line = (params.is_object() && v2) ? params.value("line", 0) : 0;

            /** only v1 loggers output to Millennium */
            if (level == "error")
                ErrorToLogger(plugin_name, message, v2, timestamp_us, src_file, src_line);
            else if (level == "warn")
                WarnToLogger(plugin_name, message, v2, timestamp_us, src_file, src_line);
            else
                InfoToLogger(plugin_name, message, v2, timestamp_us, src_file, src_line);

            return {
                { "success", true }
            };
        }

        if (method == plugin_ipc::child_method::VERSION) {
            return {
                { "version", MILLENNIUM_VERSION }
            };
        }

        if (method == plugin_ipc::child_method::STEAM_PATH) {
            try {
                return {
                    { "path", platform::get_steam_path().string() }
                };
            } catch (const std::exception& e) {
                return {
                    { "path",  ""       },
                    { "error", e.what() }
                };
            }
        }

        if (method == plugin_ipc::child_method::INSTALL_PATH) {
            try {
                return {
                    { "path", platform::get_install_path().string() }
                };
            } catch (const std::exception& e) {
                return {
                    { "path",  ""       },
                    { "error", e.what() }
                };
            }
        }

        if (method == plugin_ipc::child_method::IS_PLUGIN_ENABLED) {
            const std::string name = params.value("name", "");
            auto pm = self->m_plugin_manager;
            if (!pm) {
                return {
                    { "enabled", false }
                };
            }
            try {
                return {
                    { "enabled", pm->is_enabled(name) }
                };
            } catch (const std::exception&) {
                return {
                    { "enabled", false }
                };
            }
        }

        if (method == plugin_ipc::child_method::CMP_VERSION) {
            const std::string v1 = params.value("v1", "");
            const std::string v2 = params.value("v2", "");
            const char* s1 = v1.c_str();
            const char* s2 = v2.c_str();
            if (s1[0] == 'v' || s1[0] == 'V') s1++;
            if (s2[0] == 'v' || s2[0] == 'V') s2++;
            try {
                return {
                    { "result", semver::cmp(s1, s2) }
                };
            } catch (const std::exception&) {
                return {
                    { "result", -2 }
                };
            }
        }

        if (method == plugin_ipc::child_method::HTTP_REQUEST) {
            const std::string url = params.value("url", "");
            const std::string http_method = params.value("method", "GET");
            const std::string data = params.value("data", "");
            const long timeout = params.value("timeout", 30L);
            const bool follow_redirects = params.value("follow_redirects", true);
            const bool verify_ssl = params.value("verify_ssl", true);
            const std::string user_agent = params.value("user_agent", "Millennium/1.0");
            const std::string proxy = params.value("proxy", "");

            CURL* curl = curl_easy_init();
            if (!curl) {
                return {
                    { "error", "failed to initialize curl" }
                };
            }

            static constexpr size_t MAX_RESPONSE_BODY = 64u * 1024u * 1024u; /* 64 MB */

            struct body_ctx
            {
                std::string data;
                bool exceeded;
            };
            body_ctx body{ {}, false };
            nlohmann::json response_headers = nlohmann::json::object();

            auto write_cb = +[](char* ptr, size_t size, size_t nmemb, void* ud) -> size_t
            {
                auto* ctx = static_cast<body_ctx*>(ud);
                size_t bytes = size * nmemb;
                if (ctx->data.size() + bytes > MAX_RESPONSE_BODY) {
                    ctx->exceeded = true;
                    return 0; /* abort transfer */
                }
                ctx->data.append(ptr, bytes);
                return bytes;
            };

            struct header_ctx
            {
                nlohmann::json* headers;
            };
            header_ctx hctx{ &response_headers };

            auto header_cb = +[](char* buf, size_t size, size_t nitems, void* ud) -> size_t
            {
                size_t len = size * nitems;
                auto* ctx = static_cast<header_ctx*>(ud);
                std::string line(buf, len);
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                    line.pop_back();
                auto colon = line.find(':');
                if (colon != std::string::npos && !line.empty()) {
                    std::string key = line.substr(0, colon);
                    std::string val = line.substr(colon + 1);
                    while (!val.empty() && val.front() == ' ')
                        val.erase(val.begin());
                    (*ctx->headers)[key] = val;
                }
                return len;
            };

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &hctx);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, follow_redirects ? 1L : 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, verify_ssl ? 1L : 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, verify_ssl ? 2L : 0L);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

            struct curl_slist* req_headers = nullptr;
            if (params.contains("headers") && params["headers"].is_object()) {
                for (auto& [k, v] : params["headers"].items()) {
                    req_headers = curl_slist_append(req_headers, (k + ": " + v.get<std::string>()).c_str());
                }
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, req_headers);
            }

            if (http_method == "POST") {
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                if (!data.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(data.size()));
                }
            } else if (http_method == "PUT" || http_method == "PATCH" || http_method == "DELETE" || http_method == "OPTIONS") {
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, http_method.c_str());
                if (!data.empty()) {
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(data.size()));
                }
            } else if (http_method == "HEAD") {
                curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
            }

            if (params.contains("auth") && params["auth"].is_object()) {
                std::string auth_str = params["auth"].value("user", "") + ":" + params["auth"].value("pass", "");
                curl_easy_setopt(curl, CURLOPT_USERPWD, auth_str.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
            }

            if (!proxy.empty()) {
                curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
            }

            CURLcode res = curl_easy_perform(curl);
            long status_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

            curl_slist_free_all(req_headers);
            curl_easy_cleanup(curl);

            if (body.exceeded) {
                return {
                    { "error", "response body exceeded 64 MB limit" }
                };
            }

            if (res != CURLE_OK) {
                return {
                    { "error", curl_easy_strerror(res) }
                };
            }

            return {
                { "body",    body.data        },
                { "status",  status_code      },
                { "headers", response_headers }
            };
        }

        if (method == plugin_ipc::child_method::HTTP_DOWNLOAD) {
            const std::string url = params.value("url", "");
            const std::string dest = params.value("path", "");
            const long timeout = params.value("timeout", 0L); /* 0 = no timeout for downloads */
            const bool follow_redirects = params.value("follow_redirects", true);
            const bool verify_ssl = params.value("verify_ssl", true);
            const std::string user_agent = params.value("user_agent", "Millennium/1.0");

            if (dest.empty()) {
                return {
                    { "error", "path is required" }
                };
            }

            FILE* fp = fopen(dest.c_str(), "wb");
            if (!fp) {
                return {
                    { "error", "failed to open file: " + dest }
                };
            }

            CURL* curl = curl_easy_init();
            if (!curl) {
                fclose(fp);
                return {
                    { "error", "failed to initialize curl" }
                };
            }

            struct dl_ctx
            {
                FILE* fp;
                size_t written;
            };
            dl_ctx ctx{ fp, 0 };

            auto write_cb = +[](char* ptr, size_t size, size_t nmemb, void* ud) -> size_t
            {
                auto* c = static_cast<dl_ctx*>(ud);
                size_t bytes = fwrite(ptr, size, nmemb, c->fp);
                c->written += bytes * size;
                return bytes;
            };

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, follow_redirects ? 1L : 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, verify_ssl ? 1L : 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, verify_ssl ? 2L : 0L);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            if (timeout > 0) {
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
            }

            struct curl_slist* req_headers = nullptr;
            if (params.contains("headers") && params["headers"].is_object()) {
                for (auto& [k, v] : params["headers"].items()) {
                    req_headers = curl_slist_append(req_headers, (k + ": " + v.get<std::string>()).c_str());
                }
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, req_headers);
            }

            CURLcode res = curl_easy_perform(curl);
            long status_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

            curl_slist_free_all(req_headers);
            curl_easy_cleanup(curl);
            fclose(fp);

            if (res != CURLE_OK) {
                std::filesystem::remove(dest);
                return {
                    { "error", curl_easy_strerror(res) }
                };
            }

            return {
                { "success",       true        },
                { "status",        status_code },
                { "bytes_written", ctx.written }
            };
        }

        if (method == plugin_ipc::child_method::CONFIG_GET) {
            auto r = plugin_config::get(plugin_name, params.value("key", ""));
            return r.success ? json{{ "value", r.value }} : json{{ "error", r.value }};
        }

        if (method == plugin_ipc::child_method::CONFIG_SET) {
            auto ipc = self->m_ipc_main;
            plugin_config::notify_targets targets{ ipc ? plugin_config::eval_js_fn(
                                                             [ipc](const std::string& s)
            {
                ipc->evaluate_javascript_expression(s);
            })
                                                       : plugin_config::eval_js_fn{},
                                                   {} };
            json value = params.contains("value") ? params["value"] : json(nullptr);
            auto r = plugin_config::set(targets, plugin_config::origin::lua_child, plugin_name, params.value("key", ""), value);
            return r.success ? json{{ "success", true }} : json{{ "error", r.value }};
        }

        if (method == plugin_ipc::child_method::CONFIG_DELETE) {
            auto ipc = self->m_ipc_main;
            plugin_config::notify_targets targets{ ipc ? plugin_config::eval_js_fn(
                                                             [ipc](const std::string& s)
            {
                ipc->evaluate_javascript_expression(s);
            })
                                                       : plugin_config::eval_js_fn{},
                                                   {} };
            auto r = plugin_config::del(targets, plugin_config::origin::lua_child, plugin_name, params.value("key", ""));
            return r.success ? json{{ "success", true }} : json{{ "error", r.value }};
        }

        if (method == plugin_ipc::child_method::CONFIG_GET_ALL) {
            auto r = plugin_config::get_all(plugin_name);
            return {
                { "config", r.value }
            };
        }

        if (method == plugin_ipc::child_method::PATCHES) {
            const auto& patches = params.is_object() ? params["patches"] : params;

            if (patches.is_array() && !patches.empty()) {
                std::vector<PatchEntry> entries;
                for (const auto& p : patches) {
                    PatchEntry entry;
                    entry.plugin = plugin_name;
                    entry.file = p.value("file", "");
                    entry.find = p.value("find", "");
                    if (p.contains("transforms") && p["transforms"].is_array()) {
                        for (const auto& t : p["transforms"]) {
                            entry.transforms.push_back({
                                t.value("match", ""),
                                t.value("replace", ""),
                            });
                        }
                    }
                    if (!entry.file.empty() && !entry.find.empty()) entries.push_back(std::move(entry));
                }
                patch_registry_set(plugin_name, std::move(entries));
                patch_update_notifier::instance().notify();
            }

            return {
                { "success", true }
            };
        }

        return {
            { "error", "unknown method: " + method }
        };
    });
}

void plugin_loader::start_plugin_backends()
{
    /* has to be called here and not from init() — weak_from_this() returns an
       empty weak_ptr when called from the constructor, before make_shared finishes. */
    if (!m_child_handler_installed) {
        logger.log("Setting up child request handler");
        this->setup_child_request_handler();
        logger.log("Done!");
        m_child_handler_installed = true;
    }

    this->log_enabled_plugins();

    for (auto& plugin : *m_enabledPluginsPtr) {
        if (m_backend_manager->is_any_backend_running(plugin.plugin_name)) {
            continue;
        }

        m_backend_manager->spawn_plugin(plugin);
    }
}
void plugin_loader::set_plugin_enable(std::string plugin_name, bool enabled, bool reload_frontend)
{
    this->set_plugins_enabled({ std::make_pair(plugin_name, enabled) }, reload_frontend);
}

void plugin_loader::set_plugins_enabled(const std::vector<std::pair<std::string, bool>>& plugins, bool reload_frontend)
{
    bool should_start_backends = false;
    std::vector<std::string> plugins_to_disable;

    for (const auto& [name, enabled] : plugins) {
        m_plugin_manager->set_plugin_enabled(name.c_str(), enabled);
        logger.log("requested to {} plugin [{}]", enabled ? "enable" : "disable", name);

        if (enabled) {
            should_start_backends = true;
        } else {
            plugins_to_disable.push_back(name);
        }
    }

    /** destroy all plugins that need to be disabled */
    for (const auto& name : plugins_to_disable) {
        m_backend_manager->destroy_plugin(name);
        mep::crash_event_bus::instance().acknowledge_crash(name);
    }

    /* Refresh stale snapshots so start_plugin_backends() and log_enabled_plugins()
       see the updated enabled-plugin list rather than the init()-time snapshot. */
    *m_plugin_ptr = m_plugin_manager->get_all_plugins();
    *m_enabledPluginsPtr = m_plugin_manager->get_enabled_backends();
    register_packed_plugin_resources(*m_plugin_ptr);

    /** only spawn the plugins that were just enabled — not all non-running
        enabled plugins.  start_plugin_backends() would re-spawn crashed plugins
        that the user didn't ask to restart. */
    if (should_start_backends) {
        if (!m_child_handler_installed) {
            this->setup_child_request_handler();
            m_child_handler_installed = true;
        }
        this->log_enabled_plugins();

        for (const auto& [name, enabled] : plugins) {
            if (!enabled) continue;
            if (m_backend_manager->is_any_backend_running(name)) continue;

            for (auto& plugin : *m_enabledPluginsPtr) {
                if (plugin.plugin_name == name) {
                    m_backend_manager->spawn_plugin(plugin);
                    break;
                }
            }
        }
    }

    inject_frontend_shims(reload_frontend);
}

std::shared_ptr<ipc_main> plugin_loader::get_ipc_main()
{
    return m_ipc_main;
}

std::shared_ptr<backend_manager> plugin_loader::get_backend_manager()
{
    return m_backend_manager;
}

std::shared_ptr<backend_event_dispatcher> plugin_loader::get_backend_event_dispatcher()
{
    return m_backend_event_dispatcher;
}

std::shared_ptr<head::millennium_backend> plugin_loader::get_millennium_backend()
{
    return this->m_millennium_backend;
}

std::shared_ptr<plugin_manager> plugin_loader::get_plugin_manager()
{
    return this->m_plugin_manager;
}
