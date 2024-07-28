#pragma once
#include <iostream>
#include <nlohmann/json.hpp>
#include <logger/log.h>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <socket/await_pipe.h>

void ConnectSharedJsContext(std::function<void(websocketpp::client<websocketpp::config::asio_client>*, websocketpp::connection_hdl)> callBack)
{
    const auto onMessage = [](websocketpp::client<websocketpp::config::asio_client>* client, 
        websocketpp::connection_hdl handle, std::shared_ptr<websocketpp::config::core_client::message_type> message) {
    };

    const auto fetchSocketUrl = []() -> std::string {
        try {
            const auto windows = nlohmann::json::parse(Http::Get("http://localhost:8080/json"));

            for (const auto& window : windows) {
                if (window["title"].get<std::string>() == "SharedJSContext") {
                    return window["webSocketDebuggerUrl"].get<std::string>();
                }
            }
        }
        catch (const std::exception& e) {
            LOG_FAIL(e.what());
        }
        return {};
    };

    SocketHelpers socketHelpers;
    SocketHelpers::ConnectSocketProps browserProps = {
        .commonName     = "SharedJSContext",
        .fetchSocketUrl = fetchSocketUrl,
        .onConnect      = callBack,
        .onMessage      = onMessage,
        .bAutoReconnect = false
    };

    std::thread(std::bind(&SocketHelpers::ConnectSocket, socketHelpers, browserProps)).join();
}