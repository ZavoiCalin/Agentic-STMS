# Project Status

Date: 2026-10-20

## Smart Traffic Management System — Current Status

This project is currently positioned as a hardware-first smart traffic management platform designed around the real deployment architecture rather than the earlier simulated prototype. The system integrates air-quality sensing, camera-driven traffic analytics, adaptive routing logic, and edge-to-cloud coordination for intelligent traffic optimization.

The design now centers on a Raspberry Pi 5 as the central coordinator and edge gateway, an NVIDIA Jetson device for camera-based perception and traffic-state estimation, and an STM32 NUCLEO-N657X0-Q sensor node replacing the legacy Arduino-based sensing layer. Sensor telemetry, Bluetooth/UART communication, and traffic-state classification are handled in the embedded stage, while the Raspberry Pi coordinates orchestration, local processing, and optional cloud integration.

The project has been reorganized to reflect this active architecture. The implementation is now structured around the functional layers required for the real system:

- Raspberry Pi layer for orchestration, routing, cloud communication, and local data pipelines
- Jetson layer for computer-vision-based traffic analytics and vehicle flow detection
- STM32 layer for embedded sensor acquisition, telemetry, and traffic analytics logic
- Azure IoT Hub and Azure Blob Storage for cloud message flow and persistence
- Python local tooling for development validation and route simulation in the current environment

The older simulated Azure IoT Hub + MongoDB architecture remains part of the historical background of the project and is retained only as a reference for legacy design comparison. MongoDB is treated as compatibility-only infrastructure rather than the active persistence choice. The Arduino implementation is no longer considered part of the current runtime architecture and has been removed from the active project structure.

## Current maturity

The project is in an active development state with a clear architectural direction and a working local validation path. The Python-based route logic is successfully executable in the current environment using a dedicated virtual environment and PowerShell launcher. This ensures the project remains runnable without full cloud or hardware dependencies while the embedded and deployment layers are being developed.

The embedded STM32 path has been migrated to a CubeIDE-style project structure aligned with the NUCLEO-N657X0-Q target. This includes the sensor-node logic, board configuration, traffic analytics logic, and UART/Bluetooth-oriented telemetry format intended to match the hardware-first system design. The project is structurally complete for import into STM32CubeIDE, but final board-specific validation, pin mapping confirmation, and firmware build/test remain dependent on the real hardware toolchain.

## Technical direction

The current project direction is best described as a hybrid edge AI and cloud-enabled traffic management system with emphasis on:

- real-time route optimization using traffic and environmental inputs
- camera-based vehicle counting and congestion detection on Jetson hardware
- distributed sensing and embedded analytics on STM32 hardware
- central orchestration on Raspberry Pi 5
- optional Azure-based cloud communication and blob-backed persistence
- local development support without requiring full cloud credentials or physical hardware

## CV-style summary

Smart Traffic Management System researcher and developer focused on edge AI, embedded sensing, and adaptive traffic optimization. The project evolved from an initial Azure-based simulated IoT prototype into a hardware-first architecture based on Raspberry Pi, Jetson, and STM32 platforms. The current implementation emphasizes real-world traffic analytics, sensor-driven environmental monitoring, and AI-based routing through a distributed computing architecture. Work includes orchestration logic, local route validation, embedded telemetry design, and Azure-ready cloud integration for persistence and communication.

## Overall state

Status: In active development, architecture stabilized, local system validation working, embedded hardware integration in progress.

The project is no longer aligned to the original simulated prototype as the primary implementation. It follows the current hardware-first design and remains structured for future deployment, integration, and validation on the actual edge devices.
