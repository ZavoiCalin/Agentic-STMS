$ErrorActionPreference = 'Stop'

function Stop-ListenersOnPort {
    param([int]$Port)

    $connections = Get-NetTCPConnection -LocalPort $Port -ErrorAction SilentlyContinue
    if (-not $connections) {
        Write-Host "No listeners found on port $Port."
        return
    }

    $pids = @($connections | Select-Object -ExpandProperty OwningProcess -Unique)
    foreach ($processId in $pids) {
        if ($processId -and $processId -gt 0) {
            try {
                Stop-Process -Id $processId -Force -ErrorAction Stop
                Write-Host "Stopped process $processId listening on port $Port."
            }
            catch {
                Write-Host "Could not stop process $processId on port $Port."
            }
        }
    }
}

Stop-ListenersOnPort -Port 3000
Stop-ListenersOnPort -Port 8083

Write-Host "STMS app stopped."
