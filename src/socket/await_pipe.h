#include <utilities/http.h>
#include <thread>
#include <nlohmann/json.hpp>

const std::string GetSteamBrowserContext()
{
    nlohmann::basic_json<> instance = nlohmann::json::parse(GetRequest("http://localhost:8080/json/version"));
    return instance["webSocketDebuggerUrl"];
}

const std::string GetSharedJsContext() 
{
    std::string sharedJsContextSocket;

    while (sharedJsContextSocket.empty())
    {
        nlohmann::basic_json<> windowInstances = nlohmann::json::parse(GetRequest("http://localhost:8080/json"));

        for (const auto& instance : windowInstances) 
        {
            if (instance["title"] == "SharedJSContext") 
            {
                sharedJsContextSocket = instance["webSocketDebuggerUrl"].get<std::string>();
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return sharedJsContextSocket;
}

struct ConnectSocketProps
{
    std::string commonName;
    std::function<std::string()> fetchSocketUrl;

    std::function<void(
        websocketpp::client<websocketpp::config::asio_client>*,
        websocketpp::connection_hdl)> onConnect;

    std::function<void(
        websocketpp::client<websocketpp::config::asio_client>*,
        websocketpp::connection_hdl,
        std::shared_ptr<websocketpp::config::core_client::message_type>)> onMessage;

    PluginLoader* pluginLoader;
};

void ConnectSocket(ConnectSocketProps socketProps)
{
    const auto [commonName, fetchSocketUrl, onConnect, onMessage, pluginLoader] = socketProps;

    while (true)
    {
        const std::string socketUrl = fetchSocketUrl();

        try
        {
            websocketpp::client<websocketpp::config::asio_client> socketClient;
            socketClient.set_access_channels(websocketpp::log::alevel::none);
            socketClient.init_asio();
            socketClient.set_open_handler(bind(onConnect, &socketClient, std::placeholders::_1));
            socketClient.set_message_handler(bind(onMessage, &socketClient, std::placeholders::_1, std::placeholders::_2));

            websocketpp::lib::error_code ec;
            websocketpp::client<websocketpp::config::asio_client>::connection_ptr con = socketClient.get_connection(socketUrl, ec);

            if (ec) {
                throw websocketpp::exception(ec.message());
            }
            socketClient.connect(con);
            socketClient.run();
        }
        catch (websocketpp::exception& ex)
        {
            Logger.Error("webSocket exception thrown -> {}", ex.what());
        }

        Logger.Error("{} tunnel collapsed, attempting to reopen...", commonName);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}