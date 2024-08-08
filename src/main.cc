#define _WINSOCKAPI_
#define WIN32_LEAN_AND_MEAN 
#define UNICODE
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <filesystem>
#include <fstream>
#include <fmt/core.h>
// #include <boxer/boxer.h>
#include <sys/log.h>
#include <core/loader.h>
#include <core/py_controller/co_spawn.h>
#include <core/ftp/serv.h>
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
public:

    Preload() 
    {
        this->VerifyEnvironment();
    }

    ~Preload() { }

    const void VerifyEnvironment() 
    {
        const auto filePath = SystemIO::GetSteamPath() / ".cef-enable-remote-debugging";

        // Steam's CEF Remote Debugger isn't exposed to port 8080
        if (!std::filesystem::exists(filePath)) 
        {
            std::ofstream(filePath).close();

            Logger.Log("Successfully enabled CEF remote debugging, you can now restart Steam...");
            //boxer::show("Successfully initialized Millennium!. You can now manually restart Steam...", "Message");
            std::exit(1);
        }
    }

    const void Start() 
    {
        #ifdef _WIN32
        CheckForUpdates();
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
    //boxer::show("Millennium successfully loaded!", "Yay!");
    //EntryMain();
    return 1;
}
#elif __linux__
#include <stdio.h>
#include <stdlib.h>
#include <posix/helpers.h>
#include <dlfcn.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <stdexcept>

extern "C"
{
    /* Trampoline for the real main() */
    static int (*fnMainOriginal)(int, char **, char **);
    std::unique_ptr<std::thread> g_millenniumThread;

    /* Our fake main() that gets called by __libc_start_main() */
    int MainHooked(int argc, char **argv, char **envp)
    {
        pid_t pid = getpid();
        Logger.Log("Hooked main() with PID: {}", pid);

        std::string homeDirectory = std::getenv("HOME");

        /** 
         * Since this isn't an executable, and is "preloaded", the kernel doesn't implicitly load dependencies, so we need to manually. 
         * libpython3.11.so.1.0 should already be in $PATH, so we can just load it from there.
        */
        if (!dlopen(fmt::format("{}/.millennium/libpython-3.11.8.so", homeDirectory).c_str(), RTLD_LAZY | RTLD_GLOBAL)) 
        {
            LOG_ERROR("Failed to load python libraries: {},\n\nThis is likely because it was not found on disk, try reinstalling Millennium.", dlerror());
        }
        else
        {
            Logger.Log("Successfully loaded unix python libraries...");
        }

        /** Start Millennium on a new thread to prevent I/O blocking */
        g_millenniumThread = std::make_unique<std::thread>(EntryMain);
        g_millenniumThread->detach();

        int steam_main = fnMainOriginal(argc, argv, envp);

        Logger.Log("Steam main exited with code: {}", steam_main);
        return steam_main;
    }

    /*
    * Wrapper for __libc_start_main() that replaces the real main
    * function with our hooked version.
    */
    int __libc_start_main(
        int (*main)(int, char **, char **),
        int argc,
        char **argv,
        int (*init)(int, char **, char **),
        void (*fini)(void),
        void (*rtld_fini)(void),
        void *stack_end)
    {
        /* Save the real main function address */
        fnMainOriginal = main;

        /* Find the real __libc_start_main()... */
            typedef int (*libc_start_main_t)(int (*)(int, char **, char **), int, char **, int (*)(int, char **, char **), void (*)(void), void (*)(void), void *);
        libc_start_main_t orig = (libc_start_main_t)dlsym(RTLD_NEXT, "__libc_start_main");

        std::filesystem::path steamPath = std::filesystem::path(std::getenv("HOME")) / ".steam/steam/ubuntu12_32/steam";

        if (argv[0] != steamPath.string()) {
            /** Started from bash (invalid) */
            return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
        }

        Logger.Log("Loaded Millennium on {}, system architecture {}", GetLinuxDistro(), GetSystemArchitecture());
        /* ... and call it with our custom main function */
        return orig(MainHooked, argc, argv, init, fini, rtld_fini, stack_end);
    }


    /** This is a constructor, which is caught when a  */
    __attribute__((constructor)) void core_init() 
    {
        //Logger.Log("Loaded Millennium on {}, system architecture {}", GetLinuxDistro(), GetSystemArchitecture());
    }

    __attribute__((destructor)) void core_deinit() 
    {
        //Logger.Log("Unloading Millennium...");
    }
}

#endif
