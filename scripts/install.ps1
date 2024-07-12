# Get the process ID (PID) of steam.exe
$steamProcess = Get-Process -Name "steam" -ErrorAction SilentlyContinue

# Check if steam.exe is running
if ($steamProcess) {
    # Kill steam.exe process
    Stop-Process -Name "steam" -Force
    Write-Output "[+] Closed Steam process."
}

function Show-Progress {
    param (
        [int]$FileNumber,
        [int]$TotalFiles,
        [int]$PercentComplete,
        [string]$Message = "Downloading..."
    )

    $ProgressBarLength = 25
    $Progress = [math]::round(($PercentComplete / 100) * $ProgressBarLength)
    $ProgressBar = ("#" * $Progress).PadRight($ProgressBarLength)
    
    $FileCountMessage = "[$FileNumber/$TotalFiles]"
    $ProgressMessage = "`r$FileCountMessage [$ProgressBar] $PercentComplete% - $Message"
    
    Write-Host -NoNewline $ProgressMessage -f Magenta
}

function DownloadFileWithProgress {
    param (
        [string]$Url,
        [string]$DestinationPath,
        [int]$FileNumber,
        [int]$TotalFiles,
        [string]$FileName
    )

    try {
        # Create the web request
        $webRequest = [System.Net.HttpWebRequest]::Create($Url)
        $webRequest.Method = "GET"
        $webRequest.UserAgent = "Millennium.Installer/1.0"

        # Get the response
        $response = $webRequest.GetResponse()
        $totalBytes = $response.ContentLength
        $responseStream = $response.GetResponseStream()

        # Create the output file
        $fileStream = New-Object System.IO.FileStream($DestinationPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
        $buffer = New-Object byte[] 8192
        $bytesRead = 0
        $totalBytesRead = 0

        [Console]::CursorTop -= 0

        [Console]::SetCursorPosition(0, [Console]::CursorTop)
        [Console]::Write(' ' * [Console]::WindowWidth)

        # Download in chunks
        while (($bytesRead = $responseStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            $fileStream.Write($buffer, 0, $bytesRead)
            $totalBytesRead += $bytesRead
            $percentComplete = [math]::round(($totalBytesRead / $totalBytes) * 100)
            Show-Progress -FileNumber $FileNumber -TotalFiles $TotalFiles -PercentComplete $percentComplete -Message $FileName
        }

        # Cleanup
        $fileStream.Close()
        $responseStream.Close()
    }
    catch {
        Write-Host "Error downloading file: $_"
        if ($fileStream) { $fileStream.Close() }
        if ($responseStream) { $responseStream.Close() }
    }
}

# Define the GitHub API URL
$apiUrl = "https://api.github.com/repos/SteamClientHomebrew/Millennium/releases"

# Make the API request
$response = Invoke-RestMethod -Uri $apiUrl -Headers @{ "User-Agent" = "Millennium.Installer/1.0" }

# Filter the releases to get the latest non-prerelease release
$latestRelease = $response | Where-Object { -not $_.prerelease } | Sort-Object -Property created_at -Descending | Select-Object -First 1

# Prompt the user for a custom Steam path
$customSteamPath = Read-Host "[?] Steam Path (leave blank for default)"

if (-not $customSteamPath) {
    # Get the Steam path from the registry if no custom path is provided
    $steamPath = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam").SteamPath

    if (-not $steamPath) {
        Write-Host "Steam path not found in registry."
        exit
    }
} else {
    $steamPath = $customSteamPath
}

Write-Output "[+] Using Steam path: $steamPath"

# Ensure the destination directory exists
if (-not (Test-Path -Path $steamPath)) {
    New-Item -ItemType Directory -Path $steamPath
}

# Calculate the total size of all download assets in bytes
$totalSizeBytes = ($latestRelease.assets | Measure-Object -Property size -Sum).Sum

# Function to convert bytes to the appropriate unit (KB, MB, GB)
function ConvertTo-ReadableSize {
    param([int64]$size)
    
    $units = @("bytes", "KB", "MB", "GB")
    $index = [Math]::Floor([Math]::Log($size, 1024))
    $sizeFormatted = [Math]::Round($size / [Math]::Pow(1024, $index), 2)
    
    return "$sizeFormatted $($units[$index])"
}

# Convert the total size to a readable format
$totalSizeReadable = ConvertTo-ReadableSize -size $totalSizeBytes

# Print the total size
Write-Output "[+] Downloading [$totalSizeReadable] of required files..."

# Write-Host "[+] Downloading required files... $latestRelease"

# Download each asset in the release
$FileCount = $latestRelease.assets.Count
for ($i = 0; $i -lt $FileCount; $i++) {
    $asset = $latestRelease.assets[$i]
    $downloadUrl = $asset.browser_download_url
    $outputFile = Join-Path -Path $steamPath -ChildPath $asset.name

    # Download the file with custom progress bar
    DownloadFileWithProgress -Url $downloadUrl -DestinationPath $outputFile -FileNumber ($i + 1) -TotalFiles $FileCount -FileName $asset.name
}

# Prompt the user for input with formatted text
$choice = Read-Host "`n[+] Do you want to install developer packages? [y/N]"

# Process user input
if ($choice -eq "y" -or $choice -eq "yes") {
    Write-Host "yes"
} 
elseif ($choice -eq "n" -or $choice -eq "no") {
    Write-Host "no"
} 


# Prompt the user for input with formatted text
$choice = Read-Host "[+] Do you want Millennium to auto update? (Recommeded) [Y/n]"

# Process user input
if ($choice -eq "y" -or $choice -eq "yes") {
    Write-Host "yes"
} 
elseif ($choice -eq "n" -or $choice -eq "no") {
    Write-Host "no"
} 

# Prompt the user for input with formatted text
$choice = Read-Host "[+] Do you want to use Millennium's GUI? (no = configure millennium with files only) [Y/n]"

# Process user input
if ($choice -eq "y" -or $choice -eq "yes") {
    Write-Host "yes"
} 
elseif ($choice -eq "n" -or $choice -eq "no") {
    Write-Host "no"
} 