#pragma once
#include <nlohmann/json.hpp>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::client<websocketpp::config::asio_client> ws_Client;

namespace IPC {

	void handleMessage(const nlohmann::basic_json<> message, const nlohmann::basic_json<> socket, ws_Client* steam_client, websocketpp::connection_hdl hdl);
}