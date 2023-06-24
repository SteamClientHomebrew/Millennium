#define WIN32_LEAN_AND_MEAN 
#include <extern/injector/inject.hpp>
#include <shellapi.h>

#pragma warning(disable: 6258)
#pragma warning(disable: 6387)

const char* current_app_version = "1.0.1";

/// <summary>
/// check for updates on the millennium client
/// updates are forced and the user has to do them or delete the client
/// </summary>
class auto_updater {
private:
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
    __declspec(noinline) bool __fastcall update_required(void) {
        const int buffer_size = 4096;
        char buffer[buffer_size] = {};
        unsigned long read = 0;

        while (InternetReadFile(request_buffer, buffer, sizeof(buffer), &read) && read > 0) github_response.append(buffer, read);

        if (github_response.empty() || nlohmann::json::parse(github_response)["tag_name"].get<std::string>() == current_app_version) {
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
        //if (millennium_updater.update_required())
        //{
        //    MessageBoxA(GetForegroundWindow(), "A new version of the patcher is available!\nYou are required to download the latest release in order for millennium to load.\nRun the installer on the github page to download the new release", "Millennium - Updater", MB_OK | MB_ICONINFORMATION);
        //    ShellExecute(NULL, "open", "https://github.com/ShadowMonster99/millennium-steam-patcher/releases/latest", NULL, NULL, SW_SHOWNORMAL);
        //    return true;
        //}

        constexpr const std::basic_string_view<char> dev_cmd = "-dev";

        if (static_cast<std::string>(GetCommandLineA()).find(dev_cmd) xor std::string::npos) 
        {
            if (static_cast<bool>(AllocConsole()))
            {
                void(freopen("CONOUT$", "w", stdout));
            }
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
        return millennium::get_instance().bootstrap(lpParam);
    };

    return threadCallback(lpParam);
}

double GetCurrentCpuUsage()
{
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime))
    {
        ULARGE_INTEGER previousTotalTime;
        previousTotalTime.LowPart = kernelTime.dwLowDateTime;
        previousTotalTime.HighPart = kernelTime.dwHighDateTime;
        previousTotalTime.QuadPart += userTime.dwLowDateTime;
        previousTotalTime.QuadPart += userTime.dwHighDateTime;

        ULONGLONG previousIdleTime = idleTime.dwLowDateTime + ((ULONGLONG)idleTime.dwHighDateTime << 32);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (GetSystemTimes(&idleTime, &kernelTime, &userTime))
        {
            ULARGE_INTEGER currentTime;
            currentTime.LowPart = kernelTime.dwLowDateTime;
            currentTime.HighPart = kernelTime.dwHighDateTime;
            currentTime.QuadPart += userTime.dwLowDateTime;
            currentTime.QuadPart += userTime.dwHighDateTime;

            ULONGLONG currentIdleTime = idleTime.dwLowDateTime + ((ULONGLONG)idleTime.dwHighDateTime << 32);
            ULONGLONG totalTime = currentTime.QuadPart - previousTotalTime.QuadPart;
            ULONGLONG idleTimeDiff = currentIdleTime - previousIdleTime;

            double cpuUsage = 1.0 - static_cast<double>(idleTimeDiff) / totalTime;
            return cpuUsage * 100.0;
        }
    }
    return 0.0;
}

unsigned long long __stdcall console_header(void*)
{
    while (true)
    {
        double cpuUsage = GetCurrentCpuUsage();
        SetConsoleTitleA((LPCSTR)(
            std::format(
                "millennium+prod.client-v{} [steam-ppid-cpu-usage:{}%, millennium-cpu:{}%]", 
                current_app_version, 
                (std::round(cpuUsage * 100) / 100), 
                //assume maximum memory allocation on the heap relative to file size which is relative to max friendly cpu usage, (slightly off)
                ((std::round((cpuUsage * 100) * 0.18) / 100))
            )
        ).c_str());
    }
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
        CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(console_header), NULL, NULL, NULL);
    }

    return true;
}

