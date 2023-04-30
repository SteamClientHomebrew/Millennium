#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include <sstream>

#include <curl/curl.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <nlohmann/json.hpp>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/network/uri.hpp>
#include "logger.hpp"
#include "config.hpp"

Console console;
SkinConfig config;

using tcp = boost::asio::ip::tcp; // Define a TCP type alias for convenience

class SteamCefManager
{
private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::stringstream*)userp)->write((char*)contents, size * nmemb);
        return size * nmemb;
    }
    CURL* curl_handle;
    const char* DEBUGGING_REMOTE_HOST_URL = "http://localhost:8080/json";
public:
    SteamCefManager()
    {
        curl_global_init(CURL_GLOBAL_ALL);
        curl_handle = curl_easy_init();

        if (!curl_handle) {
            std::cerr << "Failed to initialize libcurl" << std::endl;
        }

        curl_easy_setopt(curl_handle, CURLOPT_URL, DEBUGGING_REMOTE_HOST_URL);
    }

    std::string getInstances()
    {
        std::stringstream response_stream;

        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &response_stream);

        CURLcode res = curl_easy_perform(curl_handle);
        if (res != CURLE_OK) {
            std::cout << "websocket isn't open start steam with -cef-enable-debugging manually" << std::endl;
            return "[]";
        }

        return response_stream.str();
    }

    ~SteamCefManager()
    {
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
    }
};

class SteamClientInterfaceHandler
{
public:
    void InjectCSSintoCEFInstance(boost::beast::websocket::stream<tcp::socket>& socket, std::string file)
    {
        std::string webkit_path = std::format("skins/{}/{}", std::string(config.GetConfig()["active-skin"]), file);

        std::string js_code = R"(
            (function() {
                const style = document.createElement('style');
                document.head.append(style);
                style.textContent = `@import url('https://steamloopback.host/)" + webkit_path + R"(');`;
            })()
        )";

        nlohmann::json json = {
            {"id", 1},
            {"method", "Runtime.evaluate"},
            {"params", { 
                {"expression", js_code}, {"userGesture", true}, {"awaitPromise", false} } 
            }
        };
        socket.write(boost::asio::buffer(json.dump(4)));
    }

    void CEFPageEventHandler(const rapidjson::Value& CefBrowserInstance, std::string StyleSheetInject, bool wait_until_inframe)
    {
        try
        {
            console.log("[CEFPageEventHandler] starting...");

            auto start_time = std::chrono::high_resolution_clock::now();

            boost::network::uri::uri webSocketDebuggerUrl(CefBrowserInstance["webSocketDebuggerUrl"].GetString());
            std::string path = webSocketDebuggerUrl.path();

            console.log("[CEFPageEventHandler] set webSocketDebuggerUrl: ws://localhost:8080" + path);

            boost::asio::io_context io_context;

            tcp::resolver resolver(io_context);
            auto const results = resolver.resolve("localhost", "8080");

            console.log("[CEFPageEventHandler] resolving port");

            boost::beast::websocket::stream<tcp::socket> socket(io_context);
            boost::asio::connect(socket.next_layer(), results);
            socket.handshake("localhost", path);

            console.log("[CEFPageEventHandler] handshake made to websocket");

            auto end_time = std::chrono::high_resolution_clock::now();

            console.log("[CEFPageEventHandler] socket connect time: " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count()) + " milliseconds");

            nlohmann::json CefPageEventHandler = {
                {"id", 1},
                {"method", "Page.enable"}
            };

            // collect Page event handler

            console.log("[CEFPageEventHandler] enabling PageEventCallback");

            socket.write(boost::asio::buffer(CefPageEventHandler.dump()));

            // Read incoming WebSocket messages
            for (;;) {

                boost::beast::flat_buffer buffer;
                socket.read(buffer);

                std::string message = boost::beast::buffers_to_string(buffer.data());
                nlohmann::json response = nlohmann::json::parse(message);

                console.log("[CEFPageEventHandler] parsed socket response");
                console.log("[CEFPageEventHandler] request: " + std::string(response.dump(4)));

                std::string response_body;

                if (response["method"] == "Page.frameResized")
                {
                    continue;
                }

                if (wait_until_inframe)
                {
                    console.log("[CEFPageEventHandler] wait_until_inframe = true");

                    if (response["method"] != "Page.frameNavigated" || response["method"] != "Page.loadEventFired")
                    {
                        console.log("[CEFPageEventHandler] starting next loop [wait_until_inframe = true]");
                        continue;
                    }
                }

                console.succ("[CEFPageEventHandler] hooked CEF instance finished loading");

                while (true)
                {
                    console.warn("[CEFPageEventHandler] attempting to inject css");

                    InjectCSSintoCEFInstance(socket, StyleSheetInject);

                    // Receive a response from the WebSocket
                    boost::beast::multi_buffer buffer;
                    socket.read(buffer);
                    response_body = boost::beast::buffers_to_string(buffer.data());

                    nlohmann::json response = nlohmann::json::parse(response_body);


                    if (response["result"]["exceptionDetails"]["exception"]["className"] != "TypeError")
                    {
                        console.succ("[CEFPageEventHandler] injected css successfully!");
                        break;
                    }
                    console.err("[CEFPageEventHandler] failed to inject css! retrying...");
                }
                console.succ("Response received: " + nlohmann::json::parse(response_body).dump(4));
            }
        }
        catch (nlohmann::json::exception& ex)
        {
            console.err("nlohmann::json::exception caught: " + std::string(ex.what()));
        }
        catch (const boost::system::system_error& ex)
        {
            console.err("boost::system::system_error caught: " + std::string(ex.what()));
        }
        catch (const std::exception& ex)
        {
            console.err("std::exception caught: " + std::string(ex.what()));
        }
        catch (...)
        {
            console.err("exception caught. no further details");
        }
    }

    static DWORD WINAPI Initialize(LPVOID lpParam)
    {
        SteamClientInterfaceHandler* ISteamInstance = static_cast<SteamClientInterfaceHandler*>(lpParam);

        SteamCefManager SteamCEFInstance{};
        rapidjson::Document document;

        nlohmann::json json = config.GetSkinConfig();

        static bool patch_complete = false;

        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            std::string instances = SteamCEFInstance.getInstances();

            //console.log("[Initialize] SteamCEFInstance.getInstances():\n" + instances);

            rapidjson::ParseResult _ = document.Parse(instances.c_str());

            bool hasIterableItems = false;

            for (rapidjson::SizeType i = 0; i < document.Size(); i++) {
                if (document[i].IsObject() || document[i].IsArray()) {
                    hasIterableItems = true;
                }
            }
            if (!hasIterableItems) {
                continue;
            }

            const rapidjson::Value& data = document;

            for (rapidjson::Value::ConstValueIterator itr = data.Begin(); itr != data.End(); ++itr) {

                const rapidjson::Value& item = *itr;

                std::string cef_tab_instance = item["url"].GetString();

                for (auto& addr : json["patch"]) {

                    static bool wait_until_inframe = true;

                    //if its already patched, we skip it
                    if (addr.contains("patched") && addr["patched"])
                    {
                        continue;
                    }
                    else patch_complete = false;


                    if (addr["url"] == "steam.ui")
                    {
                        if (item["title"] != "Steam") continue;
                        else wait_until_inframe = false;
                    }
                    else if (addr["url"] == "steam.friends")
                    {
                        if (item["title"] != "Friends List") continue;
                        else wait_until_inframe = false;
                    }

                    //Supernav's
                    else if (addr["url"] == "steam.supernav.profile")
                    {
                        if (item["title"] != "Profile Supernav") continue;
                        else wait_until_inframe = false;
                    }
                    else if (addr["url"] == "steam.supernav.community")
                    {
                        if (item["title"] != "Community Supernav") continue;
                        else wait_until_inframe = false;
                    }
                    else if (addr["url"] == "steam.supernav.library")
                    {
                        if (item["title"] != "Library Supernav") continue;
                        else wait_until_inframe = false;
                    }
                    else if (addr["url"] == "steam.supernav.store")
                    {
                        if (item["title"] != "Store Supernav") continue;
                        else wait_until_inframe = false;
                    }
                    else if (addr["url"] == "steam.menu.account")
                    {
                        if (item["title"] != "Account Menu") continue;
                        else wait_until_inframe = false;
                    }




                    else
                    {
                        if (cef_tab_instance.find(addr["url"]) == std::string::npos)
                        {
                            continue;
                        }
                    }

                    console.log("[Initialize] patching [url:" + std::string(addr["url"]) + "] with " + std::string(addr["css"]));

                    // Remove object that contains "steamloopback.host"
                    auto& patchAddr = json["patch"];
                    for (auto it = patchAddr.begin(); it != patchAddr.end(); ++it) {
                        if ((*it)["url"] == std::string(addr["url"])) {
                            console.log("[Initialize] marking as patched");

                            (*it)["patched"] = true;

                            break;
                        }
                    }

                    console.log("patching work left:\n" + std::string(patchAddr.dump(4)));

                    rapidjson::StringBuffer buffer;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    item.Accept(writer);
                    console.log("[Initialize] item context:\n" + std::string(nlohmann::json::parse(buffer.GetString()).dump(4)));

                    std::thread thread([&]() {
                        console.log("[Initialize] starting ISteamInstance->CEFPageEventHandler with wait_until_inframe = " + std::to_string(wait_until_inframe));
                        ISteamInstance->CEFPageEventHandler(item, addr["css"], wait_until_inframe);
                    });

                    thread.detach();
                }
            }

            if (patch_complete == true)
            {
                console.log("[Initialize] patcher finished! have fun");
                break;
            }
        }
        return 1;
    }
};