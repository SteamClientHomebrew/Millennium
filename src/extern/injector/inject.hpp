#pragma once
using tcp = boost::asio::ip::tcp;

class steam_client
{
public:
    static uint16_t get_ipc_port();
    steam_client();
};

unsigned long __stdcall Initialize(void*);