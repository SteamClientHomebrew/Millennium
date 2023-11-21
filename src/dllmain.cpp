#define WIN32_LEAN_AND_MEAN 
#define _WINSOCKAPI_   

#include <stdafx.h>
#include <core/injector/event_handler.hpp>

#include <core/steam/application.hpp>
#include <window/core/window.hpp>

#include <utils/thread/thread_handler.hpp>
#include <Psapi.h>
#include <pdh.h>

#pragma comment(lib, "pdh.lib")

HMODULE hCurrentModule = nullptr;

FILETIME prevSysIdle, prevSysKernel, prevSysUser;

// TIME DIFF FUNC
ULONGLONG SubtractTimes(const FILETIME one, const FILETIME two)
{
    LARGE_INTEGER a, b;
    a.LowPart = one.dwLowDateTime;
    a.HighPart = one.dwHighDateTime;

    b.LowPart = two.dwLowDateTime;
    b.HighPart = two.dwHighDateTime;

    return a.QuadPart - b.QuadPart;
}

int getUsage(double& val)
{
    FILETIME sysIdle, sysKernel, sysUser;
    // sysKernel include IdleTime
    if (GetSystemTimes(&sysIdle, &sysKernel, &sysUser) == 0) // GetSystemTimes func FAILED return value is zero;
        return 0;

    if (prevSysIdle.dwLowDateTime != 0 && prevSysIdle.dwHighDateTime != 0)
    {
        ULONGLONG sysIdleDiff, sysKernelDiff, sysUserDiff;
        sysIdleDiff = SubtractTimes(sysIdle, prevSysIdle);
        sysKernelDiff = SubtractTimes(sysKernel, prevSysKernel);
        sysUserDiff = SubtractTimes(sysUser, prevSysUser);

        ULONGLONG sysTotal = sysKernelDiff + sysUserDiff;
        ULONGLONG kernelTotal = sysKernelDiff - sysIdleDiff; // kernelTime - IdleTime = kernelTime, because sysKernel include IdleTime

        if (sysTotal > 0) // sometimes kernelTime > idleTime
            val = (double)(((kernelTotal + sysUserDiff) * 100.0) / sysTotal);
    }

    prevSysIdle = sysIdle;
    prevSysKernel = sysKernel;
    prevSysUser = sysUser;

    return 1;
}

namespace Millennium
{
    unsigned long __stdcall checkRemoteDebugging(void)
    {
        constexpr const std::basic_string_view<char> filePath = ".cef-enable-remote-debugging";

        if (!std::ifstream(filePath.data()))
        {
            std::ofstream(filePath.data()).close();
            return false;
        }

        return true;
    }

    unsigned long __stdcall getConsoleHeader(void* lpParam) {

        while (true) {
            PROCESS_MEMORY_COUNTERS_EX pmc;
            HANDLE hProcess = GetCurrentProcess();

            double cpuLoad;
            getUsage(cpuLoad);

            if (GetProcessMemoryInfo(hProcess, (PPROCESS_MEMORY_COUNTERS)&pmc, sizeof(pmc))) {
                SIZE_T privateBytes = pmc.PrivateUsage;
                double privateMB = static_cast<double>(privateBytes) / (1024 * 1024);

                SetConsoleTitle(std::format("Millennium.Patcher-v{} [Threads: {}, Processor: {}%, Private-Bytes {} MB]", 
                    m_ver, 
                    threadContainer::getInstance().runningThreads.size(), 
                    std::round(cpuLoad * 100.0) / 100.0,
                    std::round(privateMB * 100.0) / 100.0
                ).c_str());
            }
            else {
                std::cout << "Error: GetProcessMemoryInfo failed." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    unsigned long __stdcall Bootstrap(void* lpParam)
    {
        if (steam::get().params.has("-dev"))
        {
            if (static_cast<bool>(AllocConsole()))
            {
                void(freopen("CONOUT$", "w", stdout));
                console.consoleAllocated = true;
            }
        }

        try
        {
            nlohmann::basic_json<> response = nlohmann::json::parse(http::get(repo));

            if (response.contains("message")) {
                throw http_error(http_error::errors::not_allowed);
            }

            if (response["tag_name"] != m_ver)
            {
                const char* url = "https://github.com/ShadowMonster99/millennium-steam-patcher/releases/latest/download/Millennium.Installer-Windows.exe";

                try {
                    std::vector<unsigned char> result = http::get_bytes(url);

                    const char* filePath = "millennium.installer.exe";

                    std::ofstream outputFile(filePath, std::ios::binary);
                    outputFile.write(reinterpret_cast<const char*>(result.data()), result.size());
                    outputFile.close();

                    ShellExecute(NULL, "open", "millennium.installer.exe", NULL, NULL, SW_SHOWDEFAULT);

                }
                catch (const std::exception& e) {
                    MsgBox(std::format("Can't update millennium! Ask support for help.\n\nTrace:\n{}", e.what()).c_str(), "Error", MB_ICONERROR);
                }

                ExitProcess(EXIT_SUCCESS);
            }
        }
        catch (const http_error& error)
        {
            switch (error.code())
            {
                case http_error::errors::couldnt_connect: {
                    console.err("Couldn't make a GET request to GitHub to retrieve update information!");
                    break;
                }
                case http_error::errors::not_allowed: {
                    console.err("Networking disabled. Purged update request.");
                    break;
                }
            }
        }

        std::cout << "Millennium Dev Tools v" << m_ver << std::endl;
        std::cout << "Developed by ShadowMonster (discord: sm74)" << std::endl;
        std::cout << "Need help? Check the docs: https://millennium.gitbook.io/steam-patcher/guides/developers" << std::endl;
        std::cout << "Still need help? ask the discord server: https://discord.gg/MXMWEQKgJF, or DM me if you really cant figure it out" << std::endl;
        std::cout << "-------------------------------------------------------------------------------------\n" << std::endl;

        threadContainer::getInstance().addThread(CreateThread(0, 0, getConsoleHeader, 0, 0, 0));
        threadContainer::getInstance().addThread(CreateThread(0, 0, Initialize, 0, 0, 0));

        return true;
    }
}

int __stdcall DllMain(HINSTANCE hinstDLL, DWORD call, void*)
{
    if (call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinstDLL);
        hCurrentModule = hinstDLL;

        if (!Millennium::checkRemoteDebugging())
        {
            MsgBox("Initialization complete! Restart Steam for changed to take effect.", "Millennium", MB_ICONINFORMATION);
            ExitProcess(EXIT_SUCCESS);
        }

        threadContainer::getInstance().addThread(CreateThread(0, 0, Millennium::Bootstrap, 0, 0, 0));
    }

    return true;
}

