#include <stdafx.h>
#include <core/injector/event_handler.hpp>

#include <core/steam/application.hpp>
#include <window/core/window.hpp>

#include <utils/thread/thread_handler.hpp>
#include <core/injector/startup/bootstrap.hpp>

#pragma comment(lib, "pdh.lib")
#include <utils/io/metrics.hpp>

#include <utils/updater/update.hpp>
#include <utils/clr/platform.hpp>

#ifdef _WIN32
#include <Psapi.h>
#include <pdh.h>
#include <core/steam/window/manager.hpp>
#elif __linux__
#include <fcntl.h>
#include <unistd.h>
#endif

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
    unsigned long checkRemoteDebugging(void)
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
    unsigned long getConsoleHeader(void* lpParam) {
#ifdef _WIN32
        // only update the console header if the console is actually visible
        if (!steam::get().params.has("-dev"))
        {
            return 0;
        }

        while (true) {

            std::unique_ptr<Metrics> metrics = std::make_unique<Metrics>();

            //double cpu = metrics->getCpuUsage();
            double ram = metrics->getMemoryUsage();

            SetConsoleTitle(fmt::format("Millennium.Patcher-v{} [Threads: {}, Private-Bytes {} MB]",
                m_ver,
                c_threads::get().runningThreads.size(),
                std::round(ram * 100.0) / 100.0
            ).c_str());

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
#endif
       return 0;
    }

    unsigned long Bootstrap(void* lpParam)
    {
        if (steam::get().params.has("-dev") && 
            !steam::get().params.has("-mllm-no-stdout"))
        {
#ifdef _WIN32
            // windows allocation 
            static_cast<bool>(AllocConsole());
            void(freopen("CONOUT$", "w", stdout));
            void(freopen("CONOUT$", "w", stderr));

#elif __linux__
            // This is untested and unverified 
            int fd_console;
            fd_console = open("/dev/tty", O_RDWR);

            if (fd_console == -1) {
                perror("Error opening console");
                return 1;
            }
            dup2(fd_console, STDIN_FILENO);
            dup2(fd_console, STDOUT_FILENO);
            dup2(fd_console, STDERR_FILENO);
#endif
            console.consoleAllocated = true;
        }

        c_threads::get().hookAllThreads();
        {
            // check if remote debugging is enabled on the client.
            // required in order for millennium to work
            if (!Millennium::checkRemoteDebugging())
            {
                //MsgBox("Initialization complete! Restart Steam for changed to take effect.", "Millennium", MB_ICONINFORMATION);
                msg::show("Initialization complete! Restart Steam for changed to take effect.", "Millennium");

                exit(0);
            }

            updater::check_for_updates();

            std::cout << " Millennium Dev Tools v" << m_ver << std::endl;
            std::cout << " Developed by ShadowMonster (discord: sm74)" << std::endl;
            std::cout << " Need help? Check the docs: https://millennium.gitbook.io/steam-patcher/development/overview" << std::endl;
            std::cout << " Still need help? Join the discord server: https://millennium.web.app/discord" << std::endl;
            std::cout << "-------------------------------------------------------------------------------------\n" << std::endl;

#ifdef _WIN32
            // adjust the steam client windows.
            c_threads::get().add(std::thread([&] { StartWinHookAsync(nullptr); }));
#endif
            c_threads::get().add(std::thread([&] { getConsoleHeader(nullptr); }));
            c_threads::get().add(std::thread([&] { Initialize(); }));

            // synchronously run theme updater 
            queryThemeUpdates(nullptr);
        }
        c_threads::get().unhookAllThreads();

        return 0;
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
#ifdef _WIN32
int __stdcall DllMain(HINSTANCE hinstDLL, DWORD call, void*)
{
    if (call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinstDLL);
        // start bootstrapper thread disjoined from the main thread 
        // to prevent it from freezing itself when it hooks
        c_threads::get().add(std::thread([&] { Millennium::Bootstrap(nullptr); }));
    }

    return true;
}
#elif __linux__
int main() {
    std::cout << "This is just a test! Millennium for linux is up!" << std::endl;

    return 0;
}
#endif