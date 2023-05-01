#include <inject.hpp>

Console console;
SkinConfig config;

using tcp = boost::asio::ip::tcp;

struct SteamCefManager
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
    void SteamSocketSend(boost::beast::websocket::stream<tcp::socket>& socket, const char* js_code)
    {
        nlohmann::json json = {
            {"id", 1},
            {"method", "Runtime.evaluate"},
            {"params", {
                {"expression", js_code}, {"userGesture", true}, {"awaitPromise", false} }
            }
        };
        socket.write(boost::asio::buffer(json.dump(4)));
    }
    ~SteamCefManager()
    {
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
    }
} SteamCEFInstance;

void SteamClientInterfaceHandler::InjectCSSintoCEFInstance(boost::beast::websocket::stream<tcp::socket>& socket, std::string file)
{
    std::string stylesheet = std::format("skins/{}/{}", std::string(config.GetConfig()["active-skin"]), file);

    std::string js_code = R"(
        (function() {
            const style = document.createElement('style');
            document.head.append(style);
            style.textContent = `@import url('https://steamloopback.host/)" + stylesheet + R"(');`;
        })()
    )";

    SteamCEFInstance.SteamSocketSend(socket, (const char*)js_code.c_str());
}
void SteamClientInterfaceHandler::InjectJSintoCEFInstance(boost::beast::websocket::stream<tcp::socket>& socket, std::string file)
{
    std::string javascript = std::format("skins/{}/{}", std::string(config.GetConfig()["active-skin"]), file);

    std::string js_code = R"(
        (function() {
            const script = document.createElement('script');
            script.src = 'https://steamloopback.host/)" + javascript + R"(';
            document.head.appendChild(script);
        })()
    )";

    SteamCEFInstance.SteamSocketSend(socket, (const char*)js_code.c_str());
}
bool SteamClientInterfaceHandler::CheckPatchStatus(const rapidjson::Value& data, nlohmann::json& configJson)
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
void SteamClientInterfaceHandler::MarkPatch(nlohmann::json& configJson, nlohmann::json& patchAddress)
{
    nlohmann::basic_json<>::value_type& patchAddr = configJson["patch"];
    for (nlohmann::json::iterator item = patchAddr.begin(); item != patchAddr.end(); ++item) {
        if ((*item)["url"] == std::string(patchAddress["url"])) {
            (*item)["patched"] = true;
            break;
        }
    }
    console.log("patching work left:\n" + std::string(patchAddr.dump(4)));
}
bool SteamClientInterfaceHandler::CheckSteamCEFInstances(rapidjson::Document& document)
{
    bool hasIterableItems = std::any_of(document.Begin(), document.End(),
        [](const auto& item) { return item.IsObject() || item.IsArray(); });

    if (!hasIterableItems) {
        return false;
    }
    return true;
}
bool SteamClientInterfaceHandler::ShouldPatch(nlohmann::json& patchAddress, const rapidjson::Value& currentSteamInstance)
{
    if (patchAddress.contains("patched") && patchAddress["patched"]) {
        return false;
    }

    std::string CefTitle = currentSteamInstance["title"].GetString();
    std::string CefUrl = currentSteamInstance["url"].GetString();

    if (CefTitle == patchAddress["url"]) {
        console.imp("Patching by title: -> " + CefTitle);
    }
    else if (CefUrl.find(patchAddress["url"]) != std::string::npos && CefUrl.find("about:blank") == std::string::npos) {
        console.imp("Patching by URL: -> " + CefUrl);
    }
    else {
        return false;
    }
}

void SteamClientInterfaceHandler::CEFPageEventHandler(const rapidjson::Value& CefBrowserInstance, std::string StyleSheetInject, std::string JavaScriptInject)
{
    try {
        boost::network::uri::uri webSocketDebuggerUrl(CefBrowserInstance["webSocketDebuggerUrl"].GetString());
        std::string path = webSocketDebuggerUrl.path();

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        auto const results = resolver.resolve("localhost", "8080");

        boost::beast::websocket::stream<tcp::socket> socket(io_context);
        boost::asio::connect(socket.next_layer(), results);
        socket.handshake("localhost", path);

        nlohmann::json CefPageEventHandler = {
            {"id", 1},
            {"method", "Page.enable"}
        };

        socket.write(boost::asio::buffer(CefPageEventHandler.dump()));

        for (;;) {
            boost::beast::flat_buffer buffer;
            socket.read(buffer);

            std::string message = boost::beast::buffers_to_string(buffer.data());
            nlohmann::json response = nlohmann::json::parse(message);

            std::string response_body;
            if (response["method"] == "Page.frameResized") continue;

            while (true) {
                if (StyleSheetInject != "NULL") InjectCSSintoCEFInstance(socket, StyleSheetInject);
                if (JavaScriptInject != "NULL") InjectJSintoCEFInstance(socket, JavaScriptInject);

                boost::beast::multi_buffer buffer;
                socket.read(buffer);
                response_body = boost::beast::buffers_to_string(buffer.data());

                nlohmann::json response = nlohmann::json::parse(response_body);

                if (response["result"]["exceptionDetails"]["exception"]["className"] != "TypeError") break;
            }
        }
    }
    catch (nlohmann::json::exception& ex) { console.err("nlohmann::json::exception caught: " + std::string(ex.what())); }
    catch (const boost::system::system_error& ex) { console.err("boost::system::system_error caught: " + std::string(ex.what())); }
    catch (const std::exception& ex) { console.err("std::exception caught: " + std::string(ex.what())); }
}
SteamClientInterfaceHandler::SteamClientInterfaceHandler()
{
    rapidjson::Document document;
    nlohmann::json jsonConfig = config.GetSkinConfig();

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        rapidjson::ParseResult _ = document.Parse(SteamCEFInstance.getInstances().c_str());

        if (!CheckSteamCEFInstances(document)) continue;

        const rapidjson::Value& data = document;

        if (!CheckPatchStatus(data, jsonConfig)) jsonConfig = config.GetSkinConfig();

        for (rapidjson::Value::ConstValueIterator itr = data.Begin(); itr != data.End(); ++itr) {

            const rapidjson::Value& currentSteamInstance = *itr;

            for (nlohmann::basic_json<>& patchAddress : jsonConfig["patch"]) {

                if (!ShouldPatch(patchAddress, currentSteamInstance)) continue;

                MarkPatch(jsonConfig, patchAddress);

                std::thread thread([&]() {
                    CEFPageEventHandler(
                        currentSteamInstance,
                        (patchAddress.find("css") != patchAddress.end() ? patchAddress["css"] : "NULL"),
                        (patchAddress.find("js") != patchAddress.end() ? patchAddress["js"] : "NULL")
                    );
                    });
                thread.detach();
            }
        }
    }
}

DWORD WINAPI Initialize(LPVOID lpParam)
{
    SteamClientInterfaceHandler ISteamHandler;
    return true;
}