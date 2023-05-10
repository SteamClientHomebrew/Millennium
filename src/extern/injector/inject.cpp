#include <extern/injector/inject.hpp>
#include <extern/steamipc.hpp>
#include <include/json.hpp>
#include <extern/cef_manager.hpp>

namespace websocket = boost::beast::websocket;

Console console; SkinConfig config;
using tcp = boost::asio::ip::tcp;

bool javascript_allowed() { return json::read_json_file(std::format("{}/settings.json", config.get_steam_skin_path()))["allow-javascript"].get<bool>(); }

std::string get_css(std::string filename) { return R"((()=>{const h=')" + filename + R"('; if (!document.querySelectorAll(`link[href = "${h}"]`).length) { const l = document.createElement('link'); l.rel = 'stylesheet', l.href = h, document.head.appendChild(l); } })())"; }
std::string get_js(std::string filename) { return "!document.querySelectorAll(`script[src='" + filename + "']`).length && document.head.appendChild(Object.assign(document.createElement('script'), { src: '" + filename + "' }));"; }

void steam_client::evaluate_stylesheet(boost::beast::websocket::stream<tcp::socket>& socket, std::string file, nlohmann::basic_json<> socket_response = NULL)
{
    std::string stylesheet = std::format("skins/{}/{}", std::string(config.GetConfig()["active-skin"]), file);

    if (socket_response == NULL) 
        steam_interface.push_to_socket(socket, (const char*)get_css("https://steamloopback.host/" + stylesheet).c_str());
    else 
        steam_interface.push_to_socket_session(socket, (const char*)get_css("https://steamloopback.host/" + stylesheet).c_str(), socket_response["sessionId"]);
}
void steam_client::evaluate_javascript(boost::beast::websocket::stream<tcp::socket>& socket, std::string file, nlohmann::basic_json<> socket_response = NULL)
{
    if (!javascript_allowed())
        return;

    std::string javascript = std::format("skins/{}/{}", std::string(config.GetConfig()["active-skin"]), file);

    if (socket_response == NULL) 
        steam_interface.push_to_socket(socket, (const char*)get_js("https://steamloopback.host/" + javascript).c_str());
    else 
        steam_interface.push_to_socket_session(socket, (const char*)get_js("https://steamloopback.host/" + javascript).c_str(), socket_response["sessionId"]);
}

bool steam_client::check_interface_patch_status(const rapidjson::Value& data, nlohmann::json& configJson)
{
    if (data.IsArray()) {
        if (data.Size() == 1) {
            const rapidjson::Value& obj = data[0];
            if (obj.IsObject()
                && obj.HasMember("title")
                && obj["title"].IsString()
                && obj["title"].GetString() == std::string("SharedJSContext"))
            {
                for (auto& element : configJson["patch"]) {
                    element["patched"] = false;
                }
                return false;
            }
        }
    }
    return true;
}
void steam_client::mark_page_patch_status(nlohmann::json& configJson, nlohmann::json& patchAddress, bool patched)
{
    nlohmann::basic_json<>::value_type& patchAddr = configJson["patch"];
    for (nlohmann::json::iterator item = patchAddr.begin(); item != patchAddr.end(); ++item) {
        if ((*item)["url"] == std::string(patchAddress["url"])) {
            (*item)["patched"] = patched;
            break;
        }
    }
}
bool steam_client::check_valid_instances(rapidjson::Document& document)
{
    bool hasIterableItems = std::any_of(document.Begin(), document.End(),
        [](const auto& item) { return item.IsObject() || item.IsArray(); });

    if (!hasIterableItems) {
        return false;
    }
    return true;
}
bool steam_client::should_patch_interface(nlohmann::json& patchAddress, const rapidjson::Value& currentSteamInstance)
{
    if (patchAddress.contains("patched") && patchAddress["patched"]) {
        return false;
    }

    std::string steam_page_url_header = currentSteamInstance["url"].GetString();

    if (steam_page_url_header.find(patchAddress["url"]) != std::string::npos && steam_page_url_header.find("about:blank") == std::string::npos) {
        console.imp("patching -> " + steam_page_url_header);
    }
    else return false;
}

void steam_client::remote_page_event_handler(const rapidjson::Value& page, std::string css_to_evaluate, std::string js_to_evaluate)
{
    boost::network::uri::uri socket_url(page["webSocketDebuggerUrl"].GetString());

    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);

    boost::beast::websocket::stream<tcp::socket> socket(io_context);
    boost::asio::connect(socket.next_layer(), resolver.resolve("localhost", "8080"));

    socket.handshake("localhost", socket_url.path());

    console.imp("steam_client::remote_page_event_handler() socket.handshake success");

    std::function<void()> evaluate_scripting = ([&](void) -> void {

        console.imp("steam_client::evaluate_*() evaluating sources");

        if (javascript_allowed())
        {
            if (!js_to_evaluate.empty()) evaluate_javascript(socket, js_to_evaluate);
        }
        else console.log("javascript execution was blocked [reason:user_requested]");

        if (!css_to_evaluate.empty()) evaluate_stylesheet(socket, css_to_evaluate);
    });

    std::function<void()> page_set_settings = ([&](void) -> void {
        socket.write(boost::asio::buffer(nlohmann::json({ {"id", 6}, {"method", "Page.setBypassCSP"}, {"params", {{"enabled", true}}} }).dump()));
        socket.write(boost::asio::buffer(nlohmann::json({ {"id", 1}, {"method", "Page.enable"} }).dump()));
        socket.write(boost::asio::buffer(nlohmann::json({ {"id", 5}, {"method", "Page.reload"} }).dump()));
    });
    page_set_settings();

    console.imp("steam_client::remote_page_event_handler() connection to socket was made");

    //if the socket took too long to connect, that means no events will be logged, so just force inject on launch
    evaluate_scripting();

    while (true) {
        boost::beast::flat_buffer buffer;
        socket.read(buffer);

        nlohmann::json response = nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()));

        if (response["method"] == "Page.frameResized") continue;

        while (true) {
            evaluate_scripting();

            boost::beast::multi_buffer buffer;
            socket.read(buffer);

            if (nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()))
                ["result"]["exceptionDetails"]["exception"]["className"] != "TypeError")
            {
                break;
            }
        }
    }
}

void steam_client::steam_remote_interface_handler()
{
    nlohmann::json jsonConfig = config.get_skin_config();
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        rapidjson::Document document;

        document.Parse(steam_interface.discover("http://localhost:8080/json").c_str());
        if (!check_valid_instances(document)) continue;
        const rapidjson::Value& data = document;
        if (!check_interface_patch_status(data, jsonConfig)) jsonConfig = config.get_skin_config();

        for (rapidjson::Value::ConstValueIterator itr = data.Begin(); itr != data.End(); ++itr) 
        {
            const rapidjson::Value& current_instance = *itr;
            for (nlohmann::basic_json<>& address : jsonConfig["patch"]) 
            {
                if (address["remote"] == false) continue;
                if (!should_patch_interface(address, current_instance))  continue;

                mark_page_patch_status(jsonConfig, address, true);
                std::thread handle_event([&]() {
                    try {
                        remote_page_event_handler(
                            current_instance,
                            address.find("css") != address.end() ? address["css"] : "",
                            address.find("js") != address.end() ? address["js"] : ""
                        );
                    }
                    catch (const boost::system::system_error& ex) {
                        if (ex.code() == boost::asio::error::misc_errors::eof) {
                            mark_page_patch_status(jsonConfig, address, false);
                        }
                    }
                });
                handle_event.detach();
            }
        }
    }
}

void check_if_valid_settings(std::string settings_path)
{
    std::filesystem::path directory_path(settings_path);

    if (!std::filesystem::exists(directory_path)) {
        std::filesystem::create_directory(directory_path);
        std::ofstream settings_file(std::format("{}/settings.json", settings_path));
    }
}

void append_skins_to_settings()
{
    SkinConfig MillenniumConfig;

    std::string steam_skin_path = MillenniumConfig.get_steam_skin_path();
    std::string setting_json_path = std::format("{}/settings.json", MillenniumConfig.get_steam_skin_path());

    check_if_valid_settings(steam_skin_path);
    nlohmann::json skins, data;

    for (const auto& entry : std::filesystem::directory_iterator(steam_skin_path)) {
        if (entry.is_directory()) {
            skins.push_back({ {"name", entry.path().filename().string()} });
        }
    }
    skins.push_back({ {"name", "default"} });

    try { data = json::read_json_file(setting_json_path); }
    catch (nlohmann::detail::parse_error& ex) {
        std::ofstream file(setting_json_path, std::ofstream::out | std::ofstream::trunc);
        file.close();
    }

    //edit contents of the settings.json
    data["skins"] = skins;
    if (!data.contains("active-skin")) {
        data["active-skin"] = "default";
    }
    if (!data.contains("allow-javascript")) {
        data["allow-javascript"] = true;
    }
    if (!data.contains("enable-console")) {
        data["enable-console"] = false;
    }

    if (data["enable-console"])
    {
        AllocConsole();
        FILE* fp = freopen("CONOUT$", "w", stdout);
    }

    json::write_json_file(setting_json_path, data);
}

void millennium_settings_page(boost::beast::websocket::stream<tcp::socket>& socket, nlohmann::basic_json<>& socket_response)
{
    append_skins_to_settings();

    std::string javascript = "https://raw.githack.com/ShadowMonster99/millennium-steam-patcher/main/settings_modal/index.js";
    //std::string javascript = "https://steamloopback.host/skins/index.js";
    std::string js_code = "!document.querySelectorAll(`script[src='" + javascript + "']`).length && document.head.appendChild(Object.assign(document.createElement('script'), { src: '" + javascript + "' }));";

    socket.write(boost::asio::buffer(nlohmann::json({
        {"id", steam_socket::response::script_inject_evaluate},
        {"method", "Runtime.evaluate"},
        {"sessionId", socket_response["sessionId"]},
        {"params", {{"expression", js_code}}}
    }).dump()));
}

void steam_client::steam_client_interface_handler()
{
    const char* browser_socket_url = "http://localhost:8080/json/version";

    std::basic_string<char, std::char_traits<char>, std::allocator<char>> current_skin = std::string(config.GetConfig()["active-skin"]);
    nlohmann::basic_json<> skin_json_config = config.get_skin_config();

    boost::network::uri::uri socket_url(nlohmann::json::parse(steam_interface.discover(browser_socket_url).c_str())["webSocketDebuggerUrl"]);
    boost::asio::io_context io_context;

    boost::beast::websocket::stream<tcp::socket> socket(io_context);
    boost::asio::ip::tcp::resolver resolver(io_context);

    boost::asio::connect(socket.next_layer(), resolver.resolve("localhost", "8080"));

    socket.handshake("localhost", socket_url.path());
    socket.write(boost::asio::buffer(nlohmann::json({ {"id", 1}, {"method", "Target.setDiscoverTargets"}, {"params", {{"discover", true}}} }).dump()));

    std::basic_string<char, std::char_traits<char>, std::allocator<char>> sessionId = {  };

    while (true)
    {
        boost::beast::flat_buffer buffer;
        socket.read(buffer);

        nlohmann::json socket_response = nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()));

        if (socket_response["method"] == "Target.targetCreated" && socket_response.contains("params"))
        {
            socket.write(boost::asio::buffer(nlohmann::json({
                {"id", steam_socket::response::attached_to_target},
                {"method", "Target.attachToTarget"},
                {"params", { {"targetId", socket_response["params"]["targetInfo"]["targetId"]}, {"flatten", true} } }
            }).dump()));
        }

        if (socket_response["method"] == "Target.targetInfoChanged" && socket_response["params"]["targetInfo"]["attached"])
        {
            socket.write(boost::asio::buffer(nlohmann::json({
                {"id", steam_socket::response::received_cef_details},
                {"method", "Runtime.evaluate"},
                {"sessionId", sessionId},
                {"params", { {"expression", "(() => { return { title: document.title, url: document.location.href }; })()"}, {"returnByValue", true} }}
            }).dump()));
        }

        if (socket_response["id"] == steam_socket::response::received_cef_details)
        {
            if (socket_response.contains("error")) {
                continue;
            }

            std::string title = socket_response["result"]["result"]["value"]["title"].get<std::string>();

            if (title == "Steam Settings") {
                millennium_settings_page(socket, socket_response);
            }

            if (!skin_json_config.contains("patch")) {
                continue;
            }

            for (nlohmann::basic_json<>& patchAddress : skin_json_config["patch"]) 
            {
                if (patchAddress["remote"] == true || title != patchAddress["url"]) {
                    continue;
                }

                console.imp("patching -> " + title);

                if (patchAddress.contains("css")) { evaluate_stylesheet(socket, patchAddress["css"], socket_response); }
                if (patchAddress.contains("js"))  { evaluate_javascript(socket, patchAddress["js"], socket_response); }
            }
        }

        if (socket_response["id"] == steam_socket::response::attached_to_target)
        {
            sessionId = socket_response["result"]["sessionId"];
        }
    }
}

void steam_client::steam_to_millennium_ipc()
{
    console.log("ISteamClient::SteamMillenniumIPC() bootstrap");
    try {
        boost::asio::io_context ioc;
        millennium_ipc_listener listener(ioc);
        ioc.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

steam_client::steam_client()
{
    std::thread Client([&]() { 
        try {
            this->steam_client_interface_handler();
        }
        catch (nlohmann::json::exception& ex) { console.err("this->SteamClientInterfaceHandler() " + std::string(ex.what())); }
        catch (const boost::system::system_error& ex) { console.err("this->SteamClientInterfaceHandler() " + std::string(ex.what())); }
        catch (const std::exception& ex) { console.err("this->SteamClientInterfaceHandler() " + std::string(ex.what())); }

    });
    std::thread Remote([&]() { 
        try {
            this->steam_remote_interface_handler();
        }
        catch (nlohmann::json::exception& ex) { console.err("this->SteamRemoteInterfaceHandler() " + std::string(ex.what())); }
        catch (const boost::system::system_error& ex) { console.err("this->SteamRemoteInterfaceHandler() " + std::string(ex.what())); }
        catch (const std::exception& ex) { console.err("this->SteamRemoteInterfaceHandler() " + std::string(ex.what())); }
    });
    std::thread SteamIPC([&]() { this->steam_to_millennium_ipc(); });

    Remote.join(); Client.join(); SteamIPC.join();
}

DWORD WINAPI Initialize(LPVOID lpParam)
{
    append_skins_to_settings();
    steam_client ISteamHandler;
    return true;
}