#include "mics6814.h"

#include <stdio.h>
#include <string.h>

static bool read_voltage(ADC_HandleTypeDef *hadc, uint32_t channel, float *voltage)
{
    if (hadc == NULL || voltage == NULL) {
        return false;
    }

    ADC_ChannelConfTypeDef cfg = {0};
    cfg.Channel = channel;
    cfg.Rank = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = ADC_SAMPLETIME_28CYCLES;
    cfg.OffsetNumber = ADC_OFFSET_NONE;

    if (HAL_ADC_ConfigChannel(hadc, &cfg) != HAL_OK ||
        HAL_ADC_Start(hadc) != HAL_OK) {
        return false;
    }

    const bool conversion_ok = HAL_ADC_PollForConversion(hadc, 10U) == HAL_OK;
    const uint32_t raw = conversion_ok ? HAL_ADC_GetValue(hadc) : 0U;
    HAL_ADC_Stop(hadc);

    if (!conversion_ok) {
        return false;
    }
    *voltage = ((float)raw / MICS_ADC_MAX) * MICS_VREF * MICS_DIVIDER_RATIO;
    return true;
}

void mics6814_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;

    gpio.Pin = CO_ADC_PIN | MICS_NH3_PIN | MICS_NO2_PIN;
    HAL_GPIO_Init(MICS_NH3_PORT, &gpio);
}

void mics6814_read_json(ADC_HandleTypeDef *hadc, char *buf, size_t buf_size)
{
    if (hadc == NULL || buf == NULL || buf_size == 0U) {
        return;
    }

    float co_voltage = 0.0f;
    float nh3_voltage = 0.0f;
    float no2_voltage = 0.0f;
    const bool co_ok = read_voltage(hadc, ADC_CO_CHANNEL, &co_voltage);
    const bool nh3_ok = read_voltage(hadc, MICS_NH3_ADC_CHANNEL, &nh3_voltage);
    const bool no2_ok = read_voltage(hadc, MICS_NO2_ADC_CHANNEL, &no2_voltage);
    const float co_ppm_estimate = co_voltage * 100.0f;
    const float nh3_ppm_estimate = nh3_voltage * 300.0f;
    const float no2_ppm_estimate = no2_voltage;

    /* Requires newlib-nano float printf (-u _printf_float) in linker flags.
       Concentration scaling is approximate and must be calibrated for the module. */
    if (co_ok && nh3_ok && no2_ok) {
        snprintf(buf, buf_size, "{\"co\":%.2f,\"nh3\":%.2f,\"no2\":%.2f}\r\n",
                 (double)co_ppm_estimate, (double)nh3_ppm_estimate,
                 (double)no2_ppm_estimate);
    } else {
        snprintf(buf, buf_size, "{\"co\":null,\"nh3\":null,\"no2\":null}\r\n");
    }
}

void mics6814_send(UART_HandleTypeDef *huart, const char *json)
{
    if (huart == NULL || json == NULL) {
        return;
    }
    HAL_UART_Transmit(huart, (uint8_t *)json, (uint16_t)strlen(json), 100U);
}
