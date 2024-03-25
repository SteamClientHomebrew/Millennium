#define _WINSOCKAPI_
#include <optional>
#include <core/injector/event_handler.hpp>

#include <stdafx.h>
#include <core/steam/cef_manager.hpp>
#include <utils/config/config.hpp>
#include "colors/accent_colors.hpp"

#include <filesystem>
#include <window/interface_v2/settings.hpp>

/**
 * @brief Adds a stylesheet to the DOM.
 *
 * This function generates a JavaScript script to add a stylesheet to the DOM if it's not already injected.
 *
 * @param filename The filename of the stylesheet.
 * @return A string containing the JavaScript script.
 */
const std::basic_string<char, std::char_traits<char>, std::allocator<char>> cef_dom::stylesheet::add(std::string filename) noexcept {
    return fmt::format("if (document.querySelectorAll(`link[href='{}']`).length) {{ }} else {{ document.head.appendChild(Object.assign(document.createElement('link'), {{ rel: 'stylesheet', href: '{}', id: 'millennium-injected' }})); }}", filename, filename);
}

/**
 * @brief Adds a JavaScript file to the DOM.
 *
 * This function generates a JavaScript script to add a JavaScript file to the DOM if it's not already injected.
 *
 * @param filename The filename of the JavaScript file.
 * @return A string containing the JavaScript script.
 */
const std::basic_string<char, std::char_traits<char>, std::allocator<char>> cef_dom::javascript::add(std::string filename) noexcept {
    return fmt::format("if (document.querySelectorAll(`script[src='{}'][type='module']`).length) {{ }} else {{ document.head.appendChild(Object.assign(document.createElement('script'), {{ src: '{}', type: 'module', id: 'millennium-injected' }})); }}", filename, filename);
}

/**
 * @brief Gets the singleton instance of the CEF DOM.
 *
 * @return A reference to the singleton instance of the CEF DOM.
 */
cef_dom& cef_dom::get() 
{
    static cef_dom instance;
    return instance;
}

/**
 * @brief Generates a JavaScript script to set the system accent color.
 *
 * This function generates a JavaScript script to set the system accent color.
 *
 * @return A string containing the JavaScript script.
 */
std::string system_color_script() 
{
    return fmt::format("(document.querySelector('#SystemAccentColorInject') || document.head.appendChild(Object.assign(document.createElement('style'), {{ id: 'SystemAccentColorInject' }}))).innerText = `{}`;", getColorStr());
}

/**
 * @brief Generates a JavaScript script to set the theme colors.
 *
 * This function generates a JavaScript script to set the theme colors.
 *
 * @return A string containing the JavaScript script.
 */
std::string theme_colors() 
{
    const auto colors = config.getRootColors(skin_json_config);
    return colors != "[NoColors]" ? fmt::format("(document.querySelector('#globalColors') || document.head.appendChild(Object.assign(document.createElement('style'), {{ id: 'globalColors' }}))).innerText = `{}`;", colors) : "";
}

/**
 * @brief Pushes a raw script to the WebSocket for execution.
 *
 * This function pushes a raw script to the WebSocket for execution using the Runtime.evaluate method.
 *
 * @param c Pointer to the WebSocket client.
 * @param hdl WebSocket connection handle.
 * @param raw_script The raw script to be executed.
 * @param sessionId The session ID associated with the WebSocket connection.
 */
void steam_cef_manager::push_to_socket(ws_Client* c, websocketpp::connection_hdl hdl, std::string raw_script, std::string sessionId) noexcept
{
    try {
        std::function<void(const std::string&)> runtime_evaluate = [&](const std::string& script) -> void {
            nlohmann::json evaluate_script = {
                {"id", response::script_inject_evaluate},
                {"method", "Runtime.evaluate"},
                {"params", {{"expression", script}}}
            };

            if (!sessionId.empty()) evaluate_script["sessionId"] = sessionId;
            c->send(hdl, evaluate_script.dump(4), websocketpp::frame::opcode::text);
        };

        //inject the script that the user wants
        runtime_evaluate(theme_colors() + "\n\n\n" + system_color_script() + "\n\n\n" + raw_script);
    }
    catch (const asio::system_error& ex) {

        if (ex.code().value() == asio::error::operation_aborted || ex.code().value() == asio::error::eof)
        {
            console.warn("[non-fatal] steam garbage clean-up aborted a connected socket, re-establishing connection");
        }
    }
}
/**
 * @brief Discovers resources from a remote endpoint.
 * ~~DEPRECATED AND NEEDS TO BE MOVED TO HTTP::GET FOR CONSISTENCY~~
 *
 * This function discovers resources from a remote endpoint by making an HTTP request.
 *
 * @param remote_endpoint The remote endpoint to discover resources from.
 * @return A string containing the discovered resources.
 */
inline std::string steam_cef_manager::discover(std::basic_string<char, std::char_traits<char>, std::allocator<char>> remote_endpoint)
{
    return http::get(remote_endpoint);
}

/**
 * @brief Renders the settings modal in the Steam CEF manager.
 *
 * This function renders the settings modal in the Steam CEF manager by pushing the JavaScript code
 * of the modal script to the WebSocket for execution.
 *
 * @param steam_client Pointer to the WebSocket client.
 * @param hdl WebSocket connection handle.
 * @param sessionId The session ID associated with the WebSocket connection.
 */
void steam_cef_manager::render_settings_modal(ws_Client* steam_client, websocketpp::connection_hdl& hdl, std::string sessionId) noexcept
{
    std::string path = fmt::format("{}/{}", uri.steam_resources, "modal.js");

    steam_interface.push_to_socket(steam_client, hdl, cef_dom::get().javascript_handler.add(path), sessionId);
}

/**
 * @brief Injects Millennium UI into the Steam CEF manager.
 *
 * This function injects Millennium UI into the Steam CEF manager by pushing the JavaScript code
 * generated by the settings page renderer to the WebSocket for execution.
 *
 * @param steam_client Pointer to the WebSocket client.
 * @param hdl WebSocket connection handle.
 * @param sessionId The session ID associated with the WebSocket connection.
 */
void steam_cef_manager::inject_millennium(ws_Client* steam_client, websocketpp::connection_hdl& hdl, std::string sessionId) noexcept
{
    std::string javaScript = ui_interface::settings_page_renderer();
    steam_interface.push_to_socket(steam_client, hdl, javaScript, sessionId);
}

/**
 * @brief Calculates the endpoint for a given script file path.
 *
 * This function calculates the endpoint for a script file path. If the path is a remote URL,
 * it retains the information. If it is a local path, it adds the loopback host.
 *
 * @param endpoint_unparsed The unparsed endpoint to calculate.
 */
const void steam_cef_manager::calculate_endpoint(std::string& endpoint_unparsed)
{
    //calculate endpoint type, if remote url retain info, if local add loopbackhost
    if (endpoint_unparsed.compare(0, 4, "http") != 0) {

        //remote skin use the remote skins as a current directory from github
//        if (millennium_remote.is_remote) endpoint_unparsed = fmt::format("{}/{}", millennium_remote.host, endpoint_unparsed);
//        //just use the default from the skin path
//        else
            endpoint_unparsed = fmt::format("{}/skins/{}/{}", uri.steam_resources, Settings::Get<std::string>("active-skin"), endpoint_unparsed);
    }
}

/**
 * @brief Evaluates a script file or code block on the CEF (Chromium Embedded Framework) manager.
 *
 * This function evaluates a script file or code block on the CEF manager. It checks whether the
 * operation is allowed based on the script type and user settings. Then, it calculates the endpoint
 * for the script file, optionally extracts the session ID from the WebSocket response, and pushes
 * the script to the WebSocket for execution.
 *
 * @param c Pointer to the WebSocket client.
 * @param hdl WebSocket connection handle.
 * @param file The script file to evaluate.
 * @param type The type of the script (JavaScript or stylesheet).
 * @param socket_response The JSON response received from the WebSocket.
 */
const void steam_cef_manager::evaluate(ws_Client* c, websocketpp::connection_hdl hdl, std::string file, script_type type, nlohmann::basic_json<> socket_response)
{
    // Check if JavaScript execution is allowed
    if (type == script_type::javascript && (!Settings::Get<bool>("allow-javascript"))) {
        console.err(fmt::format("the requested operation [{}()] was cancelled. reason: script_type::javascript is prohibited", __func__));
        return;
    }

    // Check if stylesheet execution is allowed
    if (type == script_type::stylesheet && (!Settings::Get<bool>("allow-stylesheet"))) {
        console.err(fmt::format("the requested operation [{}()] was cancelled. reason: script_type::stylesheet is prohibited", __func__));
        return;
    }

    // Calculate the endpoint for the script file on the local system
    this->calculate_endpoint(file);

    std::optional<std::string> sessionId;

    // Extract sessionId from socket_response if available
    if (!socket_response.is_null() && socket_response.contains("sessionId")) {
        sessionId = socket_response["sessionId"].get<std::string>();
    }

    // Pass in sessionId if available and push the script to the WebSocket for execution
    switch (type)
    {
        case script_type::javascript:
            steam_interface.push_to_socket(c, hdl, cef_dom::get().javascript_handler.add(file).data(), sessionId.value_or(std::string()));
            break;
        case script_type::stylesheet:
            steam_interface.push_to_socket(c, hdl, cef_dom::get().stylesheet_handler.add(file).data(), sessionId.value_or(std::string()));
            break;
        default:
            throw std::runtime_error("[debug] invalid target scripting type");
    }
}


using EventHandler = std::function<void(const nlohmann::json& event_msg, int listenerId)>;

/**
 * @brief Singleton class for managing message events and their handlers.
 */
class MessageEmitter {
private:
    MessageEmitter() {} /**< Private constructor to prevent direct instantiation. */
    // Map to store event handlers for each event
    std::unordered_map<std::string, std::vector<std::pair<int, EventHandler>>> events;
    int nextListenerId = 0; /**< Next available listener ID. */

public:
    MessageEmitter(const MessageEmitter&) = delete; /**< Deleted copy constructor. */
    MessageEmitter& operator=(const MessageEmitter&) = delete; /**< Deleted copy assignment operator. */

    /**
     * @brief Get the singleton instance of MessageEmitter.
     *
     * @return The singleton instance of MessageEmitter.
     */
    static MessageEmitter& getInstance() {
        static MessageEmitter instance; // Static instance
        return instance;
    }

    /**
     * @brief Register an event handler for a given event.
     *
     * @param event The event name.
     * @param handler The event handler function.
     * @return The listener ID associated with the registered handler.
     */
    int on(const std::string& event, EventHandler handler) {
        int listenerId = nextListenerId++;
        events[event].push_back(std::make_pair(listenerId, handler));
        return listenerId;
    }

    /**
     * @brief Remove an event handler for a given event.
     *
     * @param event The event name.
     * @param listenerId The listener ID associated with the handler to remove.
     */
    void off(const std::string& event, int listenerId) {
        auto it = events.find(event);
        if (it != events.end()) {
            auto& handlers = it->second;
            handlers.erase(std::remove_if(handlers.begin(), handlers.end(), [listenerId](const auto& handler) {
                return handler.first == listenerId;
                }), handlers.end());
        }
    }

    /**
     * @brief Emit an event, invoking all registered handlers.
     *
     * @param event The event name.
     * @param data The data associated with the event.
     */
    void emit(const std::string& event, const nlohmann::json& data) {
        auto it = events.find(event);
        if (it != events.end()) {
            const auto& handlers = it->second;
            for (const auto& handler : handlers) {
                handler.second(data, handler.first); // Invoke the handler
            }
        }
    }
};

/**
 * @brief Establishes a WebSocket handshake with the SteamJSContext server.
 *
 * This function attempts to establish a WebSocket connection with the SteamJSContext server
 * using the provided callback function. It retrieves the WebSocket URL from the discovered
 * instances and initializes the WebSocket client to connect to it.
 *
 * @param callback The callback function to be invoked upon successful connection.
 */
const void steam_js_context::establish_socket_handshake(std::function<void()> callback)
{
    console.log("attempting to create a connection to SteamJSContext");

    if (callback == nullptr)
        throw std::runtime_error("callback function was nullptr");

    std::string sharedContextUrl;

    try {
        nlohmann::basic_json<> instances = nlohmann::json::parse(steam_interface.discover(uri.debugger + "/json"));

        for (const auto instance : instances) {
            if (instance["title"] == "SharedJSContext") {
                sharedContextUrl = instance["webSocketDebuggerUrl"];
            }
        }

        if (sharedContextUrl.empty()) {
            console.err("Couldn't find SharedJSContext webSocketDebuggerUrl");
        }
    }
    catch (const http_error& ex) {
        console.err(ex.what());
        return;
    }
    catch (nlohmann::detail::exception& ex) {
        console.err(ex.what());
        return;
    }

    try {

        websocketpp::client<websocketpp::config::asio_client> cl;

        std::cout << sharedContextUrl << std::endl;

        cl.set_access_channels(websocketpp::log::alevel::all);
        cl.clear_access_channels(websocketpp::log::alevel::all);
        cl.init_asio();

        cl.set_open_handler(bind([&](ws_Client* _c, websocketpp::connection_hdl _hdl) -> void {
            hdl = _hdl;
            c = _c;
            static_cast<void>(callback());
        }, & cl, ::_1));
        cl.set_message_handler(bind([&](ws_Client* c, websocketpp::connection_hdl hdl, message_ptr msg) -> void {
            MessageEmitter::getInstance().emit("msg", nlohmann::json::parse(msg->get_payload()));

        }, & cl, ::_1, ::_2));

        websocketpp::lib::error_code ec;
        ws_Client::connection_ptr con = cl.get_connection(sharedContextUrl, ec);
        if (ec) {
            console.err(fmt::format("could not create connection because: {}", ec.message()));
            return;
        }

        cl.connect(con); cl.run();
    }
    catch (websocketpp::exception& ex) {
        console.err("Failed to connect to SharedJSContext");
        console.err(ex.what());
    }

}

steam_js_context::steam_js_context() { }

/// <summary>
/// deprecated
/// </summary>
/// <returns></returns>
inline const void steam_js_context::close_socket() noexcept { }

/**
 * @brief Reloads the SteamJSContext by establishing a new WebSocket handshake and triggering a reload.
 *
 * This function establishes a new WebSocket handshake with the SteamJSContext server and triggers
 * a reload using the DevTools Protocol method Page.reload().
 */
const void steam_js_context::reload() noexcept
{
    establish_socket_handshake([this]() {
        //https://chromedevtools.github.io/devtools-protocol/tot/Page/#method-reload
        c->send(hdl,
            nlohmann::json({ {"id", 89}, {"method", "Page.reload"} }).dump(), 
            websocketpp::frame::opcode::text
        );
        c->close(hdl, websocketpp::close::status::normal, "Successful connection");
    });
}

/**
 * @brief Restarts the SteamJSContext by establishing a new WebSocket handshake.
 *
 * This function establishes a new WebSocket handshake with the SteamJSContext server.
 * It may contain legacy code for restarting the context, which is not used anymore.
 */
const void steam_js_context::restart() noexcept
{
    // this function is not tested as of 2024-02-15 since the websocket dependecy switch
    establish_socket_handshake([this]() {
        //not used anymore, but here in case
        c->send(hdl,
            nlohmann::json({
                { "id", 8987 },
                { "method", "Runtime.evaluate" },
                { "params", {
                    { "expression", "setTimeout(() => {SteamClient.User.StartRestart(true)}, 1000)" }
                }}
            }).dump(),
            websocketpp::frame::opcode::text
        );
        c->close(hdl, websocketpp::close::status::normal, "Successful connection");
    });
}

/**
 * @brief Executes a JavaScript command on the SteamJSContext and returns the result.
 *
 * This function sends a JavaScript command to the SteamJSContext via WebSocket, evaluates it,
 * and returns the result.
 *
 * @param javascript The JavaScript command to execute.
 * @return The result of executing the JavaScript command.
 */
std::string steam_js_context::exec_command(std::string javascript)
{
    // Create a promise to store the result
    std::promise<std::string> promise;

    // Establish WebSocket handshake and send the JavaScript command
    establish_socket_handshake([&]() {
        c->send(hdl,
            nlohmann::json({ {"id", 1337}, {"method", "Runtime.evaluate"}, {"params", {{"expression", javascript}, {"awaitPromise", true}}} }).dump(),
            websocketpp::frame::opcode::text
        );

        // Register message handler to capture the result
        MessageEmitter::getInstance().on("msg", [&](const json& event_msg, int listenerId) {
            try {
                console.log("MessageEmitter.on() received a message");
                promise.set_value(event_msg["result"].dump());

                // Set the value of the promise with the result
                c->close(hdl, websocketpp::close::status::normal, "Successful connection");
                MessageEmitter::getInstance().off("msg", listenerId);
            }
            catch (std::future_error& ex) {
                console.err(ex.what());
            }
        });
    });

    // Return the result obtained from the promise
    return promise.get_future().get();
}