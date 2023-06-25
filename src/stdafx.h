/// <summary>
/// precompiled headers, used to speed up the compiler because its so slow
/// </summary>
#pragma once

#include <iostream>
#include <string>
#include <regex>
#include <filesystem>
#include <fstream>
#include <set>

//websocket includes used to listen on sockets
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

//nlohmann's json parser
#include <nlohmann/json.hpp>
#include <include/logger.hpp>

//network helpers and other buffer typings to help with socket management
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/network/uri.hpp>

//making GET requests
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

//opening folders and interacting with the file system
#include <shellapi.h>