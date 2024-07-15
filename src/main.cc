#define _WINSOCKAPI_
#define WIN32_LEAN_AND_MEAN 
#define UNICODE
#ifdef _WIN32
#include <winsock2.h>
#include <procmon/thread.h>
#endif
#include <filesystem>
#include <fstream>
#include <git/git.h>
#include <fmt/core.h>
#include <boxer/boxer.h>
#include <sys/log.h>
#include <core/loader.h>
#include <core/py_controller/co_spawn.h>
#include <core/ftp/serv.h>
#include <git/pyman.h>
#include <signal.h>
#include <pipes/terminal_pipe.h>
#include <git/vcs.h>

/** Handle ^C signal from terminal */
void HandleSignalInterrupt(int sig) 
{
    std::exit(1);
}

class Preload 
{
private:

    const char* builtinsRepository = "https://github.com/SteamClientHomebrew/Core.git";
    const char* pythonModulesRepository = "https://github.com/SteamClientHomebrew/Packages.git";

    std::filesystem::path builtinsModulesPath = SystemIO::GetSteamPath() / "ext" / "data" / "assets";

public:

    Preload() 
    {
        this->VerifyEnvironment();
        #if _WIN32
        //PauseSteamCore();
        #endif
    }

    ~Preload() 
    {
        #if _WIN32
        //UnpauseSteamCore();
        #endif
    }

    const void VerifyEnvironment() 
    {
        const auto filePath = SystemIO::GetSteamPath() / ".cef-enable-remote-debugging";

        // Steam's CEF Remote Debugger isn't exposed to port 8080
        if (!std::filesystem::exists(filePath)) 
        {
            std::ofstream(filePath).close();

            boxer::show("Successfully initialized Millennium!. You can now manually restart Steam...", "Message");
            std::exit(1);
        }
    }

    const void Start() 
    {
        CheckForUpdates();

        const bool bBuiltInSuccess = Dependencies::GitAuditPackage("@Core", builtinsModulesPath.string(), builtinsRepository);

        if (!bBuiltInSuccess) 
        {
            LOG_ERROR("failed to audit builtin modules...");
            return;
        }

        // python modules only need to be audited on windows systems. 
        // linux users need their own installation of python
        #ifdef _WIN32
        {
            std::unique_ptr<PythonInstaller> pythonInstaller = std::make_unique<PythonInstaller>("3.11.8");
            pythonInstaller->InstallPython();
        }
        #endif
    }
};

/* Wrapped cross platform entrypoint */
const static void EntryMain() 
{
    std::unique_ptr<StartupParameters> startupParams = std::make_unique<StartupParameters>();
    const bool isVerboseMode = startupParams->HasArgument("-verbose");

    if (isVerboseMode)
    {
        CreateTerminalPipe();   
    }

    signal(SIGINT, HandleSignalInterrupt);
    uint16_t ftpPort = Crow::CreateAsyncServer();

    const auto startTime = std::chrono::system_clock::now();
    {
        std::unique_ptr<Preload> preload = std::make_unique<Preload>();
        preload->Start();
    }

    std::unique_ptr<PluginLoader> loader = std::make_unique<PluginLoader>(startTime, ftpPort);

    PythonManager& manager = PythonManager::GetInstance();

    std::thread([&loader, &manager] { loader->StartBackEnds(manager); }).detach();
    loader->StartFrontEnds();

    std::promise<void>().get_future().wait();
}

#ifdef _WIN32
int __stdcall DllMain(void*, unsigned long fdwReason, void*) 
{
    switch (fdwReason) 
    {
        case DLL_PROCESS_ATTACH: {
            std::thread(EntryMain).detach();
            break;
        }
        case DLL_PROCESS_DETACH: {
            Logger.Log("Shutting down Millennium...");
            std::exit(1);
            break;
        }
    }

    return true;
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdShow)
{
    boxer::show("Millennium successfully loaded!", "Yay!");
    //EntryMain();
    return 1;
}
#elif __linux__
#include <stdio.h>
#include <stdlib.h>
#include <posix/helpers.h>

int main()
{
    Logger.Log("Starting Millennium on {}, system architecture {}", GetLinuxDistro(), GetSystemArchitecture());

    EntryMain();
    return 1;
}
#endif
