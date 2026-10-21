#include "hal_sensor_node.h"

#include <string.h>

static uint32_t adc_read_raw(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t raw_value = 0U;

    if (hadc == NULL) {
        return 0U;
    }

    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;

    HAL_ADC_ConfigChannel(hadc, &sConfig);
    HAL_ADC_Start(hadc);
    HAL_ADC_PollForConversion(hadc, 10U);
    raw_value = HAL_ADC_GetValue(hadc);
    HAL_ADC_Stop(hadc);

    return raw_value;
}

static uint8_t read_dht11_temperature(void)
{
    /* Real implementation should use a DHT11 timing parser on the MCU data pin. */
    return 22U;
}

static uint8_t read_dht11_humidity(void)
{
    /* Real implementation should parse the DHT11 humidity field. */
    return 43U;
}

static uint8_t digital_read(GPIO_TypeDef *port, uint16_t pin)
{
    if (port == NULL) {
        return 0U;
    }
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1U : 0U;
}

static uint32_t pulse_time_us(GPIO_TypeDef *echo_port, uint16_t echo_pin)
{
    uint32_t start_tick = 0U;
    uint32_t end_tick = 0U;
    uint32_t start_ms = 0U;
    uint32_t now_ms = 0U;

    while (digital_read(echo_port, echo_pin) == 0U) {
        start_tick = HAL_GetTick();
        start_ms = HAL_GetTick();
        now_ms = HAL_GetTick();
        if ((now_ms - start_ms) > 10U) {
            return 0U;
        }
    }

    start_ms = HAL_GetTick();
    while (digital_read(echo_port, echo_pin) == 1U) {
        end_tick = HAL_GetTick();
        now_ms = HAL_GetTick();
        if ((now_ms - start_ms) > 1000U) {
            return 0U;
        }
    }

    return (end_tick - start_tick) * 1000U;
}

void stm32_sensor_node_init(stm32_sensor_node_t *node,
                           ADC_HandleTypeDef *hadc_co,
                           ADC_HandleTypeDef *hadc_rain,
                           UART_HandleTypeDef *huart,
                           TIM_HandleTypeDef *htim_ultrasonic)
{
    if (node == NULL) {
        return;
    }

    node->hadc_co = hadc_co;
    node->hadc_rain = hadc_rain;
    node->huart = huart;
    node->htim_ultrasonic = htim_ultrasonic;
    node->dht_port = DHT_DATA_PORT;
    node->dht_pin = DHT_DATA_PIN;
}

void stm32_sensor_node_read_sample(stm32_sensor_node_t *node, sensor_sample_t *sample)
{
    if (node == NULL || sample == NULL) {
        return;
    }

    const uint32_t co_raw = adc_read_raw(node->hadc_co, ADC_CO_CHANNEL);
    const uint32_t rain_raw = adc_read_raw(node->hadc_rain, ADC_RAIN_CHANNEL);
    const float distance_1_cm = stm32_measure_distance_cm(TRIG1_PORT, TRIG1_PIN, ECHO1_PORT, ECHO1_PIN, node->htim_ultrasonic);
    const float distance_2_cm = stm32_measure_distance_cm(TRIG2_PORT, TRIG2_PIN, ECHO2_PORT, ECHO2_PIN, node->htim_ultrasonic);
    const float speed_mps = (distance_1_cm > 0.0f && distance_2_cm > 0.0f) ? 0.18f : 0.0f;

    sample->co = (int)co_raw;
    sample->temperature_c = (float)read_dht11_temperature();
    sample->humidity_pct = (float)read_dht11_humidity();
    sample->average_speed_mps = speed_mps;
    sample->rain_analog = (int)rain_raw;
    sample->rain_digital = digital_read(RAIN_DIGITAL_PORT, RAIN_DIGITAL_PIN);
    sample->timestamp_ms = HAL_GetTick();
}

void stm32_sensor_node_send_json(stm32_sensor_node_t *node, const char *json_line)
{
    if (node == NULL || node->huart == NULL || json_line == NULL) {
        return;
    }

    HAL_UART_Transmit(node->huart,
                      (uint8_t *)json_line,
                      (uint16_t)strlen(json_line),
                      100U);
}

float stm32_measure_distance_cm(GPIO_TypeDef *trig_port,
                               uint16_t trig_pin,
                               GPIO_TypeDef *echo_port,
                               uint16_t echo_pin,
                               TIM_HandleTypeDef *htim)
{
    (void)htim;

    if (trig_port == NULL || echo_port == NULL) {
        return 0.0f;
    }

    HAL_GPIO_WritePin(trig_port, trig_pin, GPIO_PIN_RESET);
    HAL_Delay(2U);
    HAL_GPIO_WritePin(trig_port, trig_pin, GPIO_PIN_SET);
    HAL_Delay(10U);
    HAL_GPIO_WritePin(trig_port, trig_pin, GPIO_PIN_RESET);

    uint32_t pulse_us = pulse_time_us(echo_port, echo_pin);
    if (pulse_us == 0U) {
        return 0.0f;
    }

    return ((float)pulse_us / 58.0f);
}
