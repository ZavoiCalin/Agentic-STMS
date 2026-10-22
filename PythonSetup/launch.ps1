Param(
    [ValidateSet("route", "traffic", "train", "cloud", "jetson", "all", "setup", "help")]
    [string]$Command = "help"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$setupRoot = $PSScriptRoot
$venvPath = Join-Path $setupRoot ".venv"
$pythonExe = Join-Path $venvPath "Scripts\python.exe"
$requirementsFile = Join-Path $setupRoot "requirements.txt"

function Ensure-Venv {
    if (-not (Test-Path $venvPath)) {
        Write-Host "Creating virtual environment in $venvPath"
        python -m venv $venvPath
    }

    if (-not (Test-Path $pythonExe)) {
        throw "Python executable not found at $pythonExe. Check the virtual environment setup."
    }

    & $pythonExe -m pip install --upgrade pip setuptools wheel | Out-Null
    if (Test-Path $requirementsFile) {
        & $pythonExe -m pip install -r $requirementsFile
    }
}

function Run-Route {
    $scriptPath = Join-Path $projectRoot "RaspberryPi\AI_routing_algorithm\ai_routing.py"
    & $pythonExe $scriptPath
}

function Run-Traffic {
    $scriptPath = Join-Path $projectRoot "RaspberryPi\Traffic_light_commands\main.py"
    & $pythonExe $scriptPath
}

function Run-Train {
    $scriptPath = Join-Path $projectRoot "RaspberryPi\Traffic_light_commands\train.py"
    & $pythonExe $scriptPath
}

function Run-Cloud {
    $scriptPath = Join-Path $projectRoot "RaspberryPi\edge_pipeline.py"
    & $pythonExe $scriptPath --sync-cloud
}

function Run-Jetson {
    $scriptPath = Join-Path $projectRoot "RaspberryPi\edge_pipeline.py"
    & $pythonExe $scriptPath --jetson-cameras
}

function Show-Help {
    Write-Host "Smart Traffic Project launcher"
    Write-Host "Usage: ./PythonSetup/launch.ps1 -Command <route|traffic|train|cloud|jetson|all|help>"
    Write-Host "  route    - compute the AI route"
    Write-Host "  traffic  - evaluate the traffic light command script"
    Write-Host "  train    - train the Q-learning policy"
    Write-Host "  cloud    - sync computed data to Azure IoT Hub and Azure Blob Storage when configured; MongoDB is legacy compatibility only"
    Write-Host "  jetson   - launch the dual Jetson CUDA camera stream from Extra/Edge_AI"
    Write-Host "  all      - run route, traffic, and cloud sync with the optional Jetson camera step"
}

Ensure-Venv

switch ($Command) {
    "route" { Run-Route }
    "traffic" { Run-Traffic }
    "train" { Run-Train }
    "cloud" { Run-Cloud }
    "jetson" { Run-Jetson }
    "setup" { Write-Host "Python environment ready: $pythonExe" }
    "all" {
        Run-Route
        Run-Traffic
        Run-Cloud
    }
    default { Show-Help }
}
