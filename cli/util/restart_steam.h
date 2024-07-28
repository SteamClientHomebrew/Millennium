#include <util/sockets.h>
#include <thread>

void RestartSteam() {
    ConnectSharedJsContext([](websocketpp::client<websocketpp::config::asio_client>* client, websocketpp::connection_hdl handle) {
        client->send(handle, nlohmann::json({ { "id", 0 }, { "method", "Runtime.enable" } }).dump(), websocketpp::frame::opcode::text);
        client->send(handle, nlohmann::json({ { "id", 0 }, { "method", "Runtime.evaluate" }, { 
            "params", { { "expression", "SteamClient.User.StartRestart(false)" } } } 
        }).dump(), websocketpp::frame::opcode::text);

        client->close(handle, websocketpp::close::status::normal, "Closing connection");
    });
}