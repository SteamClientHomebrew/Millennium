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
        if (steam::get().params.has("-dev"))
        {
            if (static_cast<bool>(AllocConsole()))
            {
                void(freopen("CONOUT$", "w", stdout));
                console.consoleAllocated = true;
            }
        }

        try
        {
            nlohmann::basic_json<> response = nlohmann::json::parse(http::get(repo));

            if (response.contains("message")) {
                throw http_error(http_error::errors::not_allowed);
            }

            //updater::start_update();
            if (response["tag_name"] != m_ver)
            {
                updater::start_update();
            }
        }
        catch (const http_error& error)
        {
            switch (error.code())
            {
                case http_error::errors::couldnt_connect: {
                    console.err("Couldn't make a GET request to GitHub to retrieve update information!");
                    break;
                }
                case http_error::errors::not_allowed: {
                    console.err("Networking disabled. Purged update request.");
                    break;
                }
            }
        }

        std::cout << " Millennium Dev Tools v" << m_ver << std::endl;
        std::cout << " Developed by ShadowMonster (discord: sm74)" << std::endl;
        std::cout << " Need help? Check the docs: https://millennium.gitbook.io/steam-patcher/guides/developers" << std::endl;
        std::cout << " Still need help? Join the discord server: https://discord.gg/MXMWEQKgJF" << std::endl;
        std::cout << "-------------------------------------------------------------------------------------\n" << std::endl;



        //clr_interop::clr_base::instance().start_update(nlohmann::json({
        //    {"owner", "SaiyajinK"},
        //    {"repo", "Minimal-Dark-for-Steam"}
        //}).dump(4));

        //clr_interop::clr_base::instance().start_update(nlohmann::json({
        //    {"owner", "ShadowMonster99"},
        //    {"repo", "Simply-Dark"}
        //}).dump(4));


        //clr_interop::clr_base::instance().start_update(nlohmann::json({
        //    {"owner", "RoseTheFlower"},
        //    {"repo", "MetroSteam"}
        //}).dump(4));

        //clr_interop::clr_base::instance().cleanup_clr();

        threadContainer::getInstance().addThread(CreateThread(0, 0, StartWinHookAsync, 0, 0, 0));

        threadContainer::getInstance().addThread(CreateThread(0, 0, getConsoleHeader, 0, 0, 0));
        threadContainer::getInstance().addThread(CreateThread(0, 0, Initialize, 0, 0, 0));

        threadContainer::getInstance().addThread(CreateThread(0, 0, queryThemeUpdates, 0, 0, 0));

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

