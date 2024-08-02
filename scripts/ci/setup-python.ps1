# Define the URLs and the destination folder
$pythonUrl = "https://www.python.org/ftp/python/3.11.8/python-3.11.8-embed-win32.zip"
$getPipUrl = "https://bootstrap.pypa.io/get-pip.py"
$destinationFolder = "C:\Users\shadow\Desktop\cache"

# Create the destination folder if it doesn't exist
if (-Not (Test-Path $destinationFolder)) {
    New-Item -ItemType Directory -Path $destinationFolder
}

# Define the paths to the downloaded files
$zipFilePath = Join-Path $destinationFolder "python-3.11.8-embed-win32.zip"
$getPipFilePath = Join-Path $destinationFolder "get-pip.py"

# Function to download a file
function Download-File($Url, $DestinationPath) {
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

# Custom function to extract the zip file and overwrite existing files
function Extract-ZipFile {
    param (
        [string]$zipPath,
        [string]$extractPath
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zipArchive = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
    foreach ($entry in $zipArchive.Entries) {
        $destinationPath = Join-Path $extractPath $entry.FullName
        $destinationDir = [System.IO.Path]::GetDirectoryName($destinationPath)
        
        if (-Not (Test-Path $destinationDir)) {
            New-Item -ItemType Directory -Path $destinationDir | Out-Null
        }

        if ($entry.Name -ne "") {
            if (Test-Path $destinationPath) {
                Remove-Item $destinationPath -Force
            }
            $fileStream = [System.IO.File]::Create($destinationPath)
            $entryStream = $entry.Open()
            $buffer = New-Object byte[] 8192
            $bytesRead = 0

            while (($bytesRead = $entryStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
                $fileStream.Write($buffer, 0, $bytesRead)
            }

            $fileStream.Close()
            $entryStream.Close()
        }
    }
    $zipArchive.Dispose()
}

# Download and extract Python
Download-File -Url $pythonUrl -DestinationPath $zipFilePath
Extract-ZipFile -zipPath $zipFilePath -extractPath $destinationFolder
Remove-Item $zipFilePath

Write-Host "Downloaded python packages to $destinationFolder"

Set-Content -Path (Join-Path $destinationFolder "python311._pth") -Value @'
python311.zip
.
# The following line has been added by Millennium
# This will implicitly run site.main() which lets us pip from 'python -m pip'
import site
'@

Write-Host "Enabled external site package support."

# Download get-pip.py
Download-File -Url $getPipUrl -DestinationPath $getPipFilePath

# Run get-pip.py to install pip
$pythonExe = Join-Path $destinationFolder "python.exe"
& $pythonExe $getPipFilePath --no-warn-script-location

Remove-Item $getPipFilePath
Write-Host "Installed pip on embedded packages"
