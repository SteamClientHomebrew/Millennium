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

#define COLOR_RESET "\033[0m"
#define COLOR_INFO "\033[1;34m"
#define COLOR_ERROR "\033[1;31m"
#define COLOR_WARN "\033[1;33m"

#define GET_TIMESTAMP() ({ \
    struct timeval tv; \
    struct tm *tm_info; \
    gettimeofday(&tv, NULL); \
    tm_info = localtime(&tv.tv_sec); \
    static char timestamp_buf[16]; \
    snprintf(timestamp_buf, sizeof(timestamp_buf), "[%02d:%02d.%03d]", tm_info->tm_min, tm_info->tm_sec, (int)(tv.tv_usec / 10000)); \
    timestamp_buf; \
})

#define LOG_INFO(fmt, ...) fprintf(stdout, "%s " COLOR_INFO "BOOTSTRAP-INFO " COLOR_RESET fmt "\n", GET_TIMESTAMP(), ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "%s " COLOR_ERROR "BOOTSTRAP-ERROR " COLOR_RESET fmt "\n", GET_TIMESTAMP(), ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) fprintf(stderr, "%s " COLOR_WARN "BOOTSTRAP-WARN " COLOR_RESET fmt "\n", GET_TIMESTAMP(), ##__VA_ARGS__)

void* g_originalXtstInstance = NULL;

#define HOOK_FUNC(name, ret_type, params, args) \
ret_type name params { \
    static ret_type (*orig) params = NULL; \
    if (!orig) { \
        orig = (ret_type (*) params)dlsym(g_originalXtstInstance, #name); \
        if (!orig) { \
            LOG_ERROR("Cannot find original %s", #name); \
            return False; \
        } \
    } \
    return orig args; \
}

HOOK_FUNC(XTestFakeButtonEvent, int, (Display *dpy, unsigned int button, Bool is_press, unsigned long delay), (dpy, button, is_press, delay))
HOOK_FUNC(XTestFakeKeyEvent, int, (Display *dpy, unsigned int keycode, Bool is_press, unsigned long delay), (dpy, keycode, is_press, delay))
HOOK_FUNC(XTestQueryExtension, int, (Display *dpy, int *event_basep, int *error_basep, int *major_versionp, int *minor_versionp), (dpy, event_basep, error_basep, major_versionp, minor_versionp))
HOOK_FUNC(XTestFakeRelativeMotionEvent, int, (Display *dpy, int x, int y, unsigned long delay), (dpy, x, y, delay))
HOOK_FUNC(XTestFakeMotionEvent, int, (Display *dpy, int screen_number, int x, int y, unsigned long delay), (dpy, screen_number, x, y, delay))

static const std::string kMillenniumLibraryPath = []() {
    const char* envPath = std::getenv("MILLENNIUM_RUNTIME_PATH");
    return envPath ? std::string(envPath) : "/usr/lib/millennium/libmillennium_x86.so";
}();

class ProxySentinel 
{
    void* m_millenniumInstanceHandle = nullptr;
    void* m_originalXTstInstance = nullptr;

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

    static const char* GetCurrentDirectory() 
    {
        Dl_info dl_info;
        static char abs_path[PATH_MAX];
        static char parent_path[PATH_MAX];
        
        if (dladdr((void*)GetCurrentDirectory, &dl_info)) 
        {
            const char* path = realpath(dl_info.dli_fname, abs_path) ? abs_path : dl_info.dli_fname;
            strncpy(parent_path, path, sizeof(parent_path) - 1);
            parent_path[sizeof(parent_path) - 1] = '\0';
            return dirname(parent_path);
        }
        
        return nullptr;
    }

    void SetupHooks()
    {
        const char* currentModulePath = GetCurrentDirectory();
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

        m_originalXTstInstance = dlopen(libXTstPath.string().c_str(), RTLD_LAZY | RTLD_GLOBAL);
        if (!m_originalXTstInstance) 
        {
            fprintf(stderr, "Failed to load libXtst: %s\n", dlerror());
        }
    }

public:
    ProxySentinel() 
    {
        LOG_INFO("Setting up proxy hooks...");
        SetupHooks();

        LOG_INFO("Bootstrap library loaded successfully. Using Millennium library at: %s", kMillenniumLibraryPath.c_str());
        if (LoadAndStartMillennium()) 
        {
            LOG_INFO("Starting Millennium...");
        }
    }
    
    ~ProxySentinel() 
    {
        LOG_INFO("Unhooking libXtst...");
        if (m_originalXTstInstance) 
        {
            dlclose(m_originalXTstInstance);
        }

        LOG_INFO("Unloading Millennium library...");
        StopAndUnloadMillennium();
    }
};

std::shared_ptr<ProxySentinel> sentinel = std::make_shared<ProxySentinel>();