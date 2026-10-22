# Smart Traffic Management System

This workspace is organized around the active project structure and a small set of reference archives. The Arduino prototype and unused root-level launch files were removed, while the functional project areas remain separated by role.

## Active project areas

The active project folders are:

- RaspberryPi/ — edge coordination, route logic, traffic control scripts, and cloud/stream pipeline components
- Jetson/ — Jetson-specific camera and inference assets used alongside the Raspberry Pi runtime
- MobileApp/ — mobile route app (Expo frontend and Node/Express backend, with Azure Function, FastAPI, and Raspberry Pi backend components and a database schema); see `MobileApp/README.md`
- Launcher/ — the cross-platform Go launcher
- STM32/STM32CubeIDE/ — the real STM32 project area used for embedded sensor-node development and traffic analytics logic
- PythonSetup/ — Python requirements and the venv for local execution

The following is intentionally kept as reference or archived material, not as the live runtime:

- SimulatedDevicesPrototype/ — historical project snapshot retained for reference and legacy design comparison (git-ignored)

## Current architecture

The current design in this workspace is centered on the following flow:

- Raspberry Pi 5: central coordinator and edge gateway
- NVIDIA Jetson: camera-driven vehicle detection and traffic-state analytics
- STM32 sensor node (NUCLEO-N657X0-Q): embedded acquisition and traffic sensor telemetry; Arduino is no longer part of the active architecture
- Azure IoT Hub / Azure Blob Storage: optional cloud integration and persistence for the edge pipeline
- Python-based local tooling: route simulation, traffic logic evaluation, and project bootstrap

MongoDB is still kept as a legacy compatibility option, but it is not the preferred active persistence layer.

## Current local execution

### Launcher

**Quick start (Windows):** from the project root, build once and run:

```powershell
cd Launcher; go build -ldflags "-s -w" -o ..\bin\launch-stms.exe .; cd ..
.\bin\launch-stms.exe
```

Then open http://localhost:8083/ . Stop with `.\bin\launch-stms.exe stop`. On later runs only the last command is needed (add `start --skip-python` to skip the Python setup).

**Quick start (Linux / macOS / Raspberry Pi):**

```sh
cd Launcher && go build -ldflags "-s -w" -o ../bin/launch-stms . && cd ..
./bin/launch-stms
```

`Launcher/` contains the single cross-platform launcher (Go, standard library only). It builds to one native executable for Windows, Linux, macOS and Raspberry Pi. At run time it needs Python 3 and Node.js installed. It finds the project root by looking for the `PythonSetup/` folder upward from its own location.

**Build** (needs [Go](https://go.dev/dl/) 1.21+; output goes to `bin/`, which is git-ignored)

- Windows: `cd Launcher; go build -ldflags "-s -w" -o ..\bin\launch-stms.exe .`
- Linux / macOS / Raspberry Pi: `cd Launcher && go build -ldflags "-s -w" -o ../bin/launch-stms .`
- Cross-compile by setting `GOOS`/`GOARCH` (for example `GOOS=linux GOARCH=arm64` for a 64-bit Raspberry Pi, `GOOS=darwin GOARCH=arm64` for Apple Silicon).

**Usage**

- Windows: `.\bin\launch-stms.exe [command] [--skip-python]`
- Linux / macOS / Raspberry Pi: `./bin/launch-stms [command] [--skip-python]`

**Commands**

| Command | What it does |
|---------|--------------|
| `start` (default) | Python setup, Node dependency check, then starts the mobile backend and frontend |
| `stop` | Stops the mobile backend and frontend started by `start` |
| `status` | Shows whether the backend and frontend are running (exit code 1 if not) |
| `setup` | Only creates the Python venv and installs `requirements.txt` |
| `route` | Computes the AI route |
| `traffic` | Runs the traffic light command script |
| `train` | Trains the Q-learning policy |
| `cloud` | Syncs data to Azure IoT Hub / Blob Storage when configured |
| `jetson` | Launches the Jetson camera stream |
| `all` | Runs `route`, `traffic` and `cloud` |

**Options:** `--skip-python` (with `start`) skips the Python setup.

**What `start` does**

1. Python setup: creates `PythonSetup/.venv` and installs `requirements.txt`.
2. Node dependencies: runs `npm install` in `MobileApp/backend/node-express` and `MobileApp/frontend` if packages are missing. Node is found on `PATH` first, then in common install folders (including the Visual Studio bundled Node on Windows).
3. Start: launches the backend at http://localhost:3000 and the Expo web frontend at http://localhost:8083, then waits until both respond and the web bundle is built. Open the frontend URL in a browser.

Logs go to `MobileApp/logs/` and process IDs to `MobileApp/logs/pids.json`, which `stop` uses. If port 3000 or 8083 is used by another program, `start` exits with a message.

If the app shows "offline", the backend is not running (for example after `stop`): run `status`, then `start`.

The frontend runs without file watching, so after changing frontend code run `stop` and then `start` again.

## Project layout

- `RaspberryPi/` — current application logic, traffic light command flow, and routing components
- `Jetson/` — Jetson-specific processing and camera pipeline pieces
- `MobileApp/` — mobile route app (frontend, backends, logs, and local start/stop scripts)
- `Launcher/` — Go source for the launcher- `STM32/STM32CubeIDE/` — active embedded project for the STM32 sensor node and traffic analytics code
- `PythonSetup/` — Python requirements and virtual environment
- `PythonSetup/requirements.txt` — active Python dependencies for local tooling

## Important status notes

- The Arduino prototype folder was removed because it is no longer part of the active hardware architecture.
- The project intentionally keeps historical material around for reference, but the active development path is the Raspberry Pi + Jetson + STM32 + Azure workflow.
- MongoDB remains a compatibility-only option and is not the preferred live persistence layer.
- The project continues to support local execution without cloud credentials when those services are not configured.

## Notes

This README reflects the cleaned current workspace state: active project folders are preserved, obsolete Arduino artifacts are removed, and the practical working entry point is the Go launcher under `Launcher/`.

## Next steps

Suggested follow-up priorities for the project are:

- Validate the end-to-end startup flow for all launcher modes (`route`, `traffic`, `cloud`, `jetson`, `all`) and confirm the Python environment bootstrap is reliable on clean machines (Windows, Linux, macOS, Raspberry Pi).
- Tighten the Raspberry Pi coordination logic by defining the message contracts between the edge controller, Jetson inference output, and the STM32 sensor telemetry stream.
- Finalize the deployment configuration for Azure IoT Hub and Blob Storage, including environment variables, secure secrets management, and fallback behavior when cloud connectivity is unavailable.
- Expand the STM32 sensor-node integration by testing real traffic signal inputs, calibration thresholds, and fault handling for noisy or missing data.
- Add a lightweight validation pipeline for Python and embedded code, such as unit tests for route logic, linting for scripts, and a CI step that checks the project still boots cleanly.
- Document the actual runtime responsibilities of each subsystem so future contributors can tell which components are production-critical versus experimental.
- Consider a single reference architecture diagram or runbook that shows the full data flow from sensors to inference to traffic decisions and cloud logging.

These next steps will help turn the current workspace from a structured multi-device prototype into a more repeatable, testable, and deployment-ready traffic management system.
