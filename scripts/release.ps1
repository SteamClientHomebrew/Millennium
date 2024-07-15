$content = Get-Content -Path "./version"
$version = $content[1]

$response = Read-Host "[Millennium@$version] new version"

if ($response -ne "") {
    $content[1] = $response
}
# Write the modified content back to the file
$content | Set-Content -Path $file

$releaseMarkdown = "./scripts/release-notes.md"

if (!(Test-Path $releaseMarkdown)) {
    New-Item -ItemType File -Path $releaseMarkdown > $null
} else {
    Clear-Content -Path $releaseMarkdown
}

$response = Read-Host "Created release notes, press enter to continue"

git add .
git commit -m "version - bump to $response"
git push

gh workflow run 101869272 build-windows.yml
gh run watch

