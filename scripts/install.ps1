# This file is part of the Millennium project.
# This script is used to install Millennium on a Windows machine.
# https://github.com/SteamClientHomebrew/Millennium/blob/main/scripts/install.ps1
# Copyright (c) 2024 Millennium

# Define ANSI escape sequence for bold purple color
$BoldPurple = [char]27 + '[38;5;219m'
$BoldGreen = [char]27 + '[1;32m'
$BoldLightBlue = [char]27 + '[38;5;75m'

$ResetColor = [char]27 + '[0m'

function Close-SteamProcess {
    $steamProcess = Get-Process -Name "steam" -ErrorAction SilentlyContinue

    if ($steamProcess) {
        Stop-Process -Name "steam" -Force
        Write-Output "${BoldPurple}[+]${ResetColor} Closed Steam process."
    }
}

# Kill steam process before installing
Close-SteamProcess

function ConvertTo-ReadableSize {
    param([int64]$size)
    
    $units = @("B", "KiB", "MiB", "GiB")
    $index = [Math]::Floor([Math]::Log($size, 1024))
    $sizeFormatted = [Math]::Round($size / [Math]::Pow(1024, $index), 2, [MidpointRounding]::AwayFromZero)
    
    return "$sizeFormatted $($units[$index])"
}

function Update-Config {
    param([string]$JsonFilePath, [string]$Key, $Value)

    $directory = Split-Path -Path $JsonFilePath
    if (-not (Test-Path $directory)) {
        New-Item -Path $directory -ItemType Directory -Force -ErrorAction Stop | Out-Null
    }
    if (-not (Test-Path $JsonFilePath) -or (Get-Content $JsonFilePath -Raw) -eq '') {
        @{} | ConvertTo-Json | Set-Content -Path $JsonFilePath -Force -ErrorAction Stop
    }
    $jsonContent = Get-Content -Path $JsonFilePath -Raw | ConvertFrom-Json -ErrorAction Stop

    try {
        $jsonContent | Add-Member -MemberType NoteProperty -Name $Key -Value $Value -ErrorAction Stop
    }
    catch {
        $jsonContent.$Key = $Value
    }
    ($jsonContent | ConvertTo-Json -Depth 6).Replace('  ',' ') | Out-File $JsonFilePath -Force -ErrorAction Stop
}

function Show-Progress {
    param ([int]$FileNumber, [int]$TotalFiles, [int]$PercentComplete, [string]$Message, [int]$CurrentRead, [int]$TotalBytes)

    $ProgressBarLength = 65
    $Progress = [math]::round(($PercentComplete / 100) * $ProgressBarLength)
    $ProgressBar = ("#" * $Progress).PadRight($ProgressBarLength)
    
    $FileCountMessage = "${BoldPurple}[$FileNumber/$TotalFiles]${ResetColor}"
    $ProgressMessage  = "$FileCountMessage $Message"
    $currentDownload  = ConvertTo-ReadableSize -size $CurrentRead

    $placement = ([Console]::WindowWidth + 6) - $ProgressBarLength
    $addLength = $placement - $ProgressMessage.Length
    $spaces    = " " * $addLength
    $backspace = "`b" * $currentDownload.Length
    
    Write-Host -NoNewline "`r$ProgressMessage$spaces${BoldGreen}${backspace}${currentDownload} [$ProgressBar]${ResetColor} $PercentComplete%"
}

function DownloadFileWithProgress {
    param ([string]$Url, [string]$DestinationPath, [int]$FileNumber, [int]$TotalFiles, [string]$FileName)

    try {
        $webRequest = [System.Net.HttpWebRequest]::Create($Url)
        $webRequest.Method = "GET"
        $webRequest.UserAgent = "Millennium.Installer/1.0"

        $response = $webRequest.GetResponse()
        $totalBytes = $response.ContentLength
        $responseStream = $response.GetResponseStream()

        $fileStream = New-Object System.IO.FileStream($DestinationPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
        $buffer = New-Object byte[] 8192
        $bytesRead = 0
        $totalBytesRead = 0

        Write-Host "`r"

        while (($bytesRead = $responseStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            $fileStream.Write($buffer, 0, $bytesRead)
            $totalBytesRead += $bytesRead
            $percentComplete = [math]::round(($totalBytesRead / $totalBytes) * 100)
            Show-Progress -FileNumber $FileNumber -TotalFiles $TotalFiles -PercentComplete $percentComplete -Message $FileName -CurrentRead $totalBytesRead -TotalBytes $totalBytes
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

$apiUrl = "https://api.github.com/repos/SteamClientHomebrew/Millennium/releases"
$response = Invoke-RestMethod -Uri $apiUrl -Headers @{ "User-Agent" = "Millennium.Installer/1.0" }
$latestRelease = $response | Where-Object { -not $_.prerelease } | Sort-Object -Property created_at -Descending | Select-Object -First 1

$customSteamPath = Read-Host "${BoldPurple}[?]${ResetColor} Steam Path (leave blank for default)"

if (-not $customSteamPath) {
    $steamPath = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam").SteamPath

    if (-not $steamPath) {
        Write-Host "Steam path not found in registry."
        exit
    } 
    [Console]::CursorTop -= 1
    [Console]::SetCursorPosition(0, [Console]::CursorTop)
    [Console]::Write(' ' * [Console]::WindowWidth)

    Write-Output "`r${BoldPurple}[?]${ResetColor} Steam Path (leave blank for default): ${BoldLightBlue}$steamPath${ResetColor}"
} 
else {
    $steamPath = $customSteamPath
}

# Ensure the destination directory exists
if (-not (Test-Path -Path $steamPath)) {
    New-Item -ItemType Directory -Path $steamPath
}

$releaseTag   = $latestRelease.tag_name
$packageCount = $latestRelease.assets.Count
Write-Output "`n${BoldPurple}[+]${ResetColor} Packages ($packageCount) ${BoldPurple}Millennium@$releaseTag${ResetColor}`n"

$totalSizeReadable = ConvertTo-ReadableSize -size ($latestRelease.assets | Measure-Object -Property size -Sum).Sum
Write-Output "${BoldPurple}[+]${ResetColor} Total Download Size:  $totalSizeReadable"
Write-Output "${BoldPurple}[+]${ResetColor} Total Installed Size: $totalSizeReadable"

# offer to proceed with installation
$choice = Read-Host "`n${BoldPurple}::${ResetColor} Proceed with installation? [Y/n]"

if ($choice -eq "n" -or $choice -eq "no") {
    Write-Output "${BoldPurple}[+]${ResetColor} Installation aborted."
    exit
}

Write-Host "${BoldPurple}::${ResetColor} Retreiving packages..."

$FileCount = $latestRelease.assets.Count
for ($i = 0; $i -lt $FileCount; $i++) {

    $asset = $latestRelease.assets[$i]
    $downloadUrl = $asset.browser_download_url
    $outputFile = Join-Path -Path $steamPath -ChildPath $asset.name
    DownloadFileWithProgress -Url $downloadUrl -DestinationPath $outputFile -FileNumber ($i + 1) -TotalFiles $FileCount -FileName $asset.name
}

Write-Host "`n"

# future options net yet available

# $choice = Read-Host "`n${BoldPurple}[?]${ResetColor} Do you want to install developer packages? [y/N]"
# if ($choice -eq "y" -or $choice -eq "yes") {
#     Write-Host "yes"
# } 

# $choice = Read-Host "${BoldPurple}[?]${ResetColor} Do you want Millennium to auto update? (Recommeded) [Y/n]"
# if ($choice -ne "n" -or $choice -ne "no") {
#     Update-Config -Key "autoUpdate" -Value $true -JsonFilePath (Join-Path -Path $steamPath -ChildPath "/ext1/plugins.json")
# }

# $choice = Read-Host "${BoldPurple}[?]${ResetColor} Do you want to use Millennium's GUI? (no = configure millennium with files only) [Y/n]"
# if ($choice -ne "n" -or $choice -ne "no") {
#     Update-Config -Key "gui" -Value $true -JsonFilePath (Join-Path -Path $steamPath -ChildPath "/ext1/plugins.json")
# } 