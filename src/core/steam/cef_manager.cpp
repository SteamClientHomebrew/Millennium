#define _WINSOCKAPI_   

#include <core/injector/event_handler.hpp>

#include <stdafx.h>
#include <core/steam/cef_manager.hpp>
#include <utils/config/config.hpp>
#include "colors/accent_colors.hpp"

#include <filesystem>
#include <window/interface_v2/settings.hpp>

const std::string cef_dom::runtime::evaluate(std::string remote_url) noexcept {
    return R"(
    (() => fetch(')" + remote_url + R"(').then((response) => response.text()).then((data) => { 
        if (window.usermode === undefined) {
            const establishConnection = () => {
                const millennium = new WebSocket('ws://localhost:)" + std::to_string(steam_client::get_ipc_port()) + R"(');
                millennium.onmessage = (event) => {
                    if (usermode.ipc && usermode.ipc.onmessage) {
                        usermode.ipc.onmessage(JSON.parse(event.data));
                    }
                };
                return {
                    send: (message) => {
                        millennium.send(JSON.stringify(message));
                    },
                    onmessage: null
                };
            };
            window.usermode = {
                ipc: establishConnection(),
                ipc_types: { open_skins_folder: 0, skin_update: 1, open_url: 2, change_javascript: 3, change_console: 4, get_skins: 5 }
            };
            eval(data);
        }
    }))())";
}

const std::basic_string<char, std::char_traits<char>, std::allocator<char>> cef_dom::stylesheet::add(std::string filename) noexcept {
    return std::format("if (document.querySelectorAll(`link[href='{}']`).length) {{ console.log('millennium stylesheet already injected'); }} else {{ document.head.appendChild(Object.assign(document.createElement('link'), {{ rel: 'stylesheet', href: '{}', id: 'millennium-injected' }})); }}", filename, filename);
}

const std::basic_string<char, std::char_traits<char>, std::allocator<char>> cef_dom::javascript::add(std::string filename) noexcept {
    return std::format("if (document.querySelectorAll(`script[src='{}'][type='module']`).length) {{ console.log('millennium already injected'); }} else {{ document.head.appendChild(Object.assign(document.createElement('script'), {{ src: '{}', type: 'module', id: 'millennium-injected' }})); }}", filename, filename);
}

cef_dom& cef_dom::get() {
    static cef_dom instance;
    return instance;
}

std::string system_color_script() {
    return std::format("(document.querySelector('#SystemAccentColorInject') || document.head.appendChild(Object.assign(document.createElement('style'), {{ id: 'SystemAccentColorInject' }}))).innerText = `{}`;", getColorStr());
}
std::string theme_colors() {
    const auto colors = config.getRootColors(skin_json_config);

    return colors != "[NoColors]" ? std::format("(document.querySelector('#globalColors') || document.head.appendChild(Object.assign(document.createElement('style'), {{ id: 'globalColors' }}))).innerText = `{}`;", colors) : "";
}

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

inline std::string steam_cef_manager::discover(std::basic_string<char, std::char_traits<char>, std::allocator<char>> remote_endpoint)
{
    HINTERNET hInternet = InternetOpenA("millennium.patcher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    HINTERNET hUrl = InternetOpenUrlA(hInternet, remote_endpoint.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);

    if (!hUrl || !hInternet)
    {
        throw std::runtime_error(std::format("the requested operation [{}()] failed. reason: InternetOpen*A was nullptr", __func__));
    }

    char buffer[1024];
    DWORD total_bytes_read = 0;
    std::string discovery_result;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &total_bytes_read) && total_bytes_read) discovery_result.append(buffer, total_bytes_read);
    InternetCloseHandle(hUrl); InternetCloseHandle(hInternet);

    return discovery_result;
}

__declspec(noinline) void __fastcall steam_cef_manager::render_settings_modal(ws_Client* steam_client, websocketpp::connection_hdl& hdl, std::string sessionId) noexcept
{
    std::string path = std::format("{}/{}", uri.steam_resources, "modal.js");

    steam_interface.push_to_socket(steam_client, hdl, cef_dom::get().javascript_handler.add(path), sessionId);
}


__declspec(noinline) void __fastcall steam_cef_manager::inject_millennium(ws_Client* steam_client, websocketpp::connection_hdl& hdl, std::string sessionId) noexcept
{
    std::string javaScript = ui_interface::settings_page_renderer();

    std::cout << "Pushing script to ClientInterface" << std::endl;

    steam_interface.push_to_socket(steam_client, hdl, javaScript, sessionId);
}

const void steam_cef_manager::calculate_endpoint(std::string& endpoint_unparsed)
{
    //calculate endpoint type, if remote url retain info, if local add loopbackhost
    if (endpoint_unparsed.compare(0, 4, "http") != 0) {

        //remote skin use the remote skins as a current directory from github
        if (millennium_remote.is_remote) endpoint_unparsed = std::format("{}/{}", millennium_remote.host, endpoint_unparsed);
        //just use the default from the skin path
        else endpoint_unparsed = std::format("{}/skins/{}/{}", uri.steam_resources, Settings::Get<std::string>("active-skin"), endpoint_unparsed);
    }
}

const void steam_cef_manager::evaluate(ws_Client* c, websocketpp::connection_hdl hdl, std::string file, script_type type, nlohmann::basic_json<> socket_response)
{
    if (type == script_type::javascript && (!Settings::Get<bool>("allow-javascript"))) {
        console.err(std::format("the requested operation [{}()] was cancelled. reason: script_type::javascript is prohibited", __func__));
        return;
    }

    if (type == script_type::stylesheet && (!Settings::Get<bool>("allow-stylesheet"))) {
        console.err(std::format("the requested operation [{}()] was cancelled. reason: script_type::stylesheet is prohibited", __func__));
        return;
    }

    this->calculate_endpoint(file);

    std::optional<std::string> sessionId;

    if (!socket_response.is_null() && socket_response.contains("sessionId")) {
        sessionId = socket_response["sessionId"].get<std::string>();
    }

    //pass in sessionId if available
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

class MessageEmitter {
private:
    MessageEmitter() {}
    // Map to store event handlers for each event
    std::unordered_map<std::string, std::vector<std::pair<int, EventHandler>>> events;
    int nextListenerId = 0;

public:
    MessageEmitter(const MessageEmitter&) = delete;
    MessageEmitter& operator=(const MessageEmitter&) = delete;

    // Get the singleton instance
    static MessageEmitter& getInstance() {
        static MessageEmitter instance; // Static instance
        return instance;
    }

    // Register an event handler for a given event
    int on(const std::string& event, EventHandler handler) {
        int listenerId = nextListenerId++;
        events[event].push_back(std::make_pair(listenerId, handler));
        return listenerId;
    }

    // Remove an event handler for a given event
    void off(const std::string& event, int listenerId) {
        auto it = events.find(event);
        if (it != events.end()) {
            auto& handlers = it->second;
            handlers.erase(std::remove_if(handlers.begin(), handlers.end(), [listenerId](const auto& handler) {
                return handler.first == listenerId;
                }), handlers.end());
        }
    }

    // Emit an event, invoking all registered handlers
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
            //c->close(hdl, websocketpp::close::status::normal, "Successful connection");
        }, & cl, ::_1));
        cl.set_message_handler(bind([&](ws_Client* c, websocketpp::connection_hdl hdl, message_ptr msg) -> void {

            std::string message = msg->get_payload();

            std::cout << "Received a message from websocket" << std::endl;

            MessageEmitter::getInstance().emit("msg", nlohmann::json::parse(msg->get_payload()));

        }, & cl, ::_1, ::_2));

        websocketpp::lib::error_code ec;
        ws_Client::connection_ptr con = cl.get_connection(sharedContextUrl, ec);
        if (ec) {
            console.err(std::format("could not create connection because: {}", ec.message()));
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

inline const void steam_js_context::close_socket() noexcept
{
    //boost::system::error_code error_code;
    //socket.close(boost::beast::websocket::close_code::normal, error_code);
}

__declspec(noinline) const void steam_js_context::reload() noexcept
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

__declspec(noinline) const void steam_js_context::restart() noexcept
{
    establish_socket_handshake([=, this]() {
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

std::string steam_js_context::exec_command(std::string javascript)
{
    std::promise<std::string> promise;

    establish_socket_handshake([&]() {
        c->send(hdl,
            nlohmann::json({ {"id", 1337}, {"method", "Runtime.evaluate"}, {"params", {{"expression", javascript}, {"awaitPromise", true}}} }).dump(),
            websocketpp::frame::opcode::text
        );

        MessageEmitter::getInstance().on("msg", [&](const json& event_msg, int listenerId) {
            try {
                console.log("MessageEmitter.on() received a message");

                console.log(event_msg.dump(4));
                promise.set_value(event_msg["result"].dump());

                c->close(hdl, websocketpp::close::status::normal, "Successful connection");
                MessageEmitter::getInstance().off("msg", listenerId);
            }
            catch (std::future_error& ex) {
                console.err(ex.what());
            }
        });
    });

    return promise.get_future().get();
}