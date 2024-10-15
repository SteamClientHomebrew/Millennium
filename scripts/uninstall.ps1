# This file is part of the Millennium project.
# This script is used to install Millennium on a Windows machine.
# https://github.com/SteamClientHomebrew/Millennium/blob/main/scripts/install.ps1
# Copyright (c) 2024 Millennium

# Define ANSI escape sequence for bold purple color
$BoldPurple = [char]27 + '[38;5;219m'
$BoldGreen = [char]27 + '[1;32m'
$BoldYellow = [char]27 + '[1;33m'
$BoldRed = [char]27 + '[1;31m'
$BoldGrey = [char]27 + '[1;30m'
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

if (-not (Test-Path -Path $steamPath)) {
    New-Item -ItemType Directory -Path $steamPath
}

# Path to installed files
$jsonFilePath = Join-Path -Path $steamPath -ChildPath "/ext/data/logs/installer.log"

# test if file exists
if (-not (Test-Path -Path $jsonFilePath)) {
    Write-Host "`n${BoldRed}[!]${ResetColor} Your Millennium installation is corrupted. Please reinstall, then uninstall again."
    Write-Host "${BoldRed}[!]${ResetColor} Run: iwr -useb https://steambrew.app/install.ps1 | iex"
    exit
}

$jsonContent = Get-Content -Path $jsonFilePath -Raw
$jsonObject = $jsonContent | ConvertFrom-Json

# Write-Output "`n${BoldPurple}[+]${ResetColor} Packages ($packageCount) ${BoldPurple}Millennium@$releaseTag${ResetColor}"
Write-Host "${BoldPurple}::${ResetColor} Reading package database...`n"

function Get-FileSize {
    param ($relativePath)
    $totalSize = 0

    $absolutePath = Join-Path -Path $steamPath -ChildPath $relativePath

    if (Test-Path $absolutePath -PathType Leaf) {
        $fileSize = (Get-Item $absolutePath).Length
        $totalSize += $fileSize
    }

    return $totalSize
}

function Get-FolderSize {
    param ([Parameter(Mandatory = $true)] [string]$FolderPath)

    $totalSize = 0
    $absolutePath = Join-Path -Path $steamPath -ChildPath $FolderPath

    if (-not (Test-Path -Path $absolutePath -PathType Container)) {
        return 0
    }

    $files = Get-ChildItem -Path $absolutePath -File -Recurse -Force

    foreach ($file in $files) {
        $totalSize += $file.Length
    }
    return $totalSize
}

function ContentIsDirectory {
    param ([string]$path)
    return (Test-Path -Path (Join-Path -Path $steamPath -ChildPath $path) -PathType Container)
}

function DynamicSizeCalculator {
    param ($value)

    $totalSize = 0

    if ($value -is [System.Object[]]) {

        for ($i = 0; $i -lt $value.Length; $i += 1) {

            $isDirectory = ContentIsDirectory -path $value[$i]
            if ($isDirectory) {
                $totalSize += Get-FolderSize -FolderPath $value[$i]
            }
            else {
                $totalSize += Get-FileSize -relativePath $value[$i]
            }
        }
        
    }
    else { 
        $isDirectory = ContentIsDirectory -path $value
        if ($isDirectory) {
            $totalSize += Get-FolderSize -FolderPath $value
        }
        else {
            $totalSize += Get-FileSize -relativePath $value
        }
    }
    return $totalSize
}

$globalInitialSize = 0

function PrettyPrintSizeOnDisk {
    param ([Parameter(Mandatory = $true)] [object[]]$targetPath)

    $totalSize = 0
    $index = 0

    for ($i = 0; $i -lt $targetPath.Length; $i += 2) {
        $index++
        $key = $targetPath[$i]
        $value = $targetPath[$i + 1]

        $size = DynamicSizeCalculator -value $value
        $totalSize += $size

        if ($size -eq 0) {
            $strSize = "0 Bytes"
        }
        else {
            $strSize = ConvertTo-ReadableSize -size $size
        }
        Write-Output "${BoldGrey}++${ResetColor} [$index] ${BoldPurple}$($key.PadRight(15))${ResetColor} $($strSize.PadLeft(10))"
    }

    $global:globalInitialSize = $totalSize
    Write-Output "`n${BoldPurple}::${ResetColor} Current Install Size: $(ConvertTo-ReadableSize -size $totalSize)"
}

$assets = @(
    "Millennium", @("user32.dll", "python311.dll")
    "Core Modules", "ext/data/assets"
    "Python Cache", "ext/data/cache"
    "User Plugins", "plugins"
    "User Themes", "steamui/skins"
)

PrettyPrintSizeOnDisk -targetPath $assets
$packageList = (1..($assets.Length / 2)) -join ''

$result = Read-Host "${BoldPurple}::${ResetColor} Enter a numerical list of packages to uninstall [default=$packageList]"

if (-not $result) {
    $result = $packageList
}

$selectedPackages = ($result.ToCharArray() | Select-Object -Unique) | ForEach-Object { $assets[($_ - 48) * 2 - 2] }
$selectedPackagesPath = ($result.ToCharArray() | Select-Object -Unique) | ForEach-Object { $assets[($_ - 48) * 2 - 1] }

$purgedAssetsSize = DynamicSizeCalculator -value $selectedPackagesPath
$selectedPackages = $selectedPackages -join ", "

$strReclaimedSize = ConvertTo-ReadableSize -size $purgedAssetsSize
$strRemainingSize = ConvertTo-ReadableSize -size ($global:globalInitialSize - $purgedAssetsSize)

Write-Host "`n${BoldPurple}++${ResetColor} Purging Packages: [${BoldRed}$selectedPackages${ResetColor}]`n"

Write-Host " Total Removed Size:    $($strReclaimedSize.PadLeft(10))"
Write-Host " Total Remaining Size:  $($strRemainingSize.PadLeft(10))`n"


$result = Ask-Boolean-Question -question "Proceed with PERMANENT removal of selected packages?" -default $true -newLine $false

if (-not $result) {
    Write-Output "${BoldPurple}[+]${ResetColor} Removal aborted."
    exit
}

$deletionSuccess = $true

$selectedPackagesPath | ForEach-Object {

    $isDirectory = ContentIsDirectory -path $_

    if ($_ -match "user32.dll") {
        $cefRemoteDebugging = Join-Path -Path $steamPath -ChildPath ".cef-enable-remote-debugging"

        if (Test-Path -Path $cefRemoteDebugging) {
            Remove-Item -Path $cefRemoteDebugging -Force -ErrorAction SilentlyContinue
        }
    }

    $absolutePath = Join-Path -Path $steamPath -ChildPath $_

    Remove-Item -Path $absolutePath -Recurse -Force -ErrorAction SilentlyContinue

    if (-not $?) {
        $global:deletionSuccess = $false
        Write-Host "${BoldRed}[!]${ResetColor} Failed to remove: $absolutePath"
    }
}

if ($deletionSuccess) {
    Write-Host "${BoldGreen}++${ResetColor} Successfully removed selected packages."
}
else {
    Write-Host "${BoldRed}[!]${ResetColor} Some deletions failed. Please manually remove the remaining files."
}
