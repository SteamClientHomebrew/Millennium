#pragma once
#include <sys/locals.h>
#include <sys/log.h>

const bool UpdatesEnabled()
{
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();

    const bool checkForUpdates = settingsStore->GetSetting("check_for_updates", "yes") == "yes";

    return checkForUpdates;
}

const std::string GetLatestVersion()
{
    try 
    {
        const char* releaseUrl = "https://api.github.com/repos/SteamClientHomebrew/Millennium/releases";
        std::string githubResponse = Http::Get(releaseUrl, false);

        nlohmann::json releaseData = nlohmann::json::parse(githubResponse);

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

#ifdef _WIN32
bool RunPowershellCommand(const std::wstring& command) 
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring cmdLine = L"cmd.exe /c powershell.exe -NoProfile -ExecutionPolicy Bypass \"" + command + L"\"";

    if (!CreateProcess(NULL, (LPWSTR)cmdLine.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        std::wcout << L"CreateProcess failed (" << GetLastError() << L").\n";
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}
#endif

const void CheckForUpdates()
{
    if (UpdatesEnabled())
    {
        // Check for updates
        const std::string latestVersion = GetLatestVersion();

        if (latestVersion != MILLENNIUM_VERSION)
        {
            Logger.Warn("Upgrading Millennium@{} -> Millennium@{}", MILLENNIUM_VERSION, latestVersion);
            #ifdef _WIN32
            {
                RunPowershellCommand(L"iwr -useb https://steambrew.app/update.ps1 | iex");
            }
            #endif
        }
        else
        {
            Logger.Log("Millennium@{} is up to date.", MILLENNIUM_VERSION);
        }
    }
}