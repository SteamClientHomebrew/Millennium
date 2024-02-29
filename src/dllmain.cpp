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
    /**
     * @brief Checks for the presence of a file indicating remote debugging.
     *
     * @return unsigned long Returns true if the file indicating remote debugging is found; otherwise, false.
     *
     * @remarks
     * - Checks for the presence of a file named ".cef-enable-remote-debugging".
     * - If the file does not exist, creates it and returns false.
     * - If the file exists, returns true.
     */
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

    /**
     * @brief Updates the console title with relevant information.
     *
     * @param lpParam A pointer to the parameters (not used in this function).
     *
     * @return unsigned long Always returns 0.
     *
     * @remarks
     * - Updates the console title periodically with information such as the version, number of running threads,
     *   and memory usage of the application.
     * - Uses a Metrics object to retrieve memory usage.
     * - Sleeps for 1 second between updates.
     */
    unsigned long __stdcall getConsoleHeader(void* lpParam) {

        // only update the console header if the console is actually visible
        if (!steam::get().params.has("-dev"))
        {
            return 0;
        }

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
            void(freopen("CONOUT$", "w", stderr));
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

/**
 * @brief Entry point that is accidentally called by the Steam Client
 *
 * @param hinstDLL A handle to the DLL module.
 * @param call     The reason code that indicates why the DLL entry-point function is being called.
 * @param lpReserved Reserved parameter; must be NULL.
 *
 * @return BOOL For DLL_PROCESS_ATTACH, returns true to indicate success.
 *              For other cases, returns true.
 *
 * @remarks
 * - This function is called by the system when processes and threads are initialized and terminated, or when modules are loaded and unloaded.
 * - In this implementation, it disables thread notifications for the DLL and starts the Millennium Bootstrap thread if remote debugging is not active.
 * - If remote debugging is active, it displays a message box and exits the process.
 */
int __stdcall DllMain(HINSTANCE hinstDLL, DWORD call, void*)
{
    if (call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinstDLL);
        hCurrentModule = hinstDLL;

        // check if remote debugging is enabled on the client.
        // required in order for millennium to work
        if (!Millennium::checkRemoteDebugging())
        {
            MsgBox("Initialization complete! Restart Steam for changed to take effect.", "Millennium", MB_ICONINFORMATION);
            ExitProcess(EXIT_SUCCESS);
        }

        // start bootstrapper thread disjoined from the main thread 
        // to prevent it from freezing itself when it hooks
        threadContainer::getInstance().addThread(CreateThread(0, 0, Millennium::Bootstrap, 0, 0, 0));
    }

    return true;
}

