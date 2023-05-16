#include <extern/injector/inject.hpp>
#include <extern/ipc/steamipc.hpp>
#include <include/config.hpp>
#include <include/json.hpp>
#include <extern/steam/cef_manager.hpp>

struct endpoints
{
public:
    std::string steam_resources = "https://steamloopback.host/";
    std::string steam_instances = "http://localhost:8080/json";
    std::string steam_browser = "http://localhost:8080/json/version";
}endpoints;

namespace websocket = boost::beast::websocket;
using namespace boost;

Console console; skin_config config;
using tcp = boost::asio::ip::tcp;

bool steam_client::should_patch_interface(nlohmann::json& patchAddress, const nlohmann::json& currentSteamInstance)
{
    if (patchAddress.contains("patched") && patchAddress["patched"]) {
        return false;
    }

    std::string steam_page_url_header = currentSteamInstance["url"].get<std::string>();

    if (steam_page_url_header.find(patchAddress["url"]) != std::string::npos && steam_page_url_header.find("about:blank") == std::string::npos) {
        console.imp("patching -> " + steam_page_url_header);
    }
    else return false;
}

void steam_client::remote_page_event_handler(const nlohmann::json& page, std::string css_to_evaluate, std::string js_to_evaluate)
{
    boost::network::uri::uri socket_url(page["webSocketDebuggerUrl"].get<std::string>());

    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);

    boost::beast::websocket::stream<tcp::socket> socket(io_context);
    boost::asio::connect(socket.next_layer(), resolver.resolve("localhost", "8080"));

    socket.handshake("localhost", socket_url.path());

    console.imp("steam_client::remote_page_event_handler() socket.handshake success");

    std::function<void()> evaluate_scripting = ([&]() -> void 
    {
        if (!js_to_evaluate.empty())
        {
            steam_interface.evaluate(socket, js_to_evaluate, steam_interface.script_type::javascript);
        }
        if (!css_to_evaluate.empty()) 
        {
            steam_interface.evaluate(socket, css_to_evaluate, steam_interface.script_type::stylesheet);
        }
    });

    std::function<void()> page_set_settings = ([&](void) -> void {
        socket.write(boost::asio::buffer(nlohmann::json({ {"id", 6}, {"method", "Page.setBypassCSP"}, {"params", {{"enabled", true}}} }).dump()));
        socket.write(boost::asio::buffer(nlohmann::json({ {"id", 1}, {"method", "Page.enable"} }).dump()));
        socket.write(boost::asio::buffer(nlohmann::json({ {"id", 5}, {"method", "Page.reload"} }).dump()));
    });
    page_set_settings();

    console.imp("steam_client::remote_page_event_handler() connection to socket was made");

    evaluate_scripting();

    while (true) 
    {
        boost::beast::flat_buffer buffer;
        socket.read(buffer);

        nlohmann::json response = nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()));
        if (response["method"] == "Page.frameResized")
            continue;

        while (true) 
        {
            evaluate_scripting();

            boost::beast::multi_buffer buffer;
            socket.read(buffer);

            if (nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()))
                ["result"]["exceptionDetails"]["exception"]["className"] != "TypeError") {
                break;
            }
        }
    }
}

void steam_client::steam_remote_interface_handler()
{
    console.log("steam_client::steam_remote_interface_handler() starting...");

    nlohmann::json jsonConfig = config.get_skin_config();

    if (jsonConfig["config_fail"])
    {
        console.log("steam_client::steam_remote_interface_handler() loading nothing");
        return;
    }

    while (true)
    {
        nlohmann::json instances = nlohmann::json::parse(steam_interface.discover(endpoints.steam_instances));

        for (nlohmann::basic_json<>& instance : instances)
        {
            for (nlohmann::basic_json<>& address : jsonConfig["patch"])
            {
                if (address["remote"] == false || !should_patch_interface(address, instance))
                    continue;

                try {
                    //hang on this call, and wait for it to be destroyed
                    remote_page_event_handler(
                        instance,
                        address.contains("css") ? address["css"] : "",
                        address.contains("js") ? address["js"] : ""
                    );
                }
                catch (const boost::system::system_error& ex) {
                    if (ex.code() == boost::asio::error::misc_errors::eof) {
                        console.log("instance destroyed, restarting...");
                    }
                }
            }
        }
    }
}

void steam_client::steam_client_interface_handler()
{
    nlohmann::basic_json<> skin_json_config = config.get_skin_config();

    asio::io_context io_context;
    beast::websocket::stream<tcp::socket> socket(io_context);
    asio::ip::tcp::resolver resolver(io_context);

    asio::connect(socket.next_layer(), resolver.resolve("localhost", "8080"));
    socket.handshake("localhost", network::uri::uri(nlohmann::json::parse(steam_interface.discover(endpoints.steam_browser))["webSocketDebuggerUrl"]).path());
    socket.write(boost::asio::buffer(nlohmann::json({ 
        {"id", 1}, 
        {"method", "Target.setDiscoverTargets"}, 
        {"params", {{"discover", true}}} 
    }).dump()));

    std::string sessionId;

    while (true)
    {
        boost::beast::flat_buffer buffer;
        socket.read(buffer);

        nlohmann::json socket_response = nlohmann::json::parse(boost::beast::buffers_to_string(buffer.data()));

        const auto millennium_patch = ([&](std::string title) -> void
        {
            for (nlohmann::basic_json<>& patch : skin_json_config["patch"])
            {
                if (patch["remote"] == true || patch["url"] != title)
                    continue;

                console.imp("patching -> " + title);

                if (patch.contains("css")) steam_interface.evaluate(socket, patch["css"], steam_interface.script_type::stylesheet, socket_response);
                if (patch.contains("js"))  steam_interface.evaluate(socket, patch["js"] , steam_interface.script_type::javascript, socket_response);
            }
        });

        if (socket_response["id"] == steam_cef_manager::response::get_document)
        {
            try 
            {
                std::string attributes = std::string(socket_response["result"]["root"]["children"][1]["attributes"][1]);

                if (attributes.find("friendsui-container") != std::string::npos)
                {
                    millennium_patch("Friends & Chat");
                }
                if (attributes.find("settings_SettingsModalRoot") != std::string::npos)
                {
                    steam_interface.inject_millennium(socket, socket_response);
                }
            }
            catch (const nlohmann::json::exception& e) {
                continue;
            }
        }

        if (socket_response["method"] == "Target.targetCreated" && socket_response.contains("params"))
        {
            socket.write(boost::asio::buffer(nlohmann::json({
                {"id", steam_cef_manager::response::attached_to_target},
                {"method", "Target.attachToTarget"},
                {"params", { {"targetId", socket_response["params"]["targetInfo"]["targetId"]}, {"flatten", true} } }
            }).dump()));
        }

        if (socket_response["method"] == "Target.targetInfoChanged" && socket_response["params"]["targetInfo"]["attached"])
        {
            socket.write(boost::asio::buffer(nlohmann::json({
                {"id", steam_cef_manager::response::received_cef_details},
                {"method", "Runtime.evaluate"},
                {"sessionId", sessionId},
                {"params", { 
                    {"expression", "(() => { return { title: document.title, url: document.location.href }; })()"}, 
                    {"returnByValue", true} 
                }}
            }).dump()));
        }

        if (socket_response["id"] == steam_cef_manager::response::received_cef_details)
        {
            if (socket_response.contains("error"))
                continue;

            millennium_patch(socket_response["result"]["result"]["value"]["title"].get<std::string>());

            socket.write(boost::asio::buffer(
                nlohmann::json({
                    {"id", steam_cef_manager::response::get_document},
                    {"method", "DOM.getDocument"},
                    {"sessionId", sessionId}
                }).dump()
            ));
        }

        if (socket_response["id"] == steam_cef_manager::response::attached_to_target)
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
        console.log("ISteamClient::SteamMillenniumIPC() excep " + std::string(e.what()));
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
        catch (...) { console.err("this->SteamClientInterfaceHandler() unkown"); }

    });
    std::thread Remote([&]() { 
        try {
            this->steam_remote_interface_handler();
        }
        catch (nlohmann::json::exception& ex) { console.err("this->SteamRemoteInterfaceHandler() " + std::string(ex.what())); }
        catch (const boost::system::system_error& ex) { console.err("this->SteamRemoteInterfaceHandler() " + std::string(ex.what())); }
        catch (const std::exception& ex) { console.err("this->SteamRemoteInterfaceHandler() " + std::string(ex.what())); }
        catch (...) { console.err("this->SteamRemoteInterfaceHandler() unkown"); }
    });
    std::thread SteamIPC([&]() { this->steam_to_millennium_ipc(); });

    Remote.join();
    Client.join();
    SteamIPC.join();
}

DWORD WINAPI Initialize(LPVOID lpParam)
{
    config.append_skins_to_settings();
    steam_client ISteamHandler;
    return true;
}