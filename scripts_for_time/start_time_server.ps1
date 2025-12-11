# ================================
# EC535 BBB Time Sync Server
# 1. Continuously update time.txt in this folder
# 2. Start HTTP server on port 8000
# ================================

Write-Host "Starting EC535 time sync server..."

# Get script directory and switch to it
$ScriptDir = $PSScriptRoot
Set-Location -Path $ScriptDir

# Full path for time.txt
$TimeFile = Join-Path $ScriptDir "time.txt"

# Start background job to update time.txt every second
Write-Host "[1] Launching time generator..."
Start-Job -ScriptBlock {
    param($Path)
    while ($true) {
        Get-Date -Format "yyyy-MM-dd HH:mm:ss" | Out-File -Encoding ascii $Path
        Start-Sleep -Seconds 1
    }
} -ArgumentList $TimeFile

# Start HTTP server on port 8000 (serving this folder)
Write-Host "[2] Starting HTTP server on port 8000..."
python -m http.server 8000

Write-Host "Server running. BBB can sync time from:"
Write-Host "http://192.168.137.1:8000/time.txt"

