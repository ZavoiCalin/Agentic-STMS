#ifndef HAL_SENSOR_NODE_H
#define HAL_SENSOR_NODE_H

#include "board_config.h"
#include "sensor_node.h"

typedef struct {
    ADC_HandleTypeDef *hadc_co;
    ADC_HandleTypeDef *hadc_rain;
    UART_HandleTypeDef *huart;
    TIM_HandleTypeDef *htim_ultrasonic;
    GPIO_TypeDef *dht_port;
    uint16_t dht_pin;
} stm32_sensor_node_t;

void stm32_sensor_node_init(stm32_sensor_node_t *node,
                           ADC_HandleTypeDef *hadc_co,
                           ADC_HandleTypeDef *hadc_rain,
                           UART_HandleTypeDef *huart,
                           TIM_HandleTypeDef *htim_ultrasonic);
void stm32_sensor_node_read_sample(stm32_sensor_node_t *node, sensor_sample_t *sample);
void stm32_sensor_node_send_json(stm32_sensor_node_t *node, const char *json_line);
float stm32_measure_distance_cm(GPIO_TypeDef *trig_port,
                               uint16_t trig_pin,
                               GPIO_TypeDef *echo_port,
                               uint16_t echo_pin,
                               TIM_HandleTypeDef *htim);

#endif
