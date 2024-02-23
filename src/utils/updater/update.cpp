#include "update.hpp"

#include <stdafx.h>
#include <string_view>
#include <iostream>
#include <filesystem>

const constexpr std::string_view bash = R"(
@echo off
title Updating Millennium Patcher...
echo Starting update...
setlocal enabledelayedexpansion
echo Closing Steam...
taskkill /F /IM steam.exe
echo Steam closed.
echo Downloading files from GitHub...
set "releaseUrl=https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest"

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

void updater::start_update()
{
    std::filesystem::path fileName = ".millennium/bootstrapper/updater.bat";

    try {
        std::filesystem::create_directories(fileName.parent_path());
    }
    catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    std::ofstream bashFile(fileName.string());
    bashFile << bash.data();
    bashFile.close();

    //ShellExecute(0, "open", "update.bat", 0, 0, SW_SHOW);
}


void updater::check_for_updates()
{
    try
    {
        updater::start_update();

        nlohmann::basic_json<> response = nlohmann::json::parse(http::get(repo));

        if (response.contains("message")) {
            throw http_error(http_error::errors::not_allowed);
        }

        if (response["tag_name"] != m_ver)
        {
            console.log("Starting updater...");
            updater::start_update();
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
            default: {
                console.err("Misc error trying to update Millennium.");
                break;
            }
        }
    }
}
