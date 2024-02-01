#include "update.hpp"

#include <stdafx.h>
#include <string_view>

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
    std::ofstream bashFile("update.bat");
    bashFile << bash.data();
    bashFile.close();

    //run the bash file with no window
    ShellExecute(0, "open", "update.bat", 0, 0, SW_SHOW);
}