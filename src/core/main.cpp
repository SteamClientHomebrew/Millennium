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
#include <_win32/cmd.h>
#endif
#include <git2.h>
#include <boxer/boxer.h>
/**
 * @brief constructor of hooked module to setup the environment
 * 
 * - writes the debugger flag to steam so it launches with remote debugging
 * - allocated the dev console if applicable on windows
 * 
 * @return success (discarded from kernel)
*/
const bool __attribute__((constructor)) setup_env() 
{
#ifdef _WIN32
    if (steam::get().params.has("-dev") && static_cast<bool>(AllocConsole())) {
        void(freopen("CONOUT$", "w", stdout));
        void(freopen("CONOUT$", "w", stderr));
    }
#endif

#ifdef _WIN32
    constexpr const char* filePath = ".cef-enable-remote-debugging";
#elif __linux__
    std::string filePath = fmt::format("{}/.local/share/Steam/.cef-enable-remote-debugging", std::getenv("HOME"));
#endif

    if (!std::filesystem::exists(filePath)) {
        std::ofstream(filePath).close();

        boxer::show("Successfully initialized Millennium!. You can now manually restart Steam...", "Message");
        
        std::exit(0);
        return false;
    }
    return true;
}

/**
 * @brief wrapper for start function
 * 
 * - ensures modules are up to date
 * - starts the plugin backend and frontends
*/
const void bootstrap()
{
    SetConsoleOutputCP(CP_UTF8);
    set_start_time(std::chrono::high_resolution_clock::now());
    
    hook_steam_thread();

    if (!dependencies::clone_millennium_module() || !dependencies::embed_python()) {
        return;
    }

    unhook_steam_thread();
	plugin::get().bootstrap();
}

#ifdef _WIN32
int __attribute__((__stdcall__)) DllMain(void*, unsigned long fdwReason, void*) 
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        std::thread(bootstrap).detach();
    }
    return true;
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    bootstrap();
    return 1;
}
#elif __linux__
int main()
{
    bootstrap();
    return 1;
}
#endif