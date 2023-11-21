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

#pragma warning(disable: 4005)

//some helper macros

#define MsgBox(text, header, icon) MessageBoxA(FindWindow(nullptr, "Steam"), text, header, icon);

#define OpenURL(url) ShellExecute(0, "open", url, 0, 0, SW_SHOWNORMAL);

static const char* m_ver = "1.0.7";
static const char* repo = "https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest";
