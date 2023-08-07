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
#include <string>
#include <core/steam/application.hpp>

//nlohmann's json parser
#include <nlohmann/json.hpp>
#include <utils/cout/logger.hpp>
#include <utils/http/http_client.hpp>

//opening folders and interacting with the file system
#include <TCHAR.h>
#include <pdh.h>

//#pragma comment(lib, "Wininet.lib")

static const char* millennium_version = "1.0.1";