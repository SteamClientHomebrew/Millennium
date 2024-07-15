#pragma once
#include <sys/locals.h>
#include <cpr/cpr.h>
#include <sys/log.h>

const bool UpdatesEnabled()
{
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();

    const bool checkForUpdates = settingsStore->GetSetting("check_for_updates", "true") == "yes";

    return checkForUpdates;
}

const std::string GetLatestVersion()
{
    try 
    {
        const char* releaseUrl = "https://api.github.com/repos/SteamClientHomebrew/Millennium/releases";

        cpr::Response response = cpr::Get(cpr::Url{ releaseUrl }, cpr::UserAgent{"millennium.patcher/1.0"});

        if (response.error) 
        {
            throw std::runtime_error("Couldn't connect to server: " + std::string(releaseUrl));
        }

        nlohmann::json releaseData = nlohmann::json::parse(response.text);

        // find latest non-pre-release version
        for (const auto& release : releaseData)
        {
            if (release.contains("prerelease") && !release["prerelease"].get<bool>())
            {
                return release["tag_name"].get<std::string>();
            }
        }
    }
    catch (const nlohmann::json::exception& exception)
    {
        LOG_ERROR("An error occurred while getting the latest version of Millennium: {}", exception.what());
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR("An error occurred while getting the latest version of Millennium: {}", exception.what());
    }

    return MILLENNIUM_VERSION;
}

bool RunPowershellCommand(const std::wstring& command) 
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;
    ZeroMemory(&pi, sizeof(pi));

    // Create the command line for the process
    std::wstring cmdLine = L"cmd.exe /c powershell.exe -NoProfile -ExecutionPolicy Bypass \"" + command + L"\"";

    // Start the child process.
    if (!CreateProcess(NULL,   // No module name (use command line)
                    (LPWSTR)cmdLine.c_str(), // Command line
                    NULL,   // Process handle not inheritable
                    NULL,   // Thread handle not inheritable
                    FALSE,  // Set handle inheritance to FALSE
                    CREATE_NEW_CONSOLE,  // Create new console window
                    NULL,   // Use parent's environment block
                    NULL,   // Use parent's starting directory
                    &si,    // Pointer to STARTUPINFO structure
                    &pi))   // Pointer to PROCESS_INFORMATION structure
    {
        std::wcout << L"CreateProcess failed (" << GetLastError() << L").\n";
        return false;
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}

const void CheckForUpdates()
{
    if (UpdatesEnabled())
    {
        // Check for updates
        const std::string latestVersion = GetLatestVersion();

        if (latestVersion != MILLENNIUM_VERSION)
        {
            Logger.Warn("A new version of Millennium is available: {}", latestVersion);
            RunPowershellCommand(L"iwr -useb https://steambrew.app/update.ps1 | iex");
        }
        else
        {
            Logger.Log("Millennium is up to date.");
        }
    }
}