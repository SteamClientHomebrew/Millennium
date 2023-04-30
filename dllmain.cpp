#define WIN32_LEAN_AND_MEAN 

#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <Lmcons.h>
#include <string>
#include <sstream>
#include <fstream>
#include <winsock2.h>
#include <chrono>

#include "webkit.hpp"

DWORD __stdcall MainThread(void*)
{
    if (std::string(GetCommandLine()).find("-cef-enable-debugging") == std::string::npos)
    {
        MessageBoxA(0, "start steam with -cef-enable-debugging in order to enable webview skinning", "Millennium", MB_ICONINFORMATION);
    }

    if (config.GetConfig()["show-console"])
    {
        AllocConsole();
        FILE* fp = freopen("CONOUT$", "w", stdout);

        console.log(GetCommandLine());
        console.warn("show-console is set to true");
    }

    CreateThread(0, 0, SteamClientInterfaceHandler::Initialize, 0, 0, 0);

    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  call, LPVOID lpReserved)
{
    if (call == DLL_PROCESS_ATTACH) CreateThread(0, 0, MainThread, 0, 0, 0);

    return true;
}

