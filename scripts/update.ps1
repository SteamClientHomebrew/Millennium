# This file is part of the Millennium project.
# This script is used to update Millennium on a Windows machine.
# https://github.com/SteamClientHomebrew/Millennium/blob/main/scripts/update.ps1
# Copyright (c) 2024 Millennium

# Millennium artifacts repository
$apiUrl = "https://api.github.com/repos/SteamClientHomebrew/Millennium/releases"

function Close-SteamProcess {
    $steamProcess = Get-Process -Name "steam" -ErrorAction SilentlyContinue

    if ($steamProcess) {
        Stop-Process -Name "steam" -Force
    }
}

function Start-Steam {
    param([string]$steamPath)

    $steamExe = Join-Path -Path $steamPath -ChildPath "Steam.exe"
    if (-not (Test-Path -Path $steamExe)) {
        exit
    }

    Start-Process -FilePath $steamExe
}

# Kill steam process before installing
Close-SteamProcess

function DownloadFileWithProgress {
    param ([string]$Url, [string]$DestinationPath, [int]$FileNumber, [int]$TotalFiles, [string]$FileName)

    try {
        $webRequest = [System.Net.HttpWebRequest]::Create($Url)
        $webRequest.Method = "GET"
        $webRequest.UserAgent = "Millennium.Installer/1.0"

        $response = $webRequest.GetResponse()
        $responseStream = $response.GetResponseStream()

        $fileStream = New-Object System.IO.FileStream($DestinationPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
        $buffer = New-Object byte[] 8192
        $bytesRead = 0

        while (($bytesRead = $responseStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            $fileStream.Write($buffer, 0, $bytesRead)
        }

        $fileStream.Close()
        $responseStream.Close()
    }
    catch {
        Write-Host "Error downloading file: $_"
        if ($fileStream) { $fileStream.Close() }
        if ($responseStream) { $responseStream.Close() }
    }
}

$response = Invoke-RestMethod -Uri $apiUrl -Headers @{ "User-Agent" = "Millennium.Installer/1.0" }
$latestRelease = $response | Where-Object { -not $_.prerelease } | Sort-Object -Property created_at -Descending | Select-Object -First 1

$steamPath = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam").SteamPath

$latestVersion = $latestRelease.tag_name

Write-Host "Downloading Millennium $latestVersion..."

$FileCount = $latestRelease.assets.Count
for ($i = 0; $i -lt $FileCount; $i++) {

    $asset = $latestRelease.assets[$i]
    $downloadUrl = $asset.browser_download_url
    $outputFile = Join-Path -Path $steamPath -ChildPath $asset.name
    DownloadFileWithProgress -Url $downloadUrl -DestinationPath $outputFile -FileNumber ($i + 1) -TotalFiles $FileCount -FileName $asset.name
}
Start-Steam -steamPath $steamPath