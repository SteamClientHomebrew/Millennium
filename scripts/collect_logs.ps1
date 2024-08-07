$response = Invoke-WebRequest -Uri "http://localhost:8080/json" -UseBasicParsing
$json = $response.Content | ConvertFrom-Json
$item = $json | Where-Object { $_.title -eq "SharedJSContext" }

if ($item) {
    $webSocketUrl = $item.webSocketDebuggerUrl
    $strippedUrl = $webSocketUrl -replace "^ws://", ""

    Start-Process "http://localhost:8080/devtools/devtools_app.html?ws=$strippedUrl"
} else {
    Write-Host "Item with title 'SharedJSContext' not found."
}