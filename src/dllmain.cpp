#define WIN32_LEAN_AND_MEAN 
#define _WINSOCKAPI_   

#include <stdafx.h>
#include <core/injector/event_handler.hpp>

#include <core/steam/application.hpp>
#include <window/core/window.hpp>

#include <utils/thread/thread_handler.hpp>
#include <Psapi.h>
#include <pdh.h>

#include <format>
#include <core/injector/startup/bootstrap.hpp>

#pragma comment(lib, "pdh.lib")
#include <metrics.hpp>

#include <utils/updater/update.hpp>
#include <utils/clr/platform.hpp>

#include <core/steam/window/manager.hpp>

HMODULE hCurrentModule = nullptr;
using std::string;

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

    unsigned long __stdcall getConsoleHeader(void* lpParam) {

        while (true) {

            std::unique_ptr<Metrics> metrics = std::make_unique<Metrics>();

            //double cpu = metrics->getCpuUsage();
            double ram = metrics->getMemoryUsage();

            SetConsoleTitle(std::format("Millennium.Patcher-v{} [Threads: {}, Private-Bytes {} MB]",
                m_ver,
                threadContainer::getInstance().runningThreads.size(),
                std::round(ram * 100.0) / 100.0
            ).c_str());

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    unsigned long __stdcall Bootstrap(void* lpParam)
    {
        if (steam::get().params.has("-dev") && static_cast<bool>(AllocConsole()))
        {
            void(freopen("CONOUT$", "w", stdout));
            console.consoleAllocated = true;
        }

        threadContainer::getInstance().hookAllThreads();
        {
            updater::check_for_updates();

            std::cout << " Millennium Dev Tools v" << m_ver << std::endl;
            std::cout << " Developed by ShadowMonster (discord: sm74)" << std::endl;
            std::cout << " Need help? Check the docs: https://millennium.gitbook.io/steam-patcher/guides/developers" << std::endl;
            std::cout << " Still need help? Join the discord server: https://millennium.web.app/discord" << std::endl;
            std::cout << "-------------------------------------------------------------------------------------\n" << std::endl;

            threadContainer::getInstance().addThread(CreateThread(0, 0, StartWinHookAsync, 0, 0, 0));
            threadContainer::getInstance().addThread(CreateThread(0, 0, getConsoleHeader, 0, 0, 0));
            threadContainer::getInstance().addThread(CreateThread(0, 0, Initialize, 0, 0, 0));

            // synchronously run theme updater 
            queryThemeUpdates(nullptr);
        }
        threadContainer::getInstance().unhookAllThreads();

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

