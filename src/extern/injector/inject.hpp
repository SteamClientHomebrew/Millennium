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
public:
    steam_client();
};

uint16_t get_proxy_port();

unsigned long __stdcall Initialize(void*);