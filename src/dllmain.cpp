#define WIN32_LEAN_AND_MEAN 
#define _WINSOCKAPI_   

#include <stdafx.h>
#include <core/injector/event_handler.hpp>

#include <core/steam/application.hpp>
#include <window/core/window.hpp>

#include <utils/thread/thread_handler.hpp>

HMODULE hCurrentModule = nullptr;

namespace Millennium
{
    unsigned long __stdcall checkRemoteDebugging(void)
    {
        constexpr const std::basic_string_view<char> filePath = ".cef-enable-remote-debugging";

        if (!std::ifstream(filePath.data()))
        {
            std::ofstream(filePath.data()).close();
            return false;
        }

        return true;
    }

    unsigned long __stdcall Bootstrap(void* lpParam)
    {
        //try
        //{
        //    nlohmann::basic_json<> response = nlohmann::json::parse(http::get(repo));

        //    if (response["tag_name"] != m_ver)
        //    {
        //        OpenURL("https://millennium.web.app/");
        //        ExitProcess(EXIT_SUCCESS);
        //    }
        //}
        //catch (const http_error& error)
        //{
        //    switch (error.code())
        //    {
        //        case http_error::errors::couldnt_connect: {
        //            console.err("Couldn't make a GET request to GitHub to retrieve update information!");
        //            break;
        //        }
        //        case http_error::errors::not_allowed: {
        //            console.err("Networking disabled. Purged update request.");
        //            break;
        //        }
        //    }
        //}

        if (steam::get().params.has("-dev"))
        {
            if (static_cast<bool>(AllocConsole()))
            {
                void(freopen("CONOUT$", "w", stdout));
                console.consoleAllocated = true;

                std::cout << "Millennium Dev Tools v" << m_ver << std::endl;
                std::cout << "Developed by ShadowMonster (discord: sm74)" << std::endl;
                std::cout << "Need help? Check the docs: https://millennium.gitbook.io/steam-patcher/guides/developers" << std::endl;
                std::cout << "Still need help? ask the discord server: https://discord.gg/MXMWEQKgJF, or DM me if you really cant figure it out" << std::endl;
                std::cout << "-------------------------------------------------------------------------------------\n" << std::endl;
            }
        }

        //discard the value of the thread object in stack memory 
        Initialize(nullptr);
        return true;
    }
}

int __stdcall DllMain(HINSTANCE hinstDLL, DWORD call, void*)
{
    if (call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinstDLL);
        hCurrentModule = hinstDLL;

        if (!Millennium::checkRemoteDebugging())
        {
            MsgBox("Initialization complete! Restart Steam for changed to take effect.", "Millennium", MB_ICONINFORMATION);
            ExitProcess(EXIT_SUCCESS);
        }

        threadContainer::getInstance().addThread(CreateThread(0, 0, Millennium::Bootstrap, 0, 0, 0));
    }

    return true;
}

