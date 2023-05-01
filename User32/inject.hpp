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

#include <logger.hpp>
#include <config.hpp>

using tcp = boost::asio::ip::tcp;

class SteamClientInterfaceHandler
{
private:
    void InjectCSSintoCEFInstance(boost::beast::websocket::stream<tcp::socket>& socket, std::string file);
    void InjectJSintoCEFInstance(boost::beast::websocket::stream<tcp::socket>& socket, std::string file);
    bool CheckPatchStatus(const rapidjson::Value& data, nlohmann::json& configJson);
    void MarkPatch(nlohmann::json& configJson, nlohmann::json& patchAddress);
    bool CheckSteamCEFInstances(rapidjson::Document& document);
    bool ShouldPatch(nlohmann::json& patchAddress, const rapidjson::Value& currentSteamInstance);

    void CEFPageEventHandler(const rapidjson::Value& CefBrowserInstance, std::string StyleSheetInject, std::string JavaScriptInject);
public:
    SteamClientInterfaceHandler();
};

DWORD WINAPI Initialize(LPVOID lpParam);