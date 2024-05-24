#include "loader.hpp"
#include <string>
#include <iostream>
#include <Python.h>
#include <__builtins__/executor.h>
#include <core/backend/co_spawn.hpp>
#include <core/ipc/pipe.hpp>
#include <core/impl/mutex_impl.hpp>
#include <utilities/http.h>
#include <socket/await_pipe.h>
#include <core/hooks/web_load.h>
#include <generic/base.h>

websocketpp::client<websocketpp::config::asio_client>* plugin_client, *browser_c;
websocketpp::connection_hdl plugin_handle, browser_h;

extern std::string sessionId;
static std::chrono::system_clock::time_point bootstrap_start;

void set_start_time(std::chrono::system_clock::time_point time) {
    bootstrap_start = time;
}

// @ src\core\co_initialize\co_stub.cc
const void inject_shims(void);
void plugin_start_cb(stream_buffer::plugin_mgr::plugin_t& plugin);

enum socket_t {
    SHARED, 
    GLOBAL
};

bool post_virtual(nlohmann::json data, socket_t type) {
    if ((type == socket_t::SHARED ? plugin_client : browser_c) == nullptr) {
        console.err("not connected to steam, cant post message");
        return false;
    }

    (type == socket_t::SHARED ? plugin_client : browser_c)->send(
        (type == socket_t::SHARED ? plugin_handle : browser_h), data.dump(), websocketpp::frame::opcode::text);

    return true;
}

bool tunnel::post_shared(nlohmann::json data) {
    return post_virtual(data, socket_t::SHARED);
}
bool tunnel::post_global(nlohmann::json data) {
    return post_virtual(data, socket_t::GLOBAL);
}

namespace browser {

    const void on_message(websocketpp::client<websocketpp::config::asio_client>* c, 
                websocketpp::connection_hdl hdl, 
                websocketpp::config::asio_client::message_type::ptr msg) 
    {

        const auto json = nlohmann::json::parse(msg->get_payload());    
        webkit_handler::get().handle_hook(json);
    }

    const void on_connect(websocketpp::client<websocketpp::config::asio_client>* _c, websocketpp::connection_hdl _hdl) 
    {
        browser_c = _c; browser_h = _hdl;
        console.log("established browser connection @ {:p}", static_cast<void*>(&_c));

        // setup webkit handler to hook default modules
        webkit_handler::get().setup_hook();
    }
}

namespace shared_context {

    const void on_message(websocketpp::client<websocketpp::config::asio_client>* c, 
                websocketpp::connection_hdl hdl, 
                websocketpp::config::asio_client::message_type::ptr msg) 
    { }

    const void on_connect(websocketpp::client<websocketpp::config::asio_client>* _c, websocketpp::connection_hdl _hdl) 
    {
        plugin_client = _c; plugin_handle = _hdl;
        console.log("established shared connection @ {:p}", static_cast<void*>(&plugin_client));

        // setup plugin frontends
        inject_shims();
    }
}

void connect_socket(
    std::string common_name, 
    std::function<std::string()> fetch_socket,
    std::function<void(websocketpp::client<websocketpp::config::asio_client> *_c, websocketpp::connection_hdl _hdl)> on_connect,
    std::function<void(websocketpp::client<websocketpp::config::asio_client> *c, websocketpp::connection_hdl hdl, std::shared_ptr<websocketpp::config::core_client::message_type> msg)> on_message
) {
    while (true) {
        const std::string ctx = fetch_socket();

        try {
            websocketpp::client<websocketpp::config::asio_client> cl;
            cl.set_access_channels(websocketpp::log::alevel::none);
            cl.init_asio();

            cl.set_open_handler(bind(on_connect, &cl, std::placeholders::_1));
            cl.set_message_handler(bind(on_message, &cl, std::placeholders::_1, std::placeholders::_2));

            websocketpp::lib::error_code ec;
            websocketpp::client<websocketpp::config::asio_client>::connection_ptr con = cl.get_connection(ctx, ec);

            if (ec) {
                console.err("could not create connection because: {}", ec.message());
                return;
            }
            cl.connect(con); cl.run();
        }
        catch (websocketpp::exception& ex) 
        {
            console.err("Failed to connect to steam.");
            console.err(ex.what());
        }

        console.err("{} tunnel collapsed, attempting to reopen...", common_name);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void _connect_browser() {
    connect_socket("browser", get_steam_context, browser::on_connect, browser::on_message);
}

void _connect_shared() {
    connect_socket("sharedctx", get_shared_js, shared_context::on_connect, shared_context::on_message);
}

void plugin::bootstrap()
{
    console.head("backend:");

    int error;
    error = stream_buffer::setup_config();
    console.log_item("setup_config", !error ? "failed" : "success");

    error = ipc_pipe::_create();
    console.log_item("pipemon", !error ? "failed" : "success", true);

    auto plugins = std::make_shared<std::vector<stream_buffer::plugin_mgr::plugin_t>>(stream_buffer::plugin_mgr::parse_all()); 
    console.head("hosting query:");

    for (auto it = (*plugins).begin(); it != (*plugins).end(); ++it) 
    {
        const auto name = (*it).name;
        console.log_item(name, stream_buffer::plugin_mgr::is_enabled(name) ? "enabled" : "disabled", std::next(it) == (*plugins).end());
    }

    for (auto& plugin : *plugins) 
    {
        plugin_manager& manager = plugin_manager::get();

        if (!stream_buffer::plugin_mgr::is_enabled(plugin.name)) { 
            continue;
        }

        std::function<void(stream_buffer::plugin_mgr::plugin_t&)> cb = std::bind(plugin_start_cb, std::placeholders::_1);
        std::thread(std::bind(&plugin_manager::create_instance, &manager, std::ref(plugin), cb)).detach();
    }


    std::thread t_browser(_connect_browser);
    std::thread t_shared(_connect_shared);

    // Calculate the duration
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - bootstrap_start);
    console.log_item("exec", fmt::format("finished in [{}ms]\n", duration.count()), true);

    t_browser.join();
    t_shared.join();
    console.log("browser thread and shared thread exited...");
}
