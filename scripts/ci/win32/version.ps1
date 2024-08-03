$VERSION = npx semantic-release --dry-run | Select-String -Pattern "The next release version is" | ForEach-Object { $_.ToString() -replace '.*The next release version is ([0-9.]+).*', '$1' }
Write-Host "Version: $VERSION"
Write-Output "::set-output name=version::$version"
Set-Content -Path ./version -Value "# current version of millennium`nv$VERSION"