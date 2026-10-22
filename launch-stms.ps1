Param(
    [switch]$SkipPython,
    [switch]$Stop
)

# Main launcher: Python environment setup, then mobile app backend + frontend.
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$mobile = Join-Path $root "MobileApp"

if ($Stop) {
    & (Join-Path $mobile "stop-local.ps1")
    return
}

if (-not $SkipPython) {
    Write-Host "== [1/3] Python setup ==" -ForegroundColor Cyan
    & (Join-Path $root "PythonSetup\launch.ps1") -Command setup
}

Write-Host "== [2/3] Node dependencies ==" -ForegroundColor Cyan
$npm = (Get-Command npm.cmd -ErrorAction SilentlyContinue).Source
if (-not $npm) {
    $npm = @(
        "C:\Program Files\nodejs\npm.cmd",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Microsoft\VisualStudio\NodeJs\npm.cmd",
        "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Microsoft\VisualStudio\NodeJs\npm.cmd"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if ($npm) { $env:PATH = "$(Split-Path $npm);$env:PATH" }
foreach ($dir in @("MobileApp\backend\node-express", "MobileApp\frontend")) {
    $path = Join-Path $root $dir
    # Verify a known transitive dependency so a half-installed tree gets repaired
    $probe = if ($dir -like "*frontend") { "node_modules\write-file-atomic" } else { "node_modules\express" }
    if (Test-Path (Join-Path $path $probe)) {
        Write-Host "$dir : node_modules present"
    }
    elseif ($npm) {
        Push-Location $path
        try { & $npm install; if ($LASTEXITCODE -ne 0) { throw "npm install failed in $dir" } } finally { Pop-Location }
    }
    else {
        throw "node_modules missing in $dir and npm was not found. Install Node.js."
    }
}

Write-Host "== [3/3] Mobile app backend + frontend ==" -ForegroundColor Cyan
& (Join-Path $mobile "start-local.ps1")
Write-Host "Stop everything with: .\launch-stms.ps1 -Stop"
