# Smart Traffic Management System

This workspace is organized around the active project structure and a small set of reference archives. The Arduino prototype and unused root-level launch files were removed, while the functional project areas remain separated by role.

## Active project areas

The active project folders are:

- RaspberryPi/ — edge coordination, route logic, traffic control scripts, and cloud/stream pipeline components
- Jetson/ — Jetson-specific camera and inference assets used alongside the Raspberry Pi runtime
- Extra/ — additional supporting tools, scripts, and experiments such as camera streaming, Azure blob helpers, and reference integrations
- STM32/STM32CubeIDE/ — the real STM32 project area used for embedded sensor-node development and traffic analytics logic
- PythonSetup/ — the current Windows launcher and venv/bootstrap area for local execution

The following are intentionally kept as reference or archived material, not as the live runtime:

- SimulatedDevicesPrototype/ — historical project snapshot retained for reference and legacy design comparison
- Extra/Edge_AI/ and similar reference builds — useful examples, not the primary active app

## Current architecture

The current design in this workspace is centered on the following flow:

- Raspberry Pi 5: central coordinator and edge gateway
- NVIDIA Jetson: camera-driven vehicle detection and traffic-state analytics
- STM32 sensor node (NUCLEO-N657X0-Q): embedded acquisition and traffic sensor telemetry; Arduino is no longer part of the active architecture
- Azure IoT Hub / Azure Blob Storage: optional cloud integration and persistence for the edge pipeline
- Python-based local tooling: route simulation, traffic logic evaluation, and project bootstrap

MongoDB is still kept as a legacy compatibility option, but it is not the preferred active persistence layer.

## Current local execution

The active launcher is:

- `PythonSetup/launch.ps1`

Use it from PowerShell in the project root, for example:

- `powershell -ExecutionPolicy Bypass -File .\PythonSetup\launch.ps1 -Command route`
- `powershell -ExecutionPolicy Bypass -File .\PythonSetup\launch.ps1 -Command traffic`
- `powershell -ExecutionPolicy Bypass -File .\PythonSetup\launch.ps1 -Command cloud`
- `powershell -ExecutionPolicy Bypass -File .\PythonSetup\launch.ps1 -Command jetson`
- `powershell -ExecutionPolicy Bypass -File .\PythonSetup\launch.ps1 -Command all`

The Python environment is expected to live under `.venv` and is bootstrapped by the PowerShell script when needed.

## Project layout

- `RaspberryPi/` — current application logic, traffic light command flow, and routing components
- `Jetson/` — Jetson-specific processing and camera pipeline pieces
- `Extra/` — additional support scripts, experiments, and integration examples
- `STM32/STM32CubeIDE/` — active embedded project for the STM32 sensor node and traffic analytics code
- `PythonSetup/` — environment setup and local command runner
- `PythonSetup/requirements.txt` — active Python dependencies for local tooling

## Important status notes

- The Arduino prototype folder was removed because it is no longer part of the active hardware architecture.
- The project intentionally keeps historical material around for reference, but the active development path is the Raspberry Pi + Jetson + STM32 + Azure workflow.
- MongoDB remains a compatibility-only option and is not the preferred live persistence layer.
- The project continues to support local execution without cloud credentials when those services are not configured.

## Notes

This README reflects the cleaned current workspace state: active project folders are preserved, obsolete Arduino artifacts are removed, and the practical working entry point is the Windows launcher under `PythonSetup/`.

## Next steps

Suggested follow-up priorities for the project are:

- Validate the end-to-end startup flow for all launcher modes (`route`, `traffic`, `cloud`, `jetson`, `all`) and confirm the Python environment bootstrap is reliable on clean Windows machines.
- Tighten the Raspberry Pi coordination logic by defining the message contracts between the edge controller, Jetson inference output, and the STM32 sensor telemetry stream.
- Finalize the deployment configuration for Azure IoT Hub and Blob Storage, including environment variables, secure secrets management, and fallback behavior when cloud connectivity is unavailable.
- Expand the STM32 sensor-node integration by testing real traffic signal inputs, calibration thresholds, and fault handling for noisy or missing data.
- Add a lightweight validation pipeline for Python and embedded code, such as unit tests for route logic, linting for scripts, and a CI step that checks the project still boots cleanly.
- Document the actual runtime responsibilities of each subsystem so future contributors can tell which components are production-critical versus experimental.
- Consider a single reference architecture diagram or runbook that shows the full data flow from sensors to inference to traffic decisions and cloud logging.

These next steps will help turn the current workspace from a structured multi-device prototype into a more repeatable, testable, and deployment-ready traffic management system.
