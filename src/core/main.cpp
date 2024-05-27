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
#endif
#include <git2.h>
#include <boxer/boxer.h>
#include <generic/base.h>
#include <string>
#include <utilities/log.hpp>
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
    set_start_time(std::chrono::high_resolution_clock::now());
    
#ifdef _WIN32
    hook_steam_thread();
#endif

    const bool __builtins__ = dependencies::audit_package("@builtins", millennium_modules_path, builtins_repo);
    const bool __rmods__    = dependencies::audit_package("@packages", modules_basedir.generic_string(), modules_repo);

    if (!__builtins__ || !__rmods__) {
        return;
    }

#ifdef _WIN32
    unhook_steam_thread();
#endif
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