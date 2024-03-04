#pragma once
#include <winsock2.h>
#include <nlohmann/json.hpp>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::client<websocketpp::config::asio_client> ws_Client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using json = nlohmann::json;
namespace fs = std::filesystem;

typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

class steam_client
{
public:
    static uint16_t get_ipc_port();
    steam_client();
};

extern nlohmann::basic_json<> skin_json_config;

/// <summary>/// /// </summary>/// <param name=""></param>/// <returns></returns>
unsigned long __stdcall Initialize(void*);