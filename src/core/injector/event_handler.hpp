#pragma once

#include <winsock2.h>

//network helpers and other buffer typings to help with socket management

#include <boost/network/uri.hpp>

#include <nlohmann/json.hpp>

class steam_client
{
public:
    static uint16_t get_ipc_port();
    steam_client();
};

extern nlohmann::basic_json<> skin_json_config;

unsigned long __stdcall Initialize(void*);