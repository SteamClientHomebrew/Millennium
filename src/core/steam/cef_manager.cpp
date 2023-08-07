#define _WINSOCKAPI_   

#include <core/injector/event_handler.hpp>

#include <stdafx.h>
#include <core/steam/cef_manager.hpp>
#include <utils/config/config.hpp>


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
    return std::format("!document.querySelectorAll(`link[href='{}']`).length && document.head.appendChild(Object.assign(document.createElement('link'), {{ rel: 'stylesheet', href: '{}' }}));", filename, filename);
}

const std::basic_string<char, std::char_traits<char>, std::allocator<char>> cef_dom::javascript::add(std::string filename) noexcept {
    return std::format("!document.querySelectorAll(`script[src='{}'][type='module']`).length && document.head.appendChild(Object.assign(document.createElement('script'), {{ src: '{}', type: 'module' }}));", filename, filename);
}

cef_dom& cef_dom::get() {
    static cef_dom instance;
    return instance;
}

void steam_cef_manager::push_to_socket(boost::beast::websocket::stream<tcp::socket>& socket, std::string raw_script, std::string sessionId) noexcept
{
    try {
        //include millennium functions
        const std::string millennium_functions = R"(
                class millennium{static sharedjscontext_exec(c){const t=new WebSocket(atob('d3M6Ly9sb2NhbGhvc3Q6MzI0Mg==')),r=()=>t.send(JSON.stringify({type:6,content:c}));return new Promise((e,n)=>{t.onopen=r,t.onmessage=t=>(e(JSON.parse(t.data)),t.target.close()),t.onerror=n,t.onclose=()=>n(new Error)})}}window.millennium=millennium;
            )";

        std::function<void(const std::string&)> runtime_evaluate = [&](const std::string& script) -> void {
            nlohmann::json evaluate_script = {
                {"id", response::script_inject_evaluate},
                {"method", "Runtime.evaluate"},
                {"params", {{"expression", script}}}
            };

            if (!sessionId.empty()) evaluate_script["sessionId"] = sessionId;
            socket.write(boost::asio::buffer(evaluate_script.dump(4)));
        };

        //inject the script that the user wants
        runtime_evaluate(raw_script);
        //inject millennium class functions to help users interact with SharedJSContext (it probably sucks)
        runtime_evaluate(millennium_functions);
    }
    catch (const boost::system::system_error& ex) {

        if (ex.code().value() == boost::asio::error::operation_aborted || ex.code().value() == boost::asio::error::eof)
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

__declspec(noinline) void __fastcall steam_cef_manager::inject_millennium(boost::beast::websocket::stream<tcp::socket>& socket, nlohmann::basic_json<>& socket_response) noexcept
{
    std::string javascript = "https://shadowmonster99.github.io/millennium-steam-patcher/root/dependencies/index.js";
    //std::string javascript = std::format("{}/skins/index.js", endpoints.steam_resources.string());

    //deprecated, now using imgui interface
    //steam_interface.push_to_socket(
    //    socket,
    //    //uses js vm to run the javascript code from a url as its too long to send over socket, steamloopbackhost is allowed
    //    cef_dom::get().runtime_handler.evaluate(javascript),
    //    socket_response["sessionId"].get<std::string>()
    //);
}

const void steam_cef_manager::calculate_endpoint(std::string& endpoint_unparsed)
{
    //calculate endpoint type, if remote url retain info, if local add loopbackhost
    if (endpoint_unparsed.compare(0, 4, "http") != 0) {

        //remote skin use the remote skins as a current directory from github
        if (millennium_remote.is_remote) endpoint_unparsed = std::format("{}/{}", millennium_remote.host, endpoint_unparsed);
        //just use the default from the skin path
        else endpoint_unparsed = std::format("{}/skins/{}/{}", uri.steam_resources.string(), std::string(registry::get_registry("active-skin")), endpoint_unparsed);
    }
}

const void steam_cef_manager::evaluate(boost::beast::websocket::stream<tcp::socket>& socket, std::string file, script_type type, nlohmann::basic_json<> socket_response)
{
    if (type == script_type::javascript && (registry::get_registry("allow-javascript") == "false")) {
        console.err(std::format("the requested operation [{}()] was cancelled. reason: script_type::javascript is prohibited", __func__));
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
        steam_interface.push_to_socket(socket, cef_dom::get().javascript_handler.add(file).data(), sessionId.value_or(std::string()));
        break;
    case script_type::stylesheet:
        steam_interface.push_to_socket(socket, cef_dom::get().stylesheet_handler.add(file).data(), sessionId.value_or(std::string()));
        break;
    default:
        throw std::runtime_error("[debug] invalid target scripting type");
    }
}

const void steam_js_context::establish_socket_handshake(std::function<void()> callback)
{
    console.log("attempting to create a connection to SteamJSContext");

    if (callback == nullptr)
        throw std::runtime_error("callback function was nullptr");

    boost::asio::connect(socket.next_layer(), resolver.resolve(uri.debugger.host(), uri.debugger.port()));

    nlohmann::basic_json<> instances = nlohmann::json::parse(steam_interface.discover(uri.debugger.string() + "/json"));

    //instances contains all pages, filter the correct instance
    auto itr_impl = std::find_if(instances.begin(), instances.end(), [](const nlohmann::json& instance) {
        return instance["title"].get<std::string>() == "SharedJSContext";
    });
    if (itr_impl == instances.end()) { throw std::runtime_error("SharedJSContext wasn't instantiated"); }

    boost::network::uri::uri socket_url((*itr_impl)["webSocketDebuggerUrl"].get<std::string>());
    socket.handshake(uri.debugger.host(), socket_url.path());

    //implicit type converge -> interrupt_handler
    static_cast<void>(callback());
    this->close_socket();
}

steam_js_context::steam_js_context() : socket(io_context), resolver(io_context) { }

inline const void steam_js_context::close_socket() noexcept
{
    boost::system::error_code error_code;
    socket.close(boost::beast::websocket::close_code::normal, error_code);
}

__declspec(noinline) const void steam_js_context::reload() noexcept
{
    establish_socket_handshake([this]() {
        //https://chromedevtools.github.io/devtools-protocol/tot/Page/#method-reload
        socket.write(boost::asio::buffer(nlohmann::json({ {"id", 89}, {"method", "Page.reload"} }).dump()));
    });
}

__declspec(noinline) const void steam_js_context::restart() noexcept
{
    establish_socket_handshake([=, this]() {
        //not used anymore, but here in case
        socket.write(boost::asio::buffer(nlohmann::json({
            { "id", 8987 },
            { "method", "Runtime.evaluate" },
            { "params", {
                { "expression", "setTimeout(() => {SteamClient.User.StartRestart(true)}, 1000)" }
            }}
        }).dump()));
    });
}

std::string steam_js_context::exec_command(std::string javascript)
{
    std::promise<std::string> promise;

    establish_socket_handshake([&]() {
        socket.write(boost::asio::buffer(nlohmann::json({ {"id", 1337}, {"method", "Runtime.evaluate"}, {"params", {{"expression", javascript}, {"awaitPromise", true}}} }).dump()));

        boost::beast::multi_buffer buffer; socket.read(buffer);
        promise.set_value(nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()))["result"].dump());
    });

    return promise.get_future().get();
}