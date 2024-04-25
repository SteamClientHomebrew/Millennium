#include <core/loader.hpp>
#include <filesystem>
#include <fstream>
#include <deps/deps.h>

#include <stdexcept>
#include <exception>
#include <typeinfo>

unsigned long enable_debugger(void)
{
#ifdef _WIN32
    const std::string filePath = ".cef-enable-remote-debugging";
#elif __linux__
    const std::string filePath =
        fmt::format("{}/.local/share/Steam/.cef-enable-remote-debugging", std::getenv("HOME"));
#endif

    if (!std::filesystem::exists(filePath)) 
    {
        if (!std::ifstream(filePath))
        {
            std::ofstream(filePath).close();
            return false;
        }
    }
    return true;
}

const void bootstrap()
{
    enable_debugger();
    
#ifdef _WIN32
    // allocate console
    static_cast<bool>(AllocConsole());

    // redirect standard output
    void(freopen("CONOUT$", "w", stdout));
    void(freopen("CONOUT$", "w", stderr));
#endif

    if (!dependencies::clone_millennium_module()) {
        return;
    }
	plugin::get().bootstrap();
}

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