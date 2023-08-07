#pragma once

#include <string>
#include <winsock2.h>
#include <wininet.h>
#pragma comment(lib, "Wininet.lib")

//no information is transfered from the client to elsewhere.
//this is specifically for get requests to the steam socket and remotely hosted skins (getting skin info)
class http
{
public:
	static const std::string get(std::string remote_endpoint);
};