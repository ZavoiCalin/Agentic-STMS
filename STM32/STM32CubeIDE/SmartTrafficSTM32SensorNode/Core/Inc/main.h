#ifndef MAIN_H
#define MAIN_H

#include "board_config.h"
#include "hal_sensor_node.h"
#include "traffic_analytics.h"

#include <stddef.h>
#include <stdint.h>

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_ADC_Init(void);
void MX_USART2_UART_Init(void);
void MX_TIM1_Init(void);

#endif
