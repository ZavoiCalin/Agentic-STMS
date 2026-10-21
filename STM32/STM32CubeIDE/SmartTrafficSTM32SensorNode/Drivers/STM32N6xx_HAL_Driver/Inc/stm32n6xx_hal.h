#ifndef STM32N6XX_HAL_H
#define STM32N6XX_HAL_H

#include "stm32n6xx_hal_conf.h"

/* This file is a compatibility stub for a generated STM32CubeIDE project.
   In the real board environment, replace this with the actual STM32 HAL package. */

#include <stdint.h>

#define GPIO_PIN_RESET 0U
#define GPIO_PIN_SET 1U
#define GPIO_MODE_INPUT 0U
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_ANALOG 3U
#define GPIO_MODE_AF_PP 2U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_LOW 0U
#define GPIO_SPEED_FREQ_HIGH 1U
#define UART_MODE_TX_RX 0U
#define UART_WORDLENGTH_8B 0U
#define UART_STOPBITS_1 0U
#define UART_PARITY_NONE 0U
#define UART_HWCONTROL_NONE 0U
#define UART_OVERSAMPLING_16 0U
#define ADC_CLOCK_SYNC_PCLK_DIV4 0U
#define ADC_RESOLUTION_12B 0U
#define ADC_EOC_SINGLE_CONV 0U
#define ADC_DATAALIGN_RIGHT 0U
#define ADC_REGULAR_RANK_1 0U
#define ADC_SAMPLETIME_28CYCLES 0U
#define ADC_OFFSET_NONE 0U
#define TIM_COUNTERMODE_UP 0U
#define TIM_CLOCKDIVISION_DIV1 0U
#define GPIOA ((GPIO_TypeDef *)0x1)
#define GPIOB ((GPIO_TypeDef *)0x2)
#define GPIOC ((GPIO_TypeDef *)0x3)
#define ADC1 ((ADC_TypeDef *)0x11)
#define ADC2 ((ADC_TypeDef *)0x12)
#define USART2 ((UART_HandleTypeDef *)0x21)
#define TIM1 ((TIM_HandleTypeDef *)0x31)

typedef struct GPIO_TypeDef { uint32_t dummy; } GPIO_TypeDef;
typedef struct ADC_TypeDef { uint32_t dummy; } ADC_TypeDef;
typedef struct UART_HandleTypeDef { uint32_t dummy; } UART_HandleTypeDef;
typedef struct TIM_HandleTypeDef { uint32_t dummy; } TIM_HandleTypeDef;
typedef struct ADC_ChannelConfTypeDef { uint32_t Channel; uint32_t Rank; uint32_t SamplingTime; uint32_t OffsetNumber; } ADC_ChannelConfTypeDef;
typedef struct GPIO_InitTypeDef { uint32_t Pin; uint32_t Mode; uint32_t Pull; uint32_t Speed; } GPIO_InitTypeDef;

#define __HAL_RCC_GPIOA_CLK_ENABLE() do {} while (0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() do {} while (0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() do {} while (0)
#define __HAL_RCC_ADC1_CLK_ENABLE() do {} while (0)
#define __HAL_RCC_ADC2_CLK_ENABLE() do {} while (0)
#define __HAL_RCC_USART2_CLK_ENABLE() do {} while (0)
#define __HAL_RCC_TIM1_CLK_ENABLE() do {} while (0)
#define __HAL_LOCK(h) do {} while (0)
#define __HAL_UNLOCK(h) do {} while (0)
#define __HAL_GPIO_EXTI_CLEAR_IT(__EXTI_LINE__) do {} while (0)
#define __HAL_GPIO_EXTI_GET_FLAG(__EXTI_LINE__) (0U)
#define __HAL_GPIO_EXTI_GET_IT(__EXTI_LINE__) (0U)
#define __HAL_GPIO_EXTI_CLEAR_FLAG(__EXTI_LINE__) do {} while (0)
#define __HAL_ADC_READY(__HANDLE__) (1U)
#define __HAL_TIM_ENABLE(__HANDLE__) do {} while (0)
#define __HAL_TIM_DISABLE(__HANDLE__) do {} while (0)

#define ADC_CHANNEL_10 10U
#define ADC_CHANNEL_11 11U
#define GPIO_PIN_0 0x0001U
#define GPIO_PIN_1 0x0002U
#define GPIO_PIN_2 0x0004U
#define GPIO_PIN_3 0x0008U
#define GPIO_PIN_7 0x0080U
#define GPIO_PIN_8 0x0100U
#define GPIO_PIN_9 0x0200U
#define GPIO_PIN_10 0x0400U
#define GPIO_PIN_11 0x0800U

void HAL_Init(void);
void HAL_Delay(uint32_t ms);
uint32_t HAL_GetTick(void);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, uint8_t value);
uint8_t HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);
void HAL_ADC_Init(ADC_TypeDef *hadc);
void HAL_ADC_ConfigChannel(ADC_TypeDef *hadc, ADC_ChannelConfTypeDef *config);
void HAL_ADC_Start(ADC_TypeDef *hadc);
void HAL_ADC_Stop(ADC_TypeDef *hadc);
void HAL_ADC_PollForConversion(ADC_TypeDef *hadc, uint32_t timeout);
uint32_t HAL_ADC_GetValue(ADC_TypeDef *hadc);
void HAL_UART_Init(UART_HandleTypeDef *huart);
void HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout);
void HAL_TIM_Base_Init(TIM_HandleTypeDef *htim);
void HAL_IncTick(void);

#endif
