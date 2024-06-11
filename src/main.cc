#define _WINSOCKAPI_
#define WIN32_LEAN_AND_MEAN 
#define UNICODE
#include <filesystem>
#include <fstream>
#include <deps/deps.h>
#include <fmt/core.h>
#ifdef _WIN32
#include <procmon/thread.h>
#endif
#include <boxer/boxer.h>
#include <sys/log.h>
#include <core/loader.h>
#include <core/py_controller/co_spawn.h>

class Preload 
{
private:

    const char* builtinsRepository = "https://github.com/SteamClientHomebrew/__builtins__.git";
    const char* pythonModulesRepository = "https://github.com/SteamClientHomebrew/Packages.git";

    std::filesystem::path builtinsModulesPath = SystemIO::GetSteamPath() / "steamui" / "plugins" / "__millennium__";

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
        const bool bBuiltInSuccess = Dependencies::GitAuditPackage("@builtins", builtinsModulesPath.string(), builtinsRepository);

        if (!bBuiltInSuccess) 
        {
            Logger.Error("failed to audit builtin modules...");
            return;
        }

        // python modules only need to be audited on windows systems. 
        // linux users need their own installation of python
        #ifdef _WIN32
        const bool bPythonModulesSuccess = Dependencies::GitAuditPackage("@packages", pythonModulesBaseDir.string(), pythonModulesRepository);
        
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
    //#ifdef _WIN32
    //if (!IsSteamApplication())
    //{
    //    return;
    //}
    //#endif

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