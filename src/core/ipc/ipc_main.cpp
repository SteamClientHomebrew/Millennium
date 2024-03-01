#include "ipc_main.hpp"
#include <iostream>
#include <window/core/window.hpp>
#include <Windows.h>
#include <utils/thread/thread_handler.hpp>
#include <window/win_handler.hpp>

#include <utils/config/config.hpp>
#include <utils/cout/logger.hpp>
#include <filesystem>

#include <window/interface/globals.h>
#include "handlers/types.hpp"

/**
 * Escapes special characters within a JSON value.
 *
 * This function traverses the JSON value recursively and escapes special characters
 * within string values. Special characters such as newline (\n), backslash (\\), and
 * double quote (") are replaced with their escape sequences (\n, \\, \").
 *
 * @param value The JSON value to escape special characters within.
 */
void escapeSpecialChars(nlohmann::json& value) {
    // Check if the value is a string.
    if (value.is_string()) {
        // Retrieve the string value.
        std::string escapedStr = value.get<std::string>();
        // Iterate through the characters of the string.
        for (size_t i = 0; i < escapedStr.length(); ++i) {
            // Check for special characters and replace them with escape sequences.
            if (escapedStr[i] == '\n') { escapedStr.replace(i, 1, "\\n");  i++; }
            else if (escapedStr[i] == '\\') { escapedStr.replace(i, 1, "\\\\"); i++; }
            else if (escapedStr[i] == '"') { escapedStr.replace(i, 1, "\\\""); i++; }
        }
        // Update the JSON value with the escaped string.
        value = escapedStr;
    }
    // If the value is an object, recursively call escapeSpecialChars on each key-value pair.
    else if (value.is_object()) {
        for (auto& pair : value.items()) {
            escapeSpecialChars(pair.value());
        }
    }
    // If the value is an array, recursively call escapeSpecialChars on each element.
    else if (value.is_array()) {
        for (nlohmann::json& element : value) {
            escapeSpecialChars(element);
        }
    }
}


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
        std::cout << "requested to open millennium" << std::endl;

        if (!g_windowOpen) {
            threadContainer::getInstance().addThread(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)init_main_window, 0, 0, 0));
        }
        else {
            Window::bringToFront();
            PlaySoundA("SystemExclamation", NULL, SND_ALIAS);
        }
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

    if (message["id"] == "[get-active]") 
    {
        auto resp = config.getThemeData();
        std::string name = resp.contains("name") ? resp["name"].get<std::string>() : Settings::Get<std::string>("active-skin");

        respond("[get-active]", name);
    }
}
