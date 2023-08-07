#define WIN32_LEAN_AND_MEAN 
#define _WINSOCKAPI_   

#define millennium_entry_point int __stdcall
#pragma warning(disable: 6258)
#pragma warning(disable: 6387)

#include <stdafx.h>
#include <core/injector/event_handler.hpp>
#include <core/window/win_handler.hpp>

#include <Wininet.h>
#pragma comment(lib, "Wininet.lib")

/// <summary>
/// check for updates on the millennium client
/// updates are forced and the user has to do them or delete the client
/// </summary>
class auto_updater {
private:
    const char* repo_url = "https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest";
    const char* installer_url = "https://api.github.com/repos/ShadowMonster99/millennium-steam-patcher/releases/latest";

    struct installer {
        std::string installer_name;
        std::string cdn_filepath;
    } installer;

    std::string github_response, installer_response;
    HINTERNET request_buffer = NULL;
    HINTERNET connection = NULL;
public:
    /// <summary>
    /// create internet handle to connect to the GitHub repo
    /// </summary>
    auto_updater() {
        connection = InternetOpenA("millennium.updater", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);

        if (!connection) {
            InternetCloseHandle(connection);
        }
    }

    __declspec(noinline) std::string __fastcall get_millennium_installer(void) {
        request_buffer = InternetOpenUrlA(connection, installer_url, NULL, 0, INTERNET_FLAG_RELOAD, 0);

        const int buffer_size = 4096;
        char buffer[buffer_size] = {};
        unsigned long read = 0;

        while (InternetReadFile(request_buffer, buffer, sizeof(buffer), &read) && read > 0) installer_response.append(buffer, read);

        nlohmann::basic_json<> response = nlohmann::json::parse(installer_response);

        for (const auto& item : response["assets"])
        {
            if (item["name"].get<std::string>().find(".exe") == std::string::npos)
                continue;

            installer.installer_name = item["name"].get<std::string>();
            installer.cdn_filepath = item["browser_download_url"].get<std::string>();

            return item["browser_download_url"].get<std::string>();
        }
    }

    void download_installer_and_run(void)
    {
        HINTERNET hInternet = InternetOpenA("millennium.installer", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        HINTERNET hConnect = InternetOpenUrlA(hInternet, installer.cdn_filepath.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
        HANDLE hFile = CreateFileA(std::format("./{}", installer.installer_name).c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (!hConnect || !hInternet)
            return;

        if (hFile == INVALID_HANDLE_VALUE)
            return;

        constexpr int bufferSize = 4096;
        char buffer[bufferSize];
        DWORD bytesRead = 0;

        while (InternetReadFile(hConnect, buffer, bufferSize, &bytesRead) && bytesRead > 0)
        {
            DWORD bytesWritten;
            WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL);
        }

        CloseHandle(hFile);

        STARTUPINFOA startupInfo = { sizeof(startupInfo) };
        PROCESS_INFORMATION processInfo;

        CreateProcessA(installer.installer_name.c_str(), (LPSTR)"-update", NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);

        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
    }

    /// <summary>
    /// check update tag_name in the latest repo and check against the static version we have locally
    /// </summary>
    /// <param name=""></param>
    /// <returns></returns>
    __declspec(noinline) bool __fastcall update_required(void) {
        request_buffer = InternetOpenUrlA(connection, repo_url, NULL, 0, INTERNET_FLAG_RELOAD, 0);

        const int buffer_size = 4096;
        char buffer[buffer_size] = {};
        unsigned long read = 0;

        while (InternetReadFile(request_buffer, buffer, sizeof(buffer), &read) && read > 0) github_response.append(buffer, read);

        if (github_response.empty() || nlohmann::json::parse(github_response)["tag_name"].get<std::string>() == millennium_version) {
            return false;
        }
        return true;
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

/// <summary>
/// thread to display the console header
/// </summary>
/// <param name=""></param>
/// <returns></returns>
unsigned long long __stdcall console_header(void*)
{
    static void* cpuQuery, * cpuTotal;

    PdhOpenQueryA(NULL, NULL, &cpuQuery);
    PdhAddEnglishCounter(cpuQuery, (LPCSTR)"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
    PdhCollectQueryData(cpuQuery);

    while (true)
    {
        PDH_FMT_COUNTERVALUE counterVal;

        PdhCollectQueryData(cpuQuery);
        PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);

        SetConsoleTitleA((LPCSTR)(
            std::format(
                "millennium+prod.client-v{} [steam-ppid-cpu-usage:{}%]",
                millennium_version,
                (std::round(counterVal.doubleValue * 100) / 100)
            )
            ).c_str());

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

class main
{
private:
    auto_updater millennium_updater;
    main() {}
public:

    //create singleton instance of millennium
    static main& get_instance()
    {
        static main instance;
        return instance;
    }

    main(const main&) = delete;
    main& operator=(const main&) = delete;

    /// <summary>
    /// check if the cef remote debugging is enabled
    /// you can enable the debugging from the file name
    /// </summary>
    /// <param name=""></param>
    /// <returns></returns>
    static unsigned long __stdcall check_debugger(void)
    {
        constexpr const std::basic_string_view<char> filePath = ".cef-enable-remote-debugging";

        if (!std::ifstream(filePath.data()))
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
        //if (millennium_updater.update_required())
        //{
        //    millennium_updater.get_millennium_installer();
        //    millennium_updater.download_installer_and_run();

        //    return true;
        //}

        constexpr const std::basic_string_view<char> dev_cmd = "-dev";

        if (static_cast<std::string>(GetCommandLineA()).find(dev_cmd) != std::string::npos)
        {
            if (static_cast<bool>(AllocConsole()))
            {
                void(freopen("CONOUT$", "w", stdout));

                console.handle = GetStdHandle(STD_OUTPUT_HANDLE);
            }

            //CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(console_header), NULL, NULL, NULL);
        }

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
        return main::get_instance().bootstrap(lpParam);
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
millennium_entry_point DllMain(HINSTANCE hinstDLL, DWORD call, void*)
{
    //check for process attach instead of 
    if (call == DLL_PROCESS_ATTACH)
    {
        //check if the remote debugger is enabled on steam, if not close
        if (!main::check_debugger())
        {
            MessageBoxA(GetForegroundWindow(), "Restart Steam", "Millennium", MB_ICONINFORMATION);
            void(__fastfail(false));
        }

        //otherwise start millennium
        CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(thread_callback_wrapper), NULL, NULL, NULL);
    }

    return true;
}

