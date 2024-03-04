#include "ipc_main.hpp"
#include "handlers/types.hpp"

#include <utils/config/config.hpp>
#include <utils/cout/logger.hpp>

/* create and manage the interface thread */
#include <utils/thread/thread_handler.hpp>

#include <window/win_handler.hpp>
#include <window/interface/globals.h>
#include <window/core/window.hpp>
#include <window/api/installer.hpp>

/**
 * Handles incoming IPC (Inter-Process Communication) messages.
 *
 * This method processes incoming IPC messages received from the WebSocket server.
 * It performs different actions based on the message ID, such as updating the selected skin,
 * opening the Millennium application, retrieving the theme list, or getting the active theme.
 * Additionally, it interacts with various components, such as theme configurations, UI updates,
 * and WebSocket communication, to fulfill the requested actions.
 *
 * @param message The JSON object representing the incoming IPC message.
 * @param socket The JSON object representing the WebSocket session information.
 * @param steam_client A pointer to the WebSocket client used for communication.
 * @param hdl The connection handle identifying the WebSocket connection.
 */

void IPC::handleMessage(const nlohmann::basic_json<> message, 
    const nlohmann::basic_json<> socket,
    ws_Client* steam_client, 
    websocketpp::connection_hdl hdl)
{
    /**
     * Sends a message to a WebSocket server through a WebSocket client.
     *
     * This function constructs a JSON message containing the provided message content
     * and necessary parameters, such as message ID, method, and session ID. The message
     * is then sent to the WebSocket server using the specified WebSocket client and connection handle.
     *
     * @param message The message content to be sent.
     * @param socket The JSON object representing the WebSocket session information.
     * @param steam_client A pointer to the WebSocket client used to send the message.
     * @param hdl The connection handle identifying the WebSocket connection.
     */
    const auto respond = [&](std::string type, std::string message) {
        std::string content = std::format("console.log({{ returnId: '{}', message: `{}` }})", type, message);
        // Construct the JSON message to be sent.
        const nlohmann::json response = {
            { "id", 8987 },                            // Message ID.
            { "method", "Runtime.evaluate" },          // Method for message evaluation.
            { "params", {{ "expression", content }}},  // Parameters containing the message expression.
            { "sessionId", socket["sessionId"] }      // Session ID associated with the WebSocket.
        };

        // Send the JSON message to the WebSocket server using the WebSocket client.
        steam_client->send(hdl, response.dump(), websocketpp::frame::opcode::text);
    };

    console.log(std::format("handling message: {}", message.dump(4)));

    if (message["id"] == "[open-millennium]") {
        console.log("requested to open millennium interface");

        if (!g_windowOpen) {
            c_threads::get().add(std::thread([&] { init_main_window(); }));
        }
        else {
            //Window::bringToFront();
#ifdef _WIN32
            PlaySoundA("SystemExclamation", NULL, SND_ALIAS);
#endif
        }
    }
    if (message["id"] == "[install-theme]")
    {
        console.log("received a theme install message");

        std::string file_name = message["file"];
        std::string download = message["url"];

        Community::Themes->handleThemeInstall(file_name, download);
    }
    if (message["id"] == "[get-request]")
    {
        console.log("received a get cor query");

        if (!message.contains("url") || !message.contains("user_agent")) {
            respond("[get-request]", nlohmann::json({
                {"success", false},
                {"message", "you must provide 'url' and 'user_agent' in request"}
            }).dump());
            return;
        }

        auto url = message["url"].get<std::string>();
        auto user_agent = message["user_agent"].get<std::string>();

        const auto http_response = ipc_handler::cor_request(url, user_agent);

        respond("[get-request]", nlohmann::json({
            {"success", http_response.success},
            {"content", http_response.content}
        }).dump());
    }
}
