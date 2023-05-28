#pragma once
#include <filesystem>
#include <fstream>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include <nlohmann/json.hpp>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/network/uri.hpp>

#include <wininet.h>
#pragma comment(lib, "wininet.lib")

#include <include/logger.hpp>

using tcp = boost::asio::ip::tcp;

class steam_client
{
private:
    bool should_patch_interface(nlohmann::json& patchAddress, const nlohmann::json& currentSteamInstance);

    void remote_page_event_handler(const nlohmann::json& page, std::string css_to_evaluate, std::string js_to_evaluate);

    void steam_remote_interface_handler();
    void steam_to_millennium_ipc();
public:
    steam_client();
};

unsigned long __stdcall Initialize(void*);