#define WIN32_LEAN_AND_MEAN 
#include <extern/injector/inject.hpp>
#include <shellapi.h>
#include <filesystem>

#pragma warning(disable: 6258)
#pragma warning(disable: 6387)

/// <summary>
/// check for updates on the millennium client
/// updates are forced and the user has to do them or delete the client
/// </summary>
class auto_updater {
private:
    const char* current_app_version = "1.0.0";
    const char* repo_url = "https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest";

    std::string github_response;
    HINTERNET request_buffer = NULL;
    HINTERNET connection = NULL;

public:
    /// <summary>
    /// create internet handle to connect to the github repo
    /// </summary>
    auto_updater() {
        connection = InternetOpenA("millennium.updater", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        request_buffer = InternetOpenUrlA(connection, repo_url, NULL, 0, INTERNET_FLAG_RELOAD, 0);

        if (!request_buffer || !connection) {
            InternetCloseHandle(connection);
        }
    }

    /// <summary>
    /// check update tag_name in the latest repo and check against the static version we have locally
    /// </summary>
    /// <param name=""></param>
    /// <returns></returns>
    __declspec(noinline) void __fastcall check_for_updates(void) {
        const int buffer_size = 4096;
        char buffer[buffer_size] = {};
        unsigned long read = 0;

        while (InternetReadFile(request_buffer, buffer, sizeof(buffer), &read) && read > 0) github_response.append(buffer, read);

        if (github_response.empty() || nlohmann::json::parse(github_response)["tag_name"].get<std::string>() == current_app_version) {
            return;
        }

        //force the update on the user
        MessageBoxA(GetForegroundWindow(), "A new version of the patcher is available!\nYou are required to download the latest release, or you can uninstall the current release you have.\nRun the installer on the github page to download the new release", "Millennium - Updater", MB_OK | MB_ICONINFORMATION);
        ShellExecute(NULL, "open", "https://github.com/ShadowMonster99/millennium-steam-patcher/releases/latest", NULL, NULL, SW_SHOWNORMAL);
        exit(1);
    }

    /// <summary>
    /// deconstructor, used to discard the internet handles
    /// </summary>
    ~auto_updater() {
        if (request_buffer != NULL) {
            InternetCloseHandle(request_buffer);
        }
        if (connection != NULL) {
            InternetCloseHandle(connection);
        }
    }
};

class millennium
{
private:
    auto_updater millennium_updater;
    millennium() {}

public:

    //create singleton istance of millennium
    static millennium& get_instance()
    {
        static millennium instance;
        return instance;
    }

    millennium(const millennium&) = delete;
    millennium& operator=(const millennium&) = delete;

    /// <summary>
    /// check if the cef remote debugging is enabled
    /// you can enable the debugging from the file name
    /// </summary>
    /// <param name=""></param>
    /// <returns></returns>
    static unsigned long __stdcall check_debugger(void)
    {
        constexpr const std::basic_string_view<char> filePath = ".cef-enable-remote-debugging";

        if (not std::ifstream(filePath.data()))
        {
            std::ofstream(filePath.data()).close();
            return false;
        }

        return true;
    }

    /// <summary>
    /// initialize millennium and allocate console
    /// </summary>
    /// <param name="lpParam">thread context passed in by winapi</param>
    /// <returns></returns>
    unsigned long __stdcall bootstrap(LPVOID lpParam)
    {
        constexpr const std::basic_string_view<char> dev_cmd = "-dev";

        if (static_cast<std::string>(GetCommandLineA()).find(dev_cmd) xor std::string::npos) 
        {
            if (static_cast<bool>(AllocConsole()))
            {
                void(freopen("CONOUT$", "w", stdout));
            }
        }

        // millennium_updater.check_for_updates();

        //discard the value of the thread object in stack memory 
        reinterpret_cast<void*>(CreateThread(NULL, NULL, Initialize, NULL, NULL, NULL));
        return true;
    }
};

/// <summary>
/// thread callback wrapper used to initialize the millennium client through the singleton instance
/// </summary>
/// <param name="lpParam"></param>
/// <returns></returns>
unsigned long __cdecl thread_callback_wrapper(void* lpParam)
{
    const auto threadCallback = [](LPVOID lpParam) -> DWORD {
        return millennium::get_instance().bootstrap(lpParam);
    };

    return threadCallback(lpParam);
}


/// <summary>
/// dll entry point, the file is auto loaded by steam by default but it was removed or later not needed
/// </summary>
/// <param name="">discarded</param>
/// <param name="call">the call type on the library</param>
/// <param name="">discarded</param>
/// <returns></returns>
int __stdcall DllMain(void*, DWORD call, void*)
{
    //check for process attach instead of 
    if (call == DLL_PROCESS_ATTACH)
    {
        //check if the remote debugger is enabled on steam, if not close
        if (!millennium::check_debugger())
        {
            MessageBoxA(GetForegroundWindow(), "Restart Steam", "Millennium", MB_ICONINFORMATION);
            void(__fastfail(false));
        }

        //otherwise start millennium
        CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(thread_callback_wrapper), NULL, NULL, NULL);
    }

    return true;
}

