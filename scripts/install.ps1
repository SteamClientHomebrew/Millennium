# This file is part of the Millennium project.
# This script is used to install Millennium on a Windows machine.
# https://github.com/SteamClientHomebrew/Millennium/blob/main/scripts/install.ps1
# Copyright (c) 2024 Millennium

# Millennium artifacts repository
$apiUrl = "https://api.github.com/repos/SteamClientHomebrew/Millennium/releases"

# Define ANSI escape sequence for bold purple color
$BoldPurple = [char]27 + '[38;5;219m'
$BoldGreen = [char]27 + '[1;32m'
$BoldYellow = [char]27 + '[1;33m'
$BoldRed = [char]27 + '[1;31m'
$BoldLightBlue = [char]27 + '[38;5;75m'

$ResetColor = [char]27 + '[0m'

function Ask-Boolean-Question {
    param([bool]$newLine = $true, [string]$question, [bool]$default = $false)

    $choices = if ($default) { "[Y/n]" } else { "[y/N]" }
    $parsedQuestion = "${BoldPurple}::${ResetColor} $question $choices"
    $parsedQuestion = if ($newLine) { "`n$parsedQuestion" } else { $parsedQuestion }

    $choice = Read-Host "$parsedQuestion"

    if ($choice -eq "") {
        $choice = if ($default) { "y" } else { "n" }
    }

    if ($choice -eq "y" -or $choice -eq "yes") {
        $choice = "Yes"
    }
    else {
        $choice = "No"
    }

    [Console]::CursorTop -= if ($newLine) { 2 } else { 1 }
    [Console]::SetCursorPosition(0, [Console]::CursorTop)
    [Console]::Write(' ' * [Console]::WindowWidth)
    Write-Host "`r${parsedQuestion}: ${BoldLightBlue}$choice${ResetColor}"

    return $(if ($choice -eq "yes") { $true } else { $false })
}

function Close-SteamProcess {
    $steamProcess = Get-Process -Name "steam" -ErrorAction SilentlyContinue

    if ($steamProcess) {
        Stop-Process -Name "steam" -Force
        Write-Output "${BoldPurple}[+]${ResetColor} Closed Steam process."
    }
}

function Start-Steam {
    param([string]$steamPath)

    $steamExe = Join-Path -Path $steamPath -ChildPath "Steam.exe"
    if (-not (Test-Path -Path $steamExe)) {
        Write-Host "Steam executable not found."
        exit
    }

    Start-Process -FilePath $steamExe -ArgumentList "-verbose"
}

# Kill steam process before installing
Close-SteamProcess

function ConvertTo-ReadableSize {
    param([int64]$size)
    
    if ($size -eq 0) {
        return "0 Bytes"
    }

    $units = @("Bytes", "KiB", "MiB", "GiB")
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
    
    $FileCountMessage = "${BoldPurple}($FileNumber/$TotalFiles)${ResetColor}"
    $ProgressMessage  = "$FileCountMessage $Message"
    $currentDownload  = ConvertTo-ReadableSize -size $CurrentRead

    $placement = ([Console]::WindowWidth + 6) - $ProgressBarLength
    $addLength = $placement - $ProgressMessage.Length
    $spaces    = " " * $addLength
    $backspace = "`b" * $currentDownload.Length
    
    Write-Host -NoNewline "`r$ProgressMessage$spaces${backspace}${currentDownload} ${BoldLightBlue}[$ProgressBar]${ResetColor} $PercentComplete%"
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

$totalBytesFromRelease = ($latestRelease.assets | Measure-Object -Property size -Sum).Sum

function Calculate-Installed-Size {

    $installedSize = 0
    for ($i = 0; $i -lt $packageCount; $i++) {
        $asset = $latestRelease.assets[$i]
        $FilePath = Join-Path -Path $steamPath -ChildPath $asset.name
        
        if (Test-Path -Path $FilePath) {
            $installedSize += (Get-Item $FilePath).Length
        }
    }

    if ($installedSize -eq 0) {
        return -1
    }

    if ($totalBytesFromRelease - $installedSize -eq 0) {
        return "0 Bytes"
    }

    return ConvertTo-ReadableSize -size ($totalBytesFromRelease - $installedSize)
}

$releaseTag   = $latestRelease.tag_name
$packageCount = $latestRelease.assets.Count
Write-Output "`n${BoldPurple}[+]${ResetColor} Packages ($packageCount) ${BoldPurple}Millennium@$releaseTag${ResetColor}`n"

$totalSizeReadable = ConvertTo-ReadableSize -size $totalBytesFromRelease
$totalInstalledSize = Calculate-Installed-Size

Write-Output "${BoldPurple}${ResetColor} Total Download Size:   $totalSizeReadable"
Write-Output "${BoldPurple}${ResetColor} Total Installed Size:  $totalSizeReadable"

if ($totalInstalledSize -ne -1) {
    Write-Output "${BoldPurple}${ResetColor} Net Upgrade Size:      $totalInstalledSize"
}

# offer to proceed with installation
$result = Ask-Boolean-Question -question "Proceed with installation?" -default $true

if (-not $result) {
    Write-Output "${BoldPurple}[+]${ResetColor} Installation aborted."
    exit
}

Write-Host "${BoldPurple}::${ResetColor} Retreiving packages..."

$FileCount = $latestRelease.assets.Count

# Iterate through each asset and download it
for ($i = 0; $i -lt $FileCount; $i++) {

    $asset = $latestRelease.assets[$i]
    $downloadUrl = $asset.browser_download_url
    $outputFile = Join-Path -Path $steamPath -ChildPath $asset.name
    DownloadFileWithProgress -Url $downloadUrl -DestinationPath $outputFile -FileNumber ($i + 1) -TotalFiles $FileCount -FileName $asset.name
}


# This portion of the script is used to configure the millennium.ini file
# The script will prompt the user to enable these features.
Function Get-IniFile ($file)       # Based on "https://stackoverflow.com/a/422529"
 {
    $ini = [ordered]@{}
    $section = "NO_SECTION"
    $ini[$section] = [ordered]@{}

    switch -regex -file $file {    
        "^\[(.+)\]$" {
            $section = $matches[1].Trim()
            $ini[$section] = [ordered]@{}
        }
        "^\s*(.+?)\s*=\s*(.*)" {
            $name,$value = $matches[1..2]
            $ini[$section][$name] = $value.Trim()
        }
        default { $ini[$section]["<$("{0:d4}" -f $CommentCount++)>"] = $_ }
    }
    $ini
}

Function Set-IniFile ($iniObject, $Path, $PrintNoSection=$false, $PreserveNonData=$true)
{                                  # Based on "http://www.out-web.net/?p=109"
    $Content = @()
    ForEach ($Category in $iniObject.Keys) {
        if ( ($Category -notlike 'NO_SECTION') -or $PrintNoSection ) {
            $seperator = if ($Content[$Content.Count - 1] -eq "") {} else { "`n" }
            $Content += $seperator + "[$Category]";
        }

        ForEach ($Key in $iniObject.$Category.Keys) {           
            if ($Key.StartsWith('<')) {
                if ($PreserveNonData) {
                    $Content += $iniObject.$Category.$Key
                }
            }
            else {
                $Content += "$Key = " + $iniObject.$Category.$Key
            }
        }
    }
    $Content | Set-Content $Path -Force
}

$configPath = Join-Path -Path $steamPath -ChildPath "/ext/millennium.ini"

# check if the config file exists, if not, create it
if (-not (Test-Path -Path $configPath)) {
    New-Item -Path $configPath -ItemType File -Force > $null
}

Write-Host "`n`n${BoldPurple}::${ResetColor} Verifying post installation medium....`n"

$installedPackages = $latestRelease.assets | ForEach-Object { Join-Path -Path $steamPath -ChildPath $_.name }

$installedPackages | ForEach-Object -Begin { $i = 0 } {
    $i++
    $fileName = Split-Path -Path $_ -Leaf
    $fileNameWithoutExtension = [System.IO.Path]::GetFileNameWithoutExtension($fileName)
    
    $exists = if (Test-Path -Path $_) { "" } else { "${BoldRed}fail${ResetColor}" }
    Write-Output "${BoldPurple}($i/$FileCount)${ResetColor} verifying $fileNameWithoutExtension $exists"
}

# write millennium version and installed files to a log file
$millenniumVersion = $latestRelease.tag_name
$millenniumLog = Join-Path -Path $steamPath -ChildPath "/ext/data/logs/installer.log"

if (-not (Test-Path -Path $millenniumLog)) {
    New-Item -Path $millenniumLog -ItemType File -Force > $null
}

$millenniumLogContent = [PSCustomObject]@{
    version = $millenniumVersion
    assets = $installedPackages
}

$millenniumLogContent | ConvertTo-Json | Set-Content -Path $millenniumLog -Force

# Write-Host "
$iniObj = Get-IniFile $configPath
$result = Ask-Boolean-Question -question "Do you want to install developer packages?" -default $false

if (-not $iniObj.Contains("PackageManager")) { $iniObj["PackageManager"] = [ordered]@{} }
$iniObj["PackageManager"]["devtools"] = if ($result) { "yes" } else { "no" }


$result = Ask-Boolean-Question -question "Do you want Millennium to auto update? [Excluding Beta] (Recommeded)" -default $true -newLine $false

if (-not $iniObj.Contains("Settings")) { $iniObj["Settings"] = [ordered]@{} }
$iniObj["Settings"]["check_for_updates"] = if ($result) { "yes" } else { "no" }


Set-IniFile $iniObj $configPath -PreserveNonData $false
Start-Steam -steamPath $steamPath

$pipeName = "MillenniumStdoutPipe"
$bufferSize = 1024

$pipeServer = [System.IO.Pipes.NamedPipeServerStream]::new($pipeName, [System.IO.Pipes.PipeDirection]::In)

Write-Host ""

# [Console]::Write($BoldPurple)
# Write-Output ("-" * [Console]::WindowWidth)
Write-Output "${BoldPurple}::${ResetColor} Verifying runtime environment..."
Write-Output "${BoldPurple}++${ResetColor} If you face any issues while verifying, please report them on the GitHub repository."
Write-Output "${BoldPurple}++${ResetColor} ${BoldLightBlue}https://github.com/SteamClientHomebrew/Millennium/issues/new/choose${ResetColor}`n"
# Write-Output ("-" * [Console]::WindowWidth)
# [Console]::Write($ResetColor)

$pipeServer.WaitForConnection()
$buffer = New-Object byte[] $bufferSize

while ($pipeServer.IsConnected) {
    $bytesRead = $pipeServer.Read($buffer, 0, $bufferSize)
    if ($bytesRead -gt 0) {           
        $message = [System.Text.Encoding]::UTF8.GetString($buffer, 0, $bytesRead)
        [Console]::Write($message)
    }
}

Write-Output "Millennium Closed."
$pipeServer.Close()