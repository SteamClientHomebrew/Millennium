#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <nlohmann/json.hpp>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/network/uri.hpp>

#include <wininet.h>
#pragma comment(lib, "wininet.lib")

#include <include/logger.hpp>
#include <include/config.hpp>


using tcp = boost::asio::ip::tcp;

class steam_client
{
private:
    void evaluate_stylesheet(boost::beast::websocket::stream<tcp::socket>& socket, std::string file, nlohmann::basic_json<> socket_response);
    void evaluate_javascript(boost::beast::websocket::stream<tcp::socket>& socket, std::string file, nlohmann::basic_json<> socket_response);
    bool check_interface_patch_status(const rapidjson::Value& data, nlohmann::json& configJson);
    void mark_page_patch_status(nlohmann::json& configJson, nlohmann::json& patchAddress, bool patched);
    bool check_valid_instances(rapidjson::Document& document);
    bool should_patch_interface(nlohmann::json& patchAddress, const rapidjson::Value& currentSteamInstance);

    void remote_page_event_handler(const rapidjson::Value& CefBrowserInstance, std::string StyleSheetInject, std::string JavaScriptInject);

    void steam_remote_interface_handler();
    void steam_client_interface_handler();
    void steam_to_millennium_ipc();
public:
    steam_client();
};

DWORD WINAPI Initialize(LPVOID lpParam);