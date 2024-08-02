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

$isUpdaterDefined = Test-Path Variable:\UpdaterStatus
$isUpdaterTrue = $true -eq $UpdaterStatus

$isUpdater = if ($isUpdaterDefined -and $isUpdaterTrue) { $true } else { $false }

if ($isUpdater) {
    Write-Output "${BoldPurple}++${ResetColor} Updating Millennium..."
}

Add-Type -AssemblyName System.IO.Compression.FileSystem

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
        Write-Output "${BoldPurple}++${ResetColor} Closed Steam process."
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

    $ProgressBarLength = [math]::round([System.Console]::WindowWidth / 4)
    $Progress = [math]::round(($PercentComplete / 100) * $ProgressBarLength)
    $ProgressBar = ("#" * $Progress).PadRight($ProgressBarLength)
    
    $FileCountMessage = "${BoldPurple}($FileNumber/$TotalFiles)${ResetColor}"
    $ProgressMessage  = "$FileCountMessage $Message"
    $currentDownload  = ConvertTo-ReadableSize -size $CurrentRead

    if ($currentDownload -eq "0 Bytes") {
        $currentDownload = ""
    }

    $placement = ([Console]::WindowWidth + 6) - $ProgressBarLength
    $addLength = $placement - $ProgressMessage.Length

    # check if addLength is negative
    if ($addLength -lt 0) {
        $addLength = $ProgressMessage.Length
    }

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
$latestRelease = $response | Where-Object { $_.prerelease } | Sort-Object -Property created_at -Descending | Select-Object -First 1

$steamPath = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam").SteamPath

if (-not $steamPath) {
    Write-Host "Steam path not found in registry."
    exit
} 

# Ensure the destination directory exists
if (-not (Test-Path -Path $steamPath)) {
    New-Item -ItemType Directory -Path $steamPath
}

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
$packageName = "millennium-$releaseTag-windows-x86_64.zip"

# Find the size of the package from its name
$packageCount = $latestRelease.assets.Count
$targetAsset = $latestRelease.assets | Where-Object { $_.name -eq $packageName }

$totalBytesFromRelease = $totalBytesFromRelease = $targetAsset.size


Write-Output "${BoldPurple}++${ResetColor} Installing Packages (Windows) ${BoldPurple}Millennium@$releaseTag${ResetColor}`n"

$totalSizeReadable = ConvertTo-ReadableSize -size $totalBytesFromRelease
$totalInstalledSize = Calculate-Installed-Size

Write-Output "${BoldPurple}::${ResetColor} Total Download Size:   $totalSizeReadable"
Write-Host "${BoldPurple}::${ResetColor} Retreiving packages..."

$FileCount = $latestRelease.assets.Count

function Extract-ZipWithProgress {
    param (
        [string]$zipPath,
        [string]$extractPath
    )

    # Open the ZIP file
    $zipArchive = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
    $totalFiles = $zipArchive.Entries.Count
    $currentFile = 0

    foreach ($entry in $zipArchive.Entries) {
        if (-not [string]::IsNullOrEmpty($entry.FullName) -and -not $entry.FullName.EndsWith('/')) {
            $currentFile++
            $destinationPath = Join-Path -Path $extractPath -ChildPath $entry.FullName

            # Ensure the destination directory exists
            $destDir = [System.IO.Path]::GetDirectoryName($destinationPath)
            if (-not (Test-Path -Path $destDir)) {
                New-Item -Path $destDir -ItemType Directory -Force | Out-Null
            }

            # Extract the file using streams
            $entryStream = $entry.Open()
            $fileStream = [System.IO.File]::Create($destinationPath)
            $entryStream.CopyTo($fileStream)
            $fileStream.Close()
            $entryStream.Close()
            
            # Show progress
            $filename = Split-Path -Path $entry.FullName -Leaf
            Show-Progress -FileNumber 2 -TotalFiles 2 -PercentComplete ([math]::Round(($currentFile / $totalFiles) * 100)) -Message "Unpacking Assets..." -CurrentRead 0 -TotalBytes 0
        }
    }

    # Close the ZIP archive
    $zipArchive.Dispose()
}

$downloadUrl = $targetAsset.browser_download_url
$outputFile = Join-Path -Path $steamPath -ChildPath $targetAsset.name
$basename = [System.IO.Path]::GetFileNameWithoutExtension($targetAsset.name)
DownloadFileWithProgress -Url $downloadUrl -DestinationPath $outputFile -FileNumber 1 -TotalFiles 2 -FileName "Downloading $basename..."
Write-Host ""
Extract-ZipWithProgress -zipPath $outputFile -extractPath $steamPath

# Remove the downloaded zip file
Remove-Item -Path $outputFile > $null


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
    
    $exists = if (-not (Test-Path -Path $_)) { "" } else { "${BoldRed}fail${ResetColor}" }
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

if (-not $iniObj.Contains("PackageManager")) { $iniObj["PackageManager"] = [ordered]@{} }
$iniObj["PackageManager"]["devtools"] = "no"

if (-not $iniObj.Contains("Settings")) { $iniObj["Settings"] = [ordered]@{} }
$iniObj["Settings"]["check_for_updates"] = "yes"


Write-Host "`n${BoldPurple}::${ResetColor} Wrote configuration to $configPath"

Set-IniFile $iniObj $configPath -PreserveNonData $false

if ($isUpdater) {
    Write-Output "${BoldPurple}++${ResetColor} Millennium has been updated successfully."
    exit
}

$pipeName = "MillenniumStdoutPipe"
$bufferSize = 1024

$pipeServer = [System.IO.Pipes.NamedPipeServerStream]::new($pipeName, [System.IO.Pipes.PipeDirection]::In)

Write-Host ""
Write-Output "${BoldPurple}::${ResetColor} Verifying runtime environment..."
Write-Output "${BoldPurple}::${ResetColor} Millennium will now start bootstrapping Steam, expect to see Millennium logs below."
Write-Output "${BoldPurple}++${ResetColor} If you face any issues while verifying, please report them on the GitHub repository."
Write-Output "${BoldPurple}++${ResetColor} ${BoldLightBlue}https://github.com/SteamClientHomebrew/Millennium/issues/new/choose${ResetColor}`n"

# sleep 5 seconds to allow the user to read the message
Start-Sleep -Seconds 5

Start-Steam -steamPath $steamPath

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