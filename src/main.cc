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
#include <cxxabi.h>
#include <pipes/terminal_pipe.h>
#include <api/executor.h>

const static void VerifyEnvironment() 
{
    const auto filePath = SystemIO::GetSteamPath() / ".cef-enable-remote-debugging";

    // Steam's CEF Remote Debugger isn't exposed to port 8080
    if (!std::filesystem::exists(filePath)) 
    {
        std::ofstream(filePath).close();

        Logger.Log("Successfully enabled CEF remote debugging, you can now restart Steam...");
        std::exit(1);
    }
}

void OnTerminate() 
{
    if (IsDebuggerPresent())    
    {
        // If a debugger is attached, let the debugger handle the exception
        #ifdef _WIN32
        __debugbreak();
        #elif __linux__
        raise(SIGTRAP);
        #endif
        return;
    }

    auto const exceptionPtr = std::current_exception();
    std::string errorMessage = "Millennium has a fatal error that it can't recover from, check the logs for more details!";

    if (exceptionPtr) 
    {
        try 
        {
            int status;
            auto const exceptionType = abi::__cxa_demangle(abi::__cxa_current_exception_type()->name(), 0, 0, &status);
            errorMessage.append("\nTerminating with uncaught exception of type `");
            errorMessage.append(exceptionType);
            errorMessage.append("`");
            std::rethrow_exception(exceptionPtr); // rethrow the exception to catch its exception message
        }
        catch (const std::exception& e) 
        {
            errorMessage.append(" with `what()` = \"");
            errorMessage.append(e.what());
            errorMessage.append("\"");
        }
        catch (...) { }
    }

    #ifdef _WIN32
    MessageBoxA(NULL, errorMessage.c_str(), "Oops!", MB_ICONERROR | MB_OK);
    #elif __linux__
    std::cerr << errorMessage << std::endl;
    #endif
}

/* Wrapped cross platform entrypoint */
const static void EntryMain() 
{
    std::set_terminate(OnTerminate); // Set custom terminate handler for easier debugging
    
    /** Handle signal interrupts (^C) */
    signal(SIGINT, [](int signalCode) { std::exit(128 + SIGINT); });

    #ifdef _WIN32

    try {
        if (std::filesystem::exists(SystemIO::GetInstallPath() / "user32.queue.dll"))
        {
            Logger.Log("Updating shim module from cache...");

            while (true) {
                try {
                    std::filesystem::remove(SystemIO::GetInstallPath() / "user32.dll");
                    break;
                }
                catch (std::filesystem::filesystem_error& e) {
                    continue;
                }
            }

            Logger.Log("Removed old inject shim...");

            std::filesystem::rename(SystemIO::GetInstallPath() / "user32.queue.dll", SystemIO::GetInstallPath() / "user32.dll"); 
            Logger.Log("Successfully updated user32.dll!");
        }
    }
    catch (std::exception& e) {
        LOG_ERROR("Failed to update user32.dll: {}", e.what());

        #ifdef _WIN32
        MessageBoxA(NULL, "Failed to update user32.dll, it's recommended that you reinstall Millennium.", "Oops!", MB_ICONERROR | MB_OK);
        #endif
    }

    std::unique_ptr<StartupParameters> startupParams = std::make_unique<StartupParameters>();

    if (startupParams->HasArgument("-verbose"))
    {
        CreateTerminalPipe();
    }
    #endif

    uint16_t ftpPort = Crow::CreateAsyncServer();

    const auto startTime = std::chrono::system_clock::now();
    VerifyEnvironment();

    std::shared_ptr<PluginLoader> loader = std::make_shared<PluginLoader>(startTime, ftpPort);
    SetPluginLoader(loader);

    PythonManager& manager = PythonManager::GetInstance();

    auto backendThread   = std::thread([&loader, &manager] { loader->StartBackEnds(manager); });
    auto frontendThreads = std::thread([&loader] { loader->StartFrontEnds(); });

    backendThread.join();
    frontendThreads.join();

    Logger.Log("Millennium has gracefully shut down.");
}

#ifdef _WIN32
std::unique_ptr<std::thread> g_millenniumThread;

int __stdcall DllMain(void*, unsigned long fdwReason, void*) 
{
    switch (fdwReason) 
    {
        case DLL_PROCESS_ATTACH: 
        {
            g_millenniumThread = std::make_unique<std::thread>(EntryMain);
            break;
        }
        case DLL_PROCESS_DETACH: 
        {
            Logger.Log("Shutting down Millennium...");
            std::exit(EXIT_SUCCESS);
            g_threadTerminateFlag->flag.store(true);
            Sockets::Shutdown();
            g_millenniumThread->join();
            Logger.PrintMessage(" MAIN ", "Millennium has been shut down.", COL_MAGENTA);

            std::this_thread::sleep_for(std::chrono::seconds(1000));
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
        Logger.Log("Hooked main() with PID: {}", getpid());

        /** 
         * Since this isn't an executable, and is "preloaded", the kernel doesn't implicitly load dependencies, so we need to manually. 
         * libpython3.11.so.1.0 should already be in $PATH, so we can just load it from there.
        */
        std::string pythonPath = fmt::format("{}/.millennium/libpython-3.11.8.so", std::getenv("HOME"));

        if (!dlopen(pythonPath.c_str(), RTLD_LAZY | RTLD_GLOBAL)) 
        {
            LOG_ERROR("Failed to load python libraries: {},\n\nThis is likely because it was not found on disk, try reinstalling Millennium.", dlerror());
        }

        #ifdef MILLENNIUM_SHARED
        {
            /** Start Millennium on a new thread to prevent I/O blocking */
            g_millenniumThread = std::make_unique<std::thread>(EntryMain);
            int steam_main = fnMainOriginal(argc, argv, envp);
            Logger.Log("Hooked Steam entry returned {}", steam_main);

            g_threadTerminateFlag->flag.store(true);
            Sockets::Shutdown();
            g_millenniumThread->join();

            Logger.Log("Shutting down Millennium...");
            return steam_main;
        }
        #else
        {
            g_threadTerminateFlag->flag.store(true);
            g_millenniumThread = std::make_unique<std::thread>(EntryMain);
            g_millenniumThread->join();
            return 0;
        }
        #endif
    }

    void RemoveFromLdPreload() 
    {
        const std::string fileToRemove = std::filesystem::path(std::getenv("HOME")) / ".millennium" / "libMillennium.so";

        const char* ldPreload = std::getenv("LD_PRELOAD");
        if (!ldPreload) 
        {
            std::cerr << "LD_PRELOAD is not set." << std::endl;
            return;
        }

        std::string token;
        std::string ldPreloadStr = ldPreload;
        std::istringstream iss(ldPreloadStr);
        std::vector<std::string> updatedPreload;

        // Split LD_PRELOAD by spaces and filter out fileToRemove
        while (iss >> token) 
        {
            if (token != fileToRemove) 
            {
                updatedPreload.push_back(token);
            }
        }

        // Join the remaining paths back into a single string
        std::ostringstream oss;
        for (size_t i = 0; i < updatedPreload.size(); ++i) 
        {
            if (i > 0) oss << ' ';
            oss << updatedPreload[i];
        }

        Logger.Log("Updating LD_PRELOAD from [{}] to [{}]", ldPreloadStr, oss.str());

        // Set the updated LD_PRELOAD
        if (setenv("LD_PRELOAD", oss.str().c_str(), 1) != 0) 
        {
            perror("setenv");
        }
    }

    #ifdef MILLENNIUM_SHARED

    int IsSamePath(const char *path1, const char *path2) 
    {
        char realpath1[PATH_MAX], realpath2[PATH_MAX];
        struct stat stat1, stat2;

        // Get the real paths for both paths (resolves symlinks)
        if (realpath(path1, realpath1) == NULL) 
        {
            perror("realpath failed for path1");
            return 0;  // Error in resolving path
        }
        if (realpath(path2, realpath2) == NULL) 
        {
            perror("realpath failed for path2");
            return 0;  // Error in resolving path
        }

        // Compare resolved paths
        if (strcmp(realpath1, realpath2) != 0) 
        {
            return 0;  // Paths are different
        }

        // Check if both paths are symlinks and compare symlink targets
        if (lstat(path1, &stat1) == 0 && lstat(path2, &stat2) == 0) 
        {
            if (S_ISLNK(stat1.st_mode) && S_ISLNK(stat2.st_mode)) 
            {
                // Both are symlinks, compare the target paths
                char target1[PATH_MAX], target2[PATH_MAX];
                ssize_t len1 = readlink(path1, target1, sizeof(target1) - 1);
                ssize_t len2 = readlink(path2, target2, sizeof(target2) - 1);

                if (len1 == -1 || len2 == -1) 
                {
                    perror("readlink failed");
                    return 0;
                }

                target1[len1] = '\0';
                target2[len2] = '\0';

                // Compare the symlink targets
                if (strcmp(target1, target2) != 0) 
                {
                    return 0;  // Symlinks point to different targets
                }
            }
        }

        return 1;  // Paths are the same, including symlinks to each other
    }

    /*
    * Trampoline for __libc_start_main() that replaces the real main
    * function with our hooked version.
    */
    int __libc_start_main(
        int (*main)(int, char **, char **), int argc, char **argv,
        int (*init)(int, char **, char **), void (*fini)(void), void (*rtld_fini)(void), void *stack_end)
    {
        /* Save the real main function address */
        fnMainOriginal = main;

        /* Get the address of the real __libc_start_main() */
        decltype(&__libc_start_main) orig = (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

        Logger.Log("Hooked __libc_start_main() {}", argv[0]);

        /* Get the path to the Steam executable */
        std::filesystem::path steamPath = std::filesystem::path(std::getenv("HOME")) / ".steam/steam/ubuntu12_32/steam";

        /** not loaded in a invalid child process */
        if (!IsSamePath(argv[0], steamPath.string().c_str()))
        {
            return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
        }

        /* Remove the Millennium library from LD_PRELOAD */
        RemoveFromLdPreload();
        /* Log that we've loaded Millennium */
        Logger.Log("Loaded Millennium on {}, system architecture {}", GetLinuxDistro(), GetSystemArchitecture());
        /* ... and call it with our custom main function */
        return orig(MainHooked, argc, argv, init, fini, rtld_fini, stack_end);
    }
    #endif
}

int main(int argc, char **argv, char **envp)
{
    return MainHooked(argc, argv, envp);
}
#endif
