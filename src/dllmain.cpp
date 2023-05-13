#define WIN32_LEAN_AND_MEAN 
#include <extern/injector/inject.hpp>
#include <shellapi.h>
#include <filesystem>

#pragma warning(disable: 6258)

Console Out;

const char* current_app_version = "public3";
const char* repo_url = "https://api.github.com/repos/ShadowMonster99/millennium-steam-patcher/releases/latest";

class auto_updater {
private:
    char buffer[4096] = {};
    DWORD total_bytes_read = {};
    std::basic_string<char, std::char_traits<char>, std::allocator<char>> github_response;

    HINTERNET request_buffer, connection;
public:
    auto_updater()
    {
        connection = InternetOpenA("millennium.updater", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        request_buffer = InternetOpenUrlA(connection, repo_url, NULL, 0, INTERNET_FLAG_RELOAD, 0);

        if (!request_buffer || !connection) {
            std::cout << "Error opening URL: " << GetLastError() << std::endl;
            InternetCloseHandle(connection);
            return;
        }
    }

    inline void check_for_updates()
    {
        while (InternetReadFile(request_buffer, buffer, sizeof(buffer), &total_bytes_read) && total_bytes_read > 0) { github_response.append(buffer, total_bytes_read); }

        //no internet connection most likely
        if (github_response.empty()) {
            return;
        }

        if (nlohmann::json::parse(github_response)["tag_name"].get<std::string>() == current_app_version)
        {
            return;
        }

        //millennium needs an update

        MessageBoxA(
            GetForegroundWindow(),
            "New version of the patcher is available!\nYou are required to download the latest release, or you can uninstall the current release you have",
            "Millennium - Updater",
            MB_OK | MB_ICONINFORMATION);

        const char* url = "https://github.com/ShadowMonster99/millennium-steam-patcher/releases/latest";
        ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);

        exit(1);
    }

    ~auto_updater()
    {
        InternetCloseHandle(request_buffer);
        InternetCloseHandle(connection);
    }
};

class Millennium
{
private:
    auto_updater millennium_updater;
public:
    inline DWORD WINAPI Start(LPVOID lpParam)
    {
        std::string param = std::string(GetCommandLine()).find("-cef-enable-debugging") == std::string::npos ? " -cef-enable-debugging" : "";

        //store current parameters
        std::ofstream outfile("restart.bat");
        outfile << R"(
                @echo off
                :LOOP
                tasklist | find "steam.exe" > nul
                if not errorlevel 1 (
                    ping -n 1 127.0.0.1 > nul
                    goto LOOP
                )
                start "" )" + std::string(GetCommandLine()) + param +  R"(
                exit
            )";
        outfile.close();

        //if steam isnt started with the right parameters, restart it and append the argument to the list
        if (std::string(GetCommandLine()).find("-cef-enable-debugging") == std::string::npos) 
        {
            ShellExecuteA(NULL, "open", "restart.bat", NULL, NULL, SW_HIDE);
            __fastfail(0);
        }

        //millennium_updater.check_for_updates();

        (HANDLE)CreateThread(NULL, NULL, Initialize, NULL, NULL, NULL);
        return true;
    }
};

DWORD __stdcall Bootstrap(LPVOID lpParam)
{
    Millennium bootstrapper;
    bootstrapper.Start(lpParam);
    
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  call, LPVOID lpReserved)
{
    if (call == DLL_PROCESS_ATTACH) { CreateThread(NULL, NULL, Bootstrap, NULL, NULL, NULL); }
    return true;
}

