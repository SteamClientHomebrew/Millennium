#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "pipe.hpp"
#include <core/impl/mutex_impl.hpp>
#include <utilities/encoding.h>
#include <core/ipc/pipe.hpp>
#include <functional>

#define d6d base64_decode
#define d6e base64_encode

static nlohmann::json call_server_method(nlohmann::basic_json<> message)
{
    const std::string call_script = python::construct_fncall(message["data"]);

    if (!message["data"].contains("pluginName")) {
        console.err("no plugin backend specified, doing nothing...");
    }

    python::return_t response = python::evaluate_lock(message["data"]["pluginName"], call_script);
    nlohmann::json json_ret;

    switch (response.type)
    {
        case python::type_t::_bool:   { json_ret["returnValue"] = (response.plain == "True" ? true : false); break; }
        case python::type_t::_string: { json_ret["returnValue"] = d6e(response.plain); break; }
        case python::type_t::_int:    { json_ret["returnValue"] = stoi(response.plain); break; }

        case python::type_t::_err: {
            json_ret["failedRequest"] = true;
            json_ret["failMessage"] = response.plain;
            break;
        }
    }

    json_ret["id"] = message["iteration"];
    return json_ret; 
}

static nlohmann::json front_end_loaded(nlohmann::basic_json<> message)
{
    python::run_lock(
        message["data"]["pluginName"],
        "plugin._front_end_loaded()"
    );

    return nlohmann::json({
        { "id", message["iteration"] },
        { "success", true }
    });
}

typedef websocketpp::server<websocketpp::config::asio> server;

void on_message(server* serv, websocketpp::connection_hdl hdl, server::message_ptr msg) {

    server::connection_ptr con = serv->get_con_from_hdl(hdl);
    try
    {
        auto json_data = nlohmann::json::parse(msg->get_payload());
        std::string response;

        switch (json_data["id"].get<int>()) {
            case ipc_pipe::type_t::call_server_method: {
                response = (::call_server_method(json_data).dump()); break;
            }
            case ipc_pipe::type_t::front_end_loaded: {
                response = (::front_end_loaded(json_data).dump()); break;
            }
        }
        con->send(response, msg->get_opcode());
    }
    catch (nlohmann::detail::exception& ex) {
        con->send(std::string(ex.what()), msg->get_opcode());
    }
    catch (std::exception& ex) {
        con->send(std::string(ex.what()), msg->get_opcode());
    }
}

void on_open(server* s, websocketpp::connection_hdl hdl) {
    s->start_accept();
}

const void ipc_pipe::_create()
{
    std::thread([&] {
        server wss;

        try {
            wss.set_access_channels(websocketpp::log::alevel::none);
            wss.clear_access_channels(websocketpp::log::alevel::all);

            wss.init_asio();
            wss.set_message_handler(bind(on_message, &wss, std::placeholders::_1, std::placeholders::_2));
            wss.set_open_handler(bind(&on_open, &wss, std::placeholders::_1));

            wss.listen(12906);
            wss.start_accept();
            wss.run();
        }
        catch (const std::exception& e) {
            console.err("[pipecon] uncaught error -> {}", e.what());
        }
    }).detach();

    return void();
}
