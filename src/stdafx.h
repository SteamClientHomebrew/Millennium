#pragma once
#include <core/steam/application.hpp>
#include <nlohmann/json.hpp>
#include <utils/cout/logger.hpp>
#include <utils/http/http_client.hpp>
#include <fmt/core.h>
#include <imgui.h>


void OpenURL(std::string url);
void MsgBox(std::string header, std::function<void(bool* open)> cb);

#ifdef _WIN32
//#define MsgBox(text, header, icon) MessageBoxA(FindWindow(nullptr, "Steam"), text, header, icon);
//#define OpenURL(url) ShellExecute(0, "open", url, 0, 0, SW_SHOWNORMAL);
#elif __linux__

#define MB_ICONINFORMATION 1
#define MB_ICONERROR 2

//#define MsgBox(text, header, icon) system("zenity --info --text=\"" text "\" --title=\"" header "\" &");
//#define OpenURL(url) system("xdg-open " url " &");
#endif

extern const char* m_ver;
static const char* repo = "https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest";