$destinationBase = "D:/a/env/ext/data/assets"

$paths = @{
    "./assets/pipx"             = "$destinationBase/pipx"
}

foreach ($source in $paths.Keys) {
    $destination = $paths[$source]
    $destinationDir = Split-Path -Path $destination -Parent
    if (-not (Test-Path -Path $destinationDir)) {
        New-Item -Path $destinationDir -ItemType Directory -Force
    }
    Copy-Item -Path $source -Destination $destination -Recurse -Force
}
