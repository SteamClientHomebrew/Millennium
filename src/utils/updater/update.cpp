#include "update.hpp"

#include <stdafx.h>
#include <string_view>
#include <iostream>
#include <filesystem>

#include <utils/config/config.hpp>

std::string get_bash(std::string url)
{
    return R"(
@echo off
title Updating Millennium Patcher...
echo Starting update...
setlocal enabledelayedexpansion
echo Closing Steam...
taskkill /F /IM steam.exe
echo Steam closed.
echo Downloading files from GitHub...
set "releaseUrl=)"+ url +R"("

for /f "delims=" %%i in ('powershell -Command "(Invoke-RestMethod '!releaseUrl!').assets | ForEach-Object { $_.browser_download_url }"') do (
    set "downloadUrl=%%i"
    for %%A in (!downloadUrl!) do set "filename=%%~nxA"
    echo Downloading: "%CD%\!filename!"
    powershell -Command "Invoke-WebRequest -Uri '!downloadUrl!' -OutFile '%CD%\!filename!'"
)
echo Files downloaded.
echo Starting Steam...
start steam.exe
echo Steam started.
endlocal
)";
}

void updater::start_update(std::string url)
{
#ifdef _WIN32
    std::filesystem::path fileName = fmt::format("{}/.millennium/bootstrapper/updater.bat", config.getSteamRoot());

    std::cout << fileName << std::endl;

    try {
        std::filesystem::create_directories(fileName.parent_path());
    }
    catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    std::ofstream bashFile(fileName.string());
    bashFile << get_bash(url);
    bashFile.close();

    console.log("Starting updater.bat");

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Replace "compile_and_run.bat" with the name of your batch file
    if (!CreateProcessA(NULL, (LPSTR)fileName.string().c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcess failed (%d).\n", GetLastError());
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#elif __linux__
    console.err("updater::start_update() DOES NOT HAVE IMPLEMENTATION");
#endif
}


void updater::check_for_updates()
{
    try
    {
        nlohmann::basic_json<> api = 
            nlohmann::json::parse(http::get("https://millennium.web.app/api/updater"));

        if (api["up"] != true)
        {
            std::string message = api["message"];

            auto selection = msg::show(message.c_str(), "Bootstrap Error", Buttons::OK);
            return;
        }

        std::string url = api["url"];
        console.log("checking for updates from: " + url);

        nlohmann::basic_json<> response = nlohmann::json::parse(http::get(url));

        if (response.contains("message")) {
            throw http_error(http_error::errors::not_allowed, "request is not allowed");
        }

        if (response["tag_name"] != m_ver)
        {
            console.log("Starting updater...");
            updater::start_update(url);
        }
    }
    catch (nlohmann::detail::exception& ex)
    {
        console.err(fmt::format("JSON error occurred while checking for updates. {}", ex.what()));
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
            default: {
                console.err("Misc error trying to update Millennium.");
                break;
            }
        }
    }
}
