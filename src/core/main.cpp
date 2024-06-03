#define _WINSOCKAPI_
#define WIN32_LEAN_AND_MEAN 
#define UNICODE
#include <core/loader.hpp>
#include <filesystem>
#include <fstream>
#include <deps/deps.h>

#include <stdexcept>
#include <exception>
#include <typeinfo>
#include <fmt/core.h>
#ifdef _WIN32
#include <_win32/thread.h>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#endif
#include <git2.h>
#include <boxer/boxer.h>
#include <generic/base.h>
#include <string>
#include <utilities/log.hpp>

class Preload 
{
public:

    Preload() 
    {
        this->VerifyEnvironment();
        #if _WIN32
        PauseSteamCore();
        #endif
    }

    ~Preload() 
    {
        #if _WIN32
        UnpauseSteamCore();
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
            std::exit(0);
        }
    }

    const void Start() 
    {
        const bool bBuiltInSuccess = Dependencies::GitAuditPackage("@builtins", builtinsModulesAbsolutePath, builtinsRepository);

        if (!bBuiltInSuccess) 
        {
            Logger.Error("failed to audit builtin modules...");
            return;
        }

        // python modules only need to be audited on windows systems. 
        // linux users need their own installation of python
        #ifdef _WIN32
        const bool bPythonModulesSuccess = Dependencies::GitAuditPackage("@packages", pythonModulesBaseDir.generic_string(), pythonModulesRepository);
        
        if (!bPythonModulesSuccess) 
        {
            Logger.Error("failed to audit python modules...");
            return;
        }
        #endif
    }
};

/* Wrapped cross platform entrypoint */
const static void EntryMain() 
{
    #ifdef _WIN32
    {
        std::string processName = GetProcessName(GetCurrentProcessId());

        if (processName != "steam.exe")
        {
            return; // don't load into non Steam Applications. 
        }
    }
    #endif

    const auto startTime = std::chrono::system_clock::now();
    {
        std::unique_ptr<Preload> preload = std::make_unique<Preload>();
        preload->Start();
    }

    std::unique_ptr<PluginLoader> loader = std::make_unique<PluginLoader>(startTime);

    loader->StartBackEnds();
    loader->StartFrontEnds();
}

#ifdef _WIN32
int __stdcall DllMain(void*, unsigned long fdwReason, void*) 
{
    if (fdwReason == DLL_PROCESS_ATTACH) 
    {
        std::thread(EntryMain).detach();
    }
    return true;
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdShow)
{
    EntryMain();
    return 1;
}
#elif __linux__
int main()
{
    EntryMain();
    return 1;
}
#endif