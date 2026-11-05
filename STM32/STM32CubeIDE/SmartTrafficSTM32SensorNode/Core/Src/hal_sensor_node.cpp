#include "hal_sensor_node.h"

#include <string.h>

static uint32_t timer_elapsed_us(TIM_HandleTypeDef *htim, uint32_t start)
{
    return (uint16_t)(__HAL_TIM_GET_COUNTER(htim) - start);
}

static void delay_us(TIM_HandleTypeDef *htim, uint32_t delay)
{
    const uint32_t start = __HAL_TIM_GET_COUNTER(htim);
    while (timer_elapsed_us(htim, start) < delay) {
    }
}

static uint32_t adc_read_raw(ADC_HandleTypeDef *hadc, uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    uint32_t raw_value = 0U;

    if (hadc != NULL) {
        sConfig.Channel = channel;
        sConfig.Rank = ADC_REGULAR_RANK_1;
        sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES;
        sConfig.OffsetNumber = ADC_OFFSET_NONE;

        if (HAL_ADC_ConfigChannel(hadc, &sConfig) == HAL_OK &&
            HAL_ADC_Start(hadc) == HAL_OK) {
            if (HAL_ADC_PollForConversion(hadc, 10U) == HAL_OK) {
                raw_value = HAL_ADC_GetValue(hadc);
            }
            HAL_ADC_Stop(hadc);
        }
    }
    return raw_value;
}

static bool wait_for_pin(GPIO_TypeDef *port,
                         uint16_t pin,
                         GPIO_PinState state,
                         TIM_HandleTypeDef *htim,
                         uint32_t timeout_us)
{
    const uint32_t start = __HAL_TIM_GET_COUNTER(htim);
    while (HAL_GPIO_ReadPin(port, pin) != state) {
        if (timer_elapsed_us(htim, start) >= timeout_us) {
            return false;
        }
    }
    return true;
}

static uint8_t digital_read(GPIO_TypeDef *port, uint16_t pin)
{
    if (port == NULL) {
        return 0U;
    }
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1U : 0U;
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
    node->first_vehicle_sensor_ms = 0U;
    node->detection_threshold_cm = 100U;
    node->sensor_distance_m = 0.20f;
    node->last_speed_mps = 0.0f;
    node->last_detected_sensors = 0U;
    node->first_vehicle_sensor = 0U;
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
    const uint32_t now_ms = HAL_GetTick();
    const uint8_t detected_sensors =
        ((distance_1_cm > 0.0f && distance_1_cm <= (float)node->detection_threshold_cm) ? 1U : 0U) |
        ((distance_2_cm > 0.0f && distance_2_cm <= (float)node->detection_threshold_cm) ? 2U : 0U);
    const uint8_t new_detections = detected_sensors & (uint8_t)~node->last_detected_sensors;

    for (uint8_t sensor = 1U; sensor <= 2U; sensor <<= 1U) {
        if ((new_detections & sensor) == 0U) {
            continue;
        }
        if (node->first_vehicle_sensor == 0U || node->first_vehicle_sensor == sensor) {
            node->first_vehicle_sensor = sensor;
            node->first_vehicle_sensor_ms = now_ms;
        } else {
            const uint32_t elapsed_ms = now_ms - node->first_vehicle_sensor_ms;
            if (elapsed_ms >= 4U && elapsed_ms <= 2000U) {
                node->last_speed_mps = node->sensor_distance_m / ((float)elapsed_ms / 1000.0f);
            }
            node->first_vehicle_sensor = sensor;
            node->first_vehicle_sensor_ms = now_ms;
        }
    }
    node->last_detected_sensors = detected_sensors;

    sample->co = (int)co_raw;
    sample->average_speed_mps = node->last_speed_mps;
    sample->distance_1_cm = distance_1_cm;
    sample->distance_2_cm = distance_2_cm;
    sample->rain_analog = (int)rain_raw;
    sample->rain_digital = digital_read(RAIN_DIGITAL_PORT, RAIN_DIGITAL_PIN);
    sample->timestamp_ms = now_ms;
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

    if (htim == NULL) {
        return 0.0f;
    }

    HAL_GPIO_WritePin(trig_port, trig_pin, GPIO_PIN_RESET);
    delay_us(htim, 2U);
    HAL_GPIO_WritePin(trig_port, trig_pin, GPIO_PIN_SET);
    delay_us(htim, 10U);
    HAL_GPIO_WritePin(trig_port, trig_pin, GPIO_PIN_RESET);

    if (!wait_for_pin(echo_port, echo_pin, GPIO_PIN_SET, htim, 30000U)) {
        return 0.0f;
    }

    const uint32_t pulse_start = __HAL_TIM_GET_COUNTER(htim);
    if (!wait_for_pin(echo_port, echo_pin, GPIO_PIN_RESET, htim, 30000U)) {
        return 0.0f;
    }

    const uint32_t pulse_us = timer_elapsed_us(htim, pulse_start);
    return (float)pulse_us / 58.0f;
}
