# Define the URL to fetch the JSON data
$versionUrl = "http://localhost:8080/json/version"

# Fetch the JSON data from the version URL
try {
    $jsonResponse = Invoke-RestMethod -Uri $versionUrl -Method Get
} catch {
    Write-Host "Failed to fetch JSON from $versionUrl" -ForegroundColor Red
    exit 1
}

# Extract the webSocketDebuggerUrl from the JSON
$webSocketDebuggerUrl = $jsonResponse.webSocketDebuggerUrl

if (-not $webSocketDebuggerUrl) {
    Write-Host "webSocketDebuggerUrl not found" -ForegroundColor Red
    exit 1
}

Write-Host "WebSocket Debugger URL: $webSocketDebuggerUrl"

# Open a WebSocket connection
$socket = New-Object System.Net.WebSockets.ClientWebSocket
$uri = New-Object Uri($webSocketDebuggerUrl)

try {
    $socket.ConnectAsync($uri, [System.Threading.CancellationToken]::None).Wait()
    Write-Host "Connected to WebSocket"
} catch {
    Write-Host "Failed to connect to WebSocket" -ForegroundColor Red
    exit 1
}

# Prepare the message to crash the browser
$crashMessage = @{
    id = 443300
    method = "Browser.crash"
}

# Convert the crash message to JSON
$jsonCrashMessage = $crashMessage | ConvertTo-Json

# Send the crash message via the WebSocket
$buffer = [System.Text.Encoding]::UTF8.GetBytes($jsonCrashMessage)
$bufferSegment = New-Object 'ArraySegment[Byte]' $buffer, 0, $buffer.Length

try {
    $socket.SendAsync($bufferSegment, [System.Net.WebSockets.WebSocketMessageType]::Text, $true, [System.Threading.CancellationToken]::None).Wait()
    Write-Host "Crash message sent"
} catch {
    Write-Host "Failed to send message over WebSocket" -ForegroundColor Red
}

# Close the WebSocket connection
$socket.CloseAsync([System.Net.WebSockets.WebSocketCloseStatus]::NormalClosure, "Closing", [System.Threading.CancellationToken]::None).Wait()
Write-Host "WebSocket connection closed"
