#include <util/sockets.h>

void ReloadSteam() {
    ConnectSharedJsContext([](websocketpp::client<websocketpp::config::asio_client>* client, websocketpp::connection_hdl handle) {
        client->send(handle, nlohmann::json({ { "id", 0 }, { "method", "Page.enable" } }).dump(), websocketpp::frame::opcode::text);
        client->send(handle, nlohmann::json({ { "id", 0 }, { "method", "Page.reload" } }).dump(), websocketpp::frame::opcode::text);

        client->close(handle, websocketpp::close::status::normal, "Closing connection");
    });  
}