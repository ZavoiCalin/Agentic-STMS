$ErrorActionPreference = 'Stop'

$projectRoot = $PSScriptRoot
$backendDir = Join-Path $projectRoot 'backend\node-express'
$frontendDir = Join-Path $projectRoot 'frontend'
$logDir = Join-Path $projectRoot 'logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null

$nodeCandidates = @(
  'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Microsoft\VisualStudio\NodeJs\node.exe',
  'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Microsoft\VisualStudio\NodeJs\node.exe',
  'C:\Program Files\nodejs\node.exe'
)

$nodeExe = $nodeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $nodeExe) {
  throw 'Could not find node.exe. Install Node.js or use the Visual Studio NodeJs runtime.'
}

function Stop-ListenersOnPort {
  param([int]$Port)

  $connections = Get-NetTCPConnection -LocalPort $Port -ErrorAction SilentlyContinue
  if (-not $connections) {
    return
  }

  $pids = @($connections | Select-Object -ExpandProperty OwningProcess -Unique)
  foreach ($processId in $pids) {
    if ($processId -and $processId -gt 0) {
      Stop-Process -Id $processId -Force -ErrorAction SilentlyContinue
    }
  }
}

function Wait-ForHttp {
  param(
    [string]$Url,
    [int]$TimeoutSeconds = 30
  )

  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  while ((Get-Date) -lt $deadline) {
    try {
      $response = Invoke-WebRequest -UseBasicParsing -Uri $Url -TimeoutSec 5
      if ($response.StatusCode -ge 200 -and $response.StatusCode -lt 400) {
        return $true
      }
    }
    catch {
      Start-Sleep -Seconds 1
    }
  }

  return $false
}

Stop-ListenersOnPort -Port 3000
Stop-ListenersOnPort -Port 8083

$backendLog = Join-Path $logDir 'backend.log'
$backendErr = Join-Path $logDir 'backend.err'
$frontendLog = Join-Path $logDir 'frontend.log'
$frontendErr = Join-Path $logDir 'frontend.err'

Start-Process -FilePath $nodeExe -WorkingDirectory $backendDir -ArgumentList '.\server.js' -RedirectStandardOutput $backendLog -RedirectStandardError $backendErr -WindowStyle Hidden

$frontendCommand = "& '$nodeExe' .\node_modules\expo\bin\cli start --web --port 8083"
Start-Process powershell.exe -ArgumentList "-NoLogo", "-NoExit", "-WindowStyle", "Hidden", "-Command", "Set-Location '$frontendDir'; `$env:CI='1'; $frontendCommand" -RedirectStandardOutput $frontendLog -RedirectStandardError $frontendErr

if (-not (Wait-ForHttp -Url 'http://localhost:3000/api/health' -TimeoutSeconds 25)) {
  throw 'Backend did not become healthy on http://localhost:3000.'
}

if (-not (Wait-ForHttp -Url 'http://localhost:8083/' -TimeoutSeconds 35)) {
  throw 'Frontend did not become ready on http://localhost:8083/.'
}

Write-Host ''
Write-Host 'STMS app started successfully.'
Write-Host 'Backend: http://localhost:3000/'
Write-Host 'Frontend: http://localhost:8083/'
Write-Host 'Logs: ' $logDir
