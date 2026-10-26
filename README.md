# Smart Traffic Management System

This project is an AI agent enhanced version of the project. Its development and optimisation is heavily based on AI agent orchestraction using Copilot and Visual Studio Code.

This workspace is organized around the active project structure and a small set of reference archives. The Arduino prototype is archived and unused root-level launch files were removed, while the functional project areas remain separated by role.

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

- Raspberry Pi 5: native C++ route/traffic tools and TCP/Bluetooth-UART telemetry receivers that consume Jetson and sensor readings
- NVIDIA Jetson: native C++ GStreamer/DeepStream CUDA/TensorRT vehicle detection and GPU NvDCF tracking on exactly two CSI-connected Raspberry Pi Camera Module v2 cameras, with Jetson NVMM/Argus capture and batched GPU inference; sends only timestamp, intersection coordinates, and vehicle count to the Pi
- STM32 sensor node (NUCLEO-N657X0-Q): C++ application and analytics with CubeIDE/HAL startup, interrupt, and HAL glue retained in C/assembly
- Azure IoT Hub / Azure Blob Storage: optional cloud integration and persistence for the edge pipeline
- Mobile application: existing Node.js/Express backend and React Native frontend
- Legacy Python cloud adapters remain optional while their native SDK integrations are migrated

MongoDB is still kept as a legacy compatibility option, but it is not the preferred active persistence layer.

## Current local execution

### Independent Raspberry Pi and Jetson development

The Raspberry Pi and Jetson are independent development machines, not runtime
targets that require this host PC to build for them. Clone the repository (or
check out the branch you are developing) directly on each board, edit or pull
the source there, and configure, build, and run tests on that board. Keep each
board's `build/` output local; do not copy host-built executables or CMake
build directories between Windows, Raspberry Pi, and Jetson.

**Raspberry Pi 5** — install CMake 3.20+, a C++17 compiler, and Git, then run
these commands from the repository root:

```sh
sudo apt update
sudo apt install -y build-essential cmake git
cmake -S RaspberryPi/Native -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --parallel
```

The Pi native target no longer vendors its JSON header: CMake uses the system
`nlohmann-json3-dev` package if installed (`sudo apt install -y nlohmann-json3-dev`),
otherwise it downloads a pinned, SHA256-verified nlohmann/json release on first
configure, so the first configure needs network access on the Pi. On Unix the same
configure also builds `bluetooth-receiver`
(`RaspberryPi/ReceiveData/bluetooth_receiver.cpp`), which reads the STM32
MICS-6814 air-quality JSON lines sent through an HC-05 on `/dev/rfcomm0`;
pairing steps are in the source header. After changing
native source, rerun `cmake --build build/native --parallel`; CMake rebuilds
the target on the Pi. To build and test the Go launcher on the Pi, install Go
1.21+ and run `cd Launcher && go test ./... && mkdir -p ../bin && go build -o ../bin/launch-stms .`.
The native C++ target does not currently define a CTest suite.

**NVIDIA Jetson** — use the JetPack installation on the Jetson itself, with
the matching CUDA toolkit and DeepStream development files available through
`pkg-config`. From the repository root, build and rebuild the camera target
using the Jetson's installed compiler and SDK:

```sh
cmake -S Jetson/Cameras_nvidia_cuda/cpp -B Jetson/Cameras_nvidia_cuda/build -DCMAKE_BUILD_TYPE=Release
cmake --build Jetson/Cameras_nvidia_cuda/build --parallel
```

The runtime test requires the Jetson hardware, its two connected CSI cameras,
and DeepStream inference/tracker configurations; see the [Jetson runtime
guide](Jetson/Cameras_nvidia_cuda/Jetson_Stream_TensorRT/dual_stream_tensorrt_doc.md).
The Jetson camera target cannot be built or meaningfully tested by building
its executable on this Windows host.

### Launcher

**Quick start (Windows):** from the project root, build the C++ tools and run:

```powershell
cmake -S RaspberryPi\Native -B build\native
cmake --build build\native --config Release
cd Launcher; go build -ldflags "-s -w" -o ..\bin\launch-stms.exe .; cd ..
.\bin\launch-stms.exe
```

Then open http://localhost:8083/ . Stop with `.\bin\launch-stms.exe stop`. CMake and a C++17 compiler are required to build the native tools; Node.js is required for the mobile app.

**Quick start (Linux / macOS / Raspberry Pi):**

```sh
cmake -S RaspberryPi/Native -B build/native
cmake --build build/native --config Release
cd Launcher && go build -ldflags "-s -w" -o ../bin/launch-stms . && cd ..
./bin/launch-stms
```

`Launcher/` contains the cross-platform launcher (Go, standard library only). It builds to one native executable for Windows, Linux, macOS and Raspberry Pi. The C++ tools use CMake, a C++17 compiler, and nlohmann/json (uses the system package `nlohmann-json3-dev` if installed, otherwise CMake downloads a pinned, SHA256-verified release on first configure). Python remains required only for the optional, not-yet-migrated cloud commands.

**Build** (needs [Go](https://go.dev/dl/) 1.21+, CMake and a C++17 compiler; generated output goes to `bin/` and `build/`)

- Windows: `cmake -S RaspberryPi\Native -B build\native; cmake --build build\native --config Release; cd Launcher; go build -ldflags "-s -w" -o ..\bin\launch-stms.exe .`
- Linux / macOS / Raspberry Pi: `cmake -S RaspberryPi/Native -B build/native && cmake --build build/native --config Release && cd Launcher && go build -ldflags "-s -w" -o ../bin/launch-stms .`
- Optional: cross-compile only the Go launcher by setting `GOOS`/`GOARCH`. This does not build the native C++ or CUDA/DeepStream targets; use the on-device workflows above for those.

**Usage**

- Windows: `.\bin\launch-stms.exe [command] [--skip-native]`
- Linux / macOS / Raspberry Pi: `./bin/launch-stms [command] [--skip-native]`

**Commands**

| Command | What it does |
|---------|--------------|
| `start` (default) | Node dependency check, then starts the mobile backend and frontend |
| `stop` | Stops the mobile backend and frontend started by `start` |
| `status` | Shows whether the backend and frontend are running (exit code 1 if not) |
| `setup` | Configures and builds the Raspberry Pi C++ tools |
| `route` | Computes the route using the native C++ tool |
| `traffic` | Computes and saves local traffic-light states; Azure delivery remains to be migrated |
| `train` | Trains the Q-learning policy and saves `q_table.npy` |
| `receive-jetson` | Receives Jetson JSON over TCP into `jetson_data.jsonl` and recalculates the route from the latest camera/sensor readings |
| `receive-bluetooth` | Receives JSON lines from Bluetooth/UART into `arduino_data.jsonl` |
| `send-route` | Posts the sample route to the configured backend |
| `cloud` | Runs the legacy Python Azure/Blob sync integration |
| `jetson` | Runs the prebuilt C++ Jetson DeepStream pipeline |
| `all` | Runs `route`, `traffic` and the legacy `cloud` sync |

**Options:** `--skip-native` (with `start`) skips the C++ build. `--skip-python` remains an alias.

The receiver defaults to TCP port `5000` and `/dev/rfcomm0` on Linux
(`COM3` on Windows). Override these with `STMS_JETSON_PORT` and
`STMS_SENSOR_PORT`. The route starts with the road-network weights and adjusts
outgoing costs at observed intersections using the latest Jetson vehicle count
and available
speed/weather/sensor readings.

For the camera-to-route flow, start `launch-stms receive-jetson` on the Pi, then
run the Jetson binary with `STMS_PI_HOST`, `STMS_DEEPSTREAM_CONFIG`, and
`STMS_TRACKER_CONFIG` set. The Pi
persists incoming JSONL and recalculates `optimal_route_results.json` for each
new valid camera report. Each report contains only `timestamp`,
`intersection_coordinates` and `vehicle_count`. Both cameras currently use
Timișoara coordinates (45.7489, 21.2087), which map to Intersection A. The
Jetson pipeline is configured for exactly two Raspberry Pi Camera Module v2
CSI sensors, IDs 0 and 1.
Set `STMS_JETSON_HEADLESS=1` when using the launcher without a display; this
bypasses OSD and avoids unnecessary GPU work.

**What `start` does**

1. Node dependencies: runs `npm install` in `MobileApp/backend/node-express` and `MobileApp/frontend` if packages are missing. Node is found on `PATH` first, then in common install folders (including the Visual Studio bundled Node on Windows).
2. Start: launches the backend at http://localhost:3000 and the Expo web frontend at http://localhost:8083, then waits until both respond and the web bundle is built. Open the frontend URL in a browser.

Logs go to `MobileApp/logs/` and process IDs to `MobileApp/logs/pids.json`, which `stop` uses. If port 3000 or 8083 is used by another program, `start` exits with a message.

If the app shows "offline", the backend is not running (for example after `stop`): run `status`, then `start`.

The frontend runs without file watching, so after changing frontend code run `stop` and then `start` again.

## Project layout

- `RaspberryPi/` — current application logic, traffic light command flow, and routing components
- `Jetson/` — Jetson-specific processing and camera pipeline pieces
- `MobileApp/` — mobile route app (frontend, backends, logs, and local start/stop scripts)
- `Launcher/` — Go source for the launcher
- `STM32/STM32CubeIDE/` — active embedded project for the STM32 sensor node and traffic analytics code
- `PythonSetup/` — dependencies for remaining optional Python cloud integrations
- `RaspberryPi/Native/` — portable C++17 edge tools for route calculation and telemetry ingestion
- `SimulatedDevicesPrototype/` — archived reference snapshot (git-ignored, not part of the live runtime)

## Important status notes

- The Arduino prototype is no longer part of the active hardware architecture; it is kept only as the git-ignored `SimulatedDevicesPrototype/` reference snapshot.
- The project intentionally keeps historical material around for reference, but the active development path is the Raspberry Pi + Jetson + STM32 + Azure workflow.
- MongoDB remains a compatibility-only option and is not the preferred live persistence layer.
- The project continues to support local execution without cloud credentials when those services are not configured.

## Notes

This README reflects the cleaned current workspace state: active project folders are preserved, the Arduino prototype is archived for reference only, and the practical working entry point is the Go launcher under `Launcher/`.

## Next steps

Suggested follow-up priorities for the project are:

- Build the Raspberry Pi native tools and Go launcher on clean Windows/Linux/Raspberry Pi environments, then validate native telemetry-to-route flow on hardware.
- Validate the live Raspberry Pi route recomputation using Jetson camera metadata and STM32 sensor telemetry on target hardware.
- Finalize the deployment configuration for Azure IoT Hub and Blob Storage, including environment variables, secure secrets management, and fallback behavior when cloud connectivity is unavailable.
- Expand the STM32 sensor-node integration by testing real traffic signal inputs, calibration thresholds, and fault handling for noisy or missing data.
- Add a lightweight native validation pipeline for route logic, telemetry parsing, and embedded C++ code.
- Document the actual runtime responsibilities of each subsystem so future contributors can tell which components are production-critical versus experimental.
- Consider a single reference architecture diagram or runbook that shows the full data flow from sensors to inference to traffic decisions and cloud logging.

These next steps will help turn the current workspace from a structured multi-device prototype into a more repeatable, testable, and deployment-ready traffic management system.
