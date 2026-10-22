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
- Native C++ Raspberry Pi route and telemetry processing, with optional Python cloud adapters

The older simulated Azure IoT Hub + MongoDB architecture remains part of the historical background of the project and is retained only as a reference for legacy design comparison. MongoDB is treated as compatibility-only infrastructure rather than the active persistence choice. The Arduino implementation is no longer considered part of the current runtime architecture and has been removed from the active project structure.

## Current maturity

The project is in active development with a native C++ Raspberry Pi route and telemetry path. The Go launcher dispatches route, traffic, and receiver commands to the native executable. Azure/Blob/Mongo compatibility tooling remains Python-based and optional pending native SDK migration.

STM32 application, sensor acquisition, and analytics sources are C++. CubeIDE-style interrupt/startup C and assembly remain. The repository only contains a HAL compatibility stub, not the vendor implementation or a complete CubeIDE project, so producing flashable firmware and board-level validation require the generated NUCLEO-N657X0-Q project and physical hardware.

## Technical direction

The current project direction is best described as a hybrid edge AI and cloud-enabled traffic management system with emphasis on:

- real-time route optimization using Jetson detections and environmental inputs
- CUDA-backed camera inference on Jetson hardware
- distributed sensing and embedded analytics on STM32 hardware
- central orchestration on Raspberry Pi 5
- optional Azure-based cloud communication and blob-backed persistence
- local development support without requiring full cloud credentials or physical hardware

## CV-style summary

Smart Traffic Management System researcher and developer focused on edge AI, embedded sensing, and adaptive traffic optimization. The project evolved from an initial Azure-based simulated IoT prototype into a hardware-first architecture based on Raspberry Pi, Jetson, and STM32 platforms. The current implementation emphasizes real-world traffic analytics, sensor-driven environmental monitoring, and AI-based routing through a distributed computing architecture. Work includes orchestration logic, local route validation, embedded telemetry design, and Azure-ready cloud integration for persistence and communication.

## Overall state

Status: In active development, architecture stabilized, local system validation working, embedded hardware integration in progress.

The project is no longer aligned to the original simulated prototype as the primary implementation. It follows the current hardware-first design and remains structured for future deployment, integration, and validation on the actual edge devices.
