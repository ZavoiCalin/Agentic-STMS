# SmartTrafficSTM32SensorNode

This folder is a CubeIDE-style sensor-node skeleton. The active application reads the three analog outputs of a MICS6814 and sends them over Bluetooth UART. Other sensor and traffic-analytics sources are retained for separate development but are not used by the active main loop.

## Purpose

The active firmware reads:

- MICS6814 CO, NH3, and NO2 analog outputs

## NUCLEO-N657X0-Q HAL target

This project is now aligned to the STM32 NUCLEO-N657X0-Q board and uses a HAL-style initialization flow for:

- ADC conversion for the three MICS6814 analog outputs
- UART for Bluetooth/UART telemetry to the Raspberry Pi

## Example pin mapping

The current example assumes these analog outputs are connected to ADC1:

- CO analog output: PC0 (ADC channel 10)
- NH3 analog output: PC2 (ADC channel 12)
- NO2 analog output: PC3 (ADC channel 13)
- Bluetooth UART: PA2 / PA3 -> USART2

Verify these pins against the board variant and wiring. Ensure each sensor output stays within the STM32 ADC input-voltage limit; use a suitable divider if the module can output more than the ADC reference voltage.

The USART2 link uses 9600 baud, 8-N-1. Connect the Bluetooth module's RX to STM32 TX and its TX to STM32 RX, with a shared ground and compatible logic levels.

## Telemetry

One JSON line is sent per second:

{"co":1.23,"nh3":2.34,"no2":0.56}

The JSON-lines format matches the native Raspberry Pi Bluetooth receiver. The source currently uses provisional voltage-to-concentration scaling; these values are not calibrated ppm readings. Calibrate each sensor channel with the actual module/load resistors and the sensor's baseline before using the values as gas concentrations.

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

The Makefile builds the C++ sensor/analytics library. This repository does not include a complete STM32CubeIDE project or the vendor HAL implementation, so it cannot yet produce flashable firmware. Generate/import the full STM32N6 HAL project, include the active application and MICS6814 sources, and link the generated C HAL files with C linkage before flashing. The clock setup and ADC/pin mapping must be completed and verified for the exact board. Hardware operation and gas calibration still require on-board validation.
