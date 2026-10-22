# SmartTrafficSTM32SensorNode

This folder is a CubeIDE-style sensor-node skeleton. The application entry point, sensor model, HAL-facing sensor acquisition, and traffic analytics are C++; CubeIDE-style interrupt handlers and startup code remain C/assembly, as is normal for generated STM32 projects.

## Purpose

The firmware reads the same sensor inputs that the legacy Arduino sketch used:

- CO analog sensor
- DHT temperature and humidity
- rain sensor (analog + digital)
- speed estimation using two HC-SR04 ultrasonic sensors

It also includes the traffic analytics engine used to classify lane congestion, determine the dominant lane, and build the MQTT-style payload that the Raspberry Pi can consume.

## NUCLEO-N657X0-Q HAL target

This project is now aligned to the STM32 NUCLEO-N657X0-Q board and uses a HAL-style initialization flow for:

- ADC conversion for CO and rain analog inputs
- UART for Bluetooth/UART telemetry to the Raspberry Pi
- GPIO for rain digital input, DHT signal, and ultrasonic trigger/echo
- timer-driven ultrasonic distance measurement

## Concrete board mapping used in the HAL skeleton

This is the current working example mapping for the Nucleo target:

- CO analog sensor: PC0 (ADC1/ADC10)
- Rain analog sensor: PC1 (ADC2/ADC11)
- Rain digital sensor: PB0
- DHT11 data: PC7
- Ultrasonic trigger 1: PA8
- Ultrasonic echo 1: PA9
- Ultrasonic trigger 2: PA10
- Ultrasonic echo 2: PA11
- Bluetooth UART: PA2 / PA3 -> USART2

Adjust the final real pin mapping to the exact wiring harness and connector layout used on your board.

## Merged functionality

- sensor acquisition and JSON telemetry via Bluetooth/UART
- queue estimation and congestion scoring
- traffic-state classification (FREE, LIGHT, MODERATE, HEAVY, CRITICAL)
- dominant-lane scheduling decision
- MQTT payload generation compatible with the edge-gateway design

## Expected serial payload format

{"co":410,"temperature":22.0,"humidity":43.0,"average_speed":0.20,"rainAnalog":435,"rainDigital":0}

This JSON-lines format is compatible with the native Raspberry Pi `receive-bluetooth` receiver, which writes `arduino_data.jsonl` at the project root for route calculations.

## Generated-style layout

- Core/Inc/board_config.h
- Core/Inc/sensor_node.h
- Core/Inc/traffic_analytics.h
- Core/Inc/hal_sensor_node.h
- Core/Inc/stm32n6xx_hal_conf.h
- Core/Inc/main.h
- Core/Inc/stm32n6xx_it.h
- Core/Src/main.cpp
- Core/Src/sensor_node.cpp
- Core/Src/hal_sensor_node.cpp
- Core/Src/traffic_analytics.cpp
- Core/Src/system_stm32n6xx.c
- Core/Src/stm32n6xx_it.c
- Core/Startup/startup_stm32n657xx.s
- Drivers/CMSIS/Device/ST/STM32N6xx/Include/system_stm32n6xx.h
- Drivers/STM32N6xx_HAL_Driver/Inc/stm32n6xx_hal.h

The Makefile builds the C++ sensor/analytics library. This repository does not include a complete STM32CubeIDE project or the vendor HAL implementation, so it cannot yet produce flashable firmware. Generate/import the full STM32N6 HAL project, include the four C++ application sources, and link the generated C HAL files with C linkage before flashing. `SystemClock_Config`, DHT11 reads, and lane samples remain placeholders and need board configuration and real sensor readings.
