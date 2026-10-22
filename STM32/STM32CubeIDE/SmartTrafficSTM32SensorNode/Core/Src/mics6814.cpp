#include "mics6814.h"

#include <stdio.h>
#include <string.h>

static float read_voltage(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    ADC_ChannelConfTypeDef cfg = {0};
    cfg.Channel = channel;
    cfg.Rank = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = ADC_SAMPLETIME_28CYCLES;
    cfg.OffsetNumber = ADC_OFFSET_NONE;

    HAL_ADC_ConfigChannel(hadc, &cfg);
    HAL_ADC_Start(hadc);
    uint32_t raw = 0U;
    if (HAL_ADC_PollForConversion(hadc, 10U) == HAL_OK) {
        raw = HAL_ADC_GetValue(hadc);
    }
    HAL_ADC_Stop(hadc);

    return ((float)raw / MICS_ADC_MAX) * MICS_VREF * MICS_DIVIDER_RATIO;
}

void mics6814_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;

    gpio.Pin = MICS_NH3_PIN;
    HAL_GPIO_Init(MICS_NH3_PORT, &gpio);
    gpio.Pin = MICS_NO2_PIN;
    HAL_GPIO_Init(MICS_NO2_PORT, &gpio);
}

void mics6814_read_json(ADC_HandleTypeDef *hadc, char *buf, size_t buf_size)
{
    if (hadc == NULL || buf == NULL || buf_size == 0U) {
        return;
    }

    const float co_ppm = read_voltage(hadc, ADC_CO_CHANNEL) * 100.0f;
    const float nh3_ppm = read_voltage(hadc, MICS_NH3_ADC_CHANNEL) * 300.0f;
    const float no2_ppm = read_voltage(hadc, MICS_NO2_ADC_CHANNEL) * 1.0f;

    /* Requires newlib-nano float printf (-u _printf_float) in linker flags. */
    snprintf(buf, buf_size, "{\"co\":%.2f,\"nh3\":%.2f,\"no2\":%.2f}\r\n",
             (double)co_ppm, (double)nh3_ppm, (double)no2_ppm);
}

void mics6814_send(UART_HandleTypeDef *huart, const char *json)
{
    if (huart == NULL || json == NULL) {
        return;
    }
    HAL_UART_Transmit(huart, (uint8_t *)json, (uint16_t)strlen(json), 100U);
}
