#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <linux/limits.h>
#include <cstring>
#include <libgen.h>
#include <unistd.h>
#include <memory>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <filesystem>
#include "proxy-trampoline.h"

/**
 * When built in debug mode, MILLENNIUM_RUNTIME_PATH is set. 
 * In release you can choose wether to use MILLENNIUM_RUNTIME_PATH as a environment variable or not.
 */
#ifdef MILLENNIUM_RUNTIME_PATH
static const std::string kMillenniumLibraryPath = MILLENNIUM_RUNTIME_PATH;
#else
static const std::string kMillenniumLibraryPath = []() {
    const char* envPath = std::getenv("MILLENNIUM_RUNTIME_PATH");
    return envPath ? std::string(envPath) : "/usr/lib/millennium/libmillennium_x86.so";
}();
#endif

class ProxySentinel 
{
    void* m_millenniumInstanceHandle = nullptr;
    bool m_hasLoadedMillennium = false;

    typedef int (*StartMillennium_t)(void);
    typedef int (*StopMillennium_t)(void);

    bool LoadAndStartMillennium() 
    {
        m_millenniumInstanceHandle = dlopen(kMillenniumLibraryPath.c_str(), RTLD_LAZY | RTLD_GLOBAL);
        if (!m_millenniumInstanceHandle) 
        {
            LOG_ERROR("Failed to load Millennium library: %s", dlerror());
            return false;
        }

        auto startMillennium = (StartMillennium_t)dlsym(m_millenniumInstanceHandle, "StartMillennium");
        if (!startMillennium) 
        {
            LOG_ERROR("Failed to locate ordinal HookInterop::StartMillennium: %s", dlerror());
            dlclose(m_millenniumInstanceHandle);
            m_millenniumInstanceHandle = nullptr;
            return false;
        }

        int result = startMillennium();
        if (result < 0) 
        {
            LOG_ERROR("Failed to start Millennium: %d", result);
            dlclose(m_millenniumInstanceHandle);
            m_millenniumInstanceHandle = nullptr;
            return false;
        }

        return true;
    }

    void StopAndUnloadMillennium() 
    {
        if (!m_millenniumInstanceHandle) 
        {
            LOG_ERROR("Millennium library is not loaded.");
            return;
        }

        auto stopMillennium = (StopMillennium_t)dlsym(m_millenniumInstanceHandle, "StopMillennium");
        if (stopMillennium) 
        {
            int result = stopMillennium();
            if (result < 0) 
            {
                LOG_ERROR("Failed to stop Millennium: %d", result);
            }
        } 
        else 
        {
            LOG_ERROR("Failed to locate ordinal HookInterop::StopMillennium: %s", dlerror());
        }
        
        dlclose(m_millenniumInstanceHandle);
        m_millenniumInstanceHandle = nullptr;
    }

    char* GetProcessPath()
    {
        static char path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1)
        {
            path[len] = '\0';
            return path;
        }
        return nullptr;
    }

    char* GetProcessPathParent()
    {
        static char parentPath[PATH_MAX];
        char* exePath = GetProcessPath();
        if (!exePath) return nullptr;

        strncpy(parentPath, exePath, PATH_MAX - 1);
        parentPath[PATH_MAX - 1] = '\0';
        char* lastSlash = strrchr(parentPath, '/');

        if (lastSlash) *lastSlash = '\0';
        return parentPath;
    }

    void SetupHooks()
    {
        const char* currentModulePath = GetProcessPathParent();

        if (!currentModulePath) 
        {
            LOG_ERROR("Failed to retrieve current directory.");
            return;
        }

        auto libXTstPath = std::filesystem::path(currentModulePath) / "steam-runtime" / "usr" / "lib" / "i386-linux-gnu" / "libXtst.so.6";

        if (access(libXTstPath.string().c_str(), F_OK) == -1) 
        {
            LOG_ERROR("Pinned libXtst does not exist at: %s", libXTstPath.string().c_str());
            return;
        }

        g_originalXTstInstance = dlopen(libXTstPath.string().c_str(), RTLD_LAZY | RTLD_GLOBAL);
        if (!g_originalXTstInstance) 
        {
            fprintf(stderr, "Failed to load libXtst: %s\n", dlerror());
        }
    }

    bool IsSteamProcess()
    {
        char* exePath = GetProcessPath();
        if (!exePath) return false;

        char resolvedPath[PATH_MAX];
        if (!realpath(exePath, resolvedPath)) return false;

        const char* homeDir = getenv("HOME");
        if (!homeDir) return false;

        std::string steamPath = std::string(homeDir) + "/.steam/steam/ubuntu12_32/steam";

        char resolvedSteamPath[PATH_MAX];
        if (!realpath(steamPath.c_str(), resolvedSteamPath)) return false;

        return strcmp(resolvedPath, resolvedSteamPath) == 0;
    }

public:
    ProxySentinel() 
    {
        const bool isSteamProcess = IsSteamProcess();

        if (!isSteamProcess) 
        {
            LOG_INFO("Skipping Millennium setup for non-Steam process. Process path: %s", GetProcessPath());
            return;
        }

        LOG_INFO("Setting up proxy hooks...");
        this->SetupHooks();

        m_hasLoadedMillennium = true;

        LOG_INFO("Bootstrap library loaded successfully. Using Millennium library at: %s", kMillenniumLibraryPath.c_str());
        if (this->LoadAndStartMillennium()) 
        {
            LOG_INFO("Starting Millennium...");
        }
    }
    
    ~ProxySentinel() 
    {
        if (g_originalXTstInstance) 
        {
            dlclose(g_originalXTstInstance);
        }

        if (!m_hasLoadedMillennium) 
        {
            return;
        }

        LOG_INFO("Unloading Millennium library...");
        this->StopAndUnloadMillennium();
    }
};

std::shared_ptr<ProxySentinel> sentinel = std::make_shared<ProxySentinel>();