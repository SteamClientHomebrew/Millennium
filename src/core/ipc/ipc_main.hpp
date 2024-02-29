#pragma once
#include <nlohmann/json.hpp>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::client<websocketpp::config::asio_client> ws_Client;
/**
 * Namespace for handling Inter-Process Communication (IPC) messages.
 */
namespace IPC {

    /**
     * Handles incoming IPC messages received from the WebSocket server.
     *
     * This function processes incoming IPC messages and performs various actions
     * based on the message content and context. Actions include updating the selected
     * theme, opening the Millennium application, retrieving the theme list, or getting
     * the active theme. It interacts with WebSocket clients and other components to
     * fulfill the requested actions.
     *
     * @param message The JSON object representing the incoming IPC message.
     * @param socket The JSON object representing the WebSocket session information.
     * @param steam_client A pointer to the WebSocket client used for communication.
     * @param hdl The connection handle identifying the WebSocket connection.
     */
	void handleMessage(const nlohmann::basic_json<> message, 
        const nlohmann::basic_json<> socket, 
        ws_Client* steam_client, 
        websocketpp::connection_hdl hdl);
}