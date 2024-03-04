#pragma once
#include <core/steam/application.hpp>
#include <nlohmann/json.hpp>
#include <utils/cout/logger.hpp>
#include <utils/http/http_client.hpp>

#define MsgBox(text, header, icon) MessageBoxA(FindWindow(nullptr, "Steam"), text, header, icon);
#define OpenURL(url) ShellExecute(0, "open", url, 0, 0, SW_SHOWNORMAL);

extern const char* m_ver;
static const char* repo = "https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest";