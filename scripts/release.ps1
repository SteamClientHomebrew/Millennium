$content = Get-Content -Path "./version"
$version = $content[1]

$response = Read-Host "[Millennium@$version] new version"

Write-Host "You entered: $response"

if ($response -ne "") {
    $content[1] = $response
}
# Write the modified content back to the file
$content | Set-Content -Path $file

git add .
git commit -m "version - bump to $response"
git push

gh workflow run 101869272 build-windows.yml
gh run watch

