#include <iostream>
#include <Windows.h>
#include <Psapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "Psapi.lib")

static bool install_success = true;

std::string get_steam_install_path()
{
    std::string steamPath;
    HKEY hKey;
    LPCSTR subKey = "Software\\Valve\\Steam";

    if (RegOpenKeyExA(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        CHAR value[MAX_PATH];
        DWORD bufferSize = sizeof(value);

        if (RegGetValueA(hKey, nullptr, "SteamPath", RRF_RT_REG_SZ, nullptr, value, &bufferSize) == ERROR_SUCCESS)
        {
            steamPath = value;
        }

        RegCloseKey(hKey);
    }

    return steamPath;
}

void install_millennium()
{
    std::string processName = "Steam";
    std::vector<DWORD> pids;
    DWORD pidArraySize = 1024;
    DWORD bytesReturned;
    DWORD* processIds = new DWORD[pidArraySize / sizeof(DWORD)];

    if (EnumProcesses(processIds, pidArraySize, &bytesReturned))
    {
        DWORD processCount = bytesReturned / sizeof(DWORD);

        for (DWORD i = 0; i < processCount; i++)
        {
            if (processIds[i] != 0)
            {
                char processNameBuffer[MAX_PATH];
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processIds[i]);

                if (hProcess != nullptr)
                {
                    if (GetModuleBaseNameA(hProcess, nullptr, processNameBuffer, sizeof(processNameBuffer)) > 0)
                    {
                        std::string runningProcessName(processNameBuffer);

                        if (runningProcessName == processName)
                        {
                            std::cout << " [WARNING] Process '" + processName + "' is running. Close it to continue" << std::endl;
                            delete[] processIds;
                            return;
                        }
                    }
                }
            }
        }
    }

    delete[] processIds;

    char currentDirectory[MAX_PATH];
    if (GetCurrentDirectoryA(MAX_PATH, currentDirectory) != 0)
    {
        std::string steamPath = get_steam_install_path();

        if (!steamPath.empty())
        {
            std::vector<std::string> filesToCopy = { "user32.dll", "libcrypto-3.dll" };

            for (const std::string& fileName : filesToCopy)
            {
                std::string sourceFilePath = std::string(currentDirectory) + "\\" + fileName;
                std::string destinationFilePath = steamPath + "\\" + fileName;

                if (CopyFileA(sourceFilePath.c_str(), destinationFilePath.c_str(), false))
                {
                    std::cout << " [SUCCESS] File '" + fileName + "' copied successfully." << std::endl;
                }
                else
                {
                    install_success = false;
                    DWORD errorCode = GetLastError();
                    std::string errorMessage = "An error occurred while copying '" + fileName + "': " + std::to_string(errorCode);
                    std::cout << " [FATAL] " << errorMessage << std::endl;
                }
            }
        }
        else
        {
            install_success = false;
            std::cout << " [FATAL] Steam path not found." << std::endl;
        }
    }
}

int main()
{
    SetConsoleTitleA("Millennium Installer");

    std::cout << " [INFO] Starting installer..." << std::endl;
    install_millennium();

    if (install_success)
    {
        std::cout << " [SUCCESS] Successfully installed Millennium on your computer!" << std::endl;
        std::cout << " [INFO] Start steam and go to the Millennium tab to get started" << std::endl;
        std::cout << " [INFO] You can now close this window..." << std::endl;
    }
    else
    {
        std::cout << " [FATAL] Install failed, refer to the errors above." << std::endl;
    }

    getchar();
}
