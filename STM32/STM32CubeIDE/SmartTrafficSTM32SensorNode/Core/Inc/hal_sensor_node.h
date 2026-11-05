#ifndef HAL_SENSOR_NODE_H
#define HAL_SENSOR_NODE_H

#include "board_config.h"
#include "sensor_node.h"

typedef struct {
    ADC_HandleTypeDef *hadc_co;
    ADC_HandleTypeDef *hadc_rain;
    UART_HandleTypeDef *huart;
    TIM_HandleTypeDef *htim_ultrasonic;
    uint32_t first_vehicle_sensor_ms;
    uint32_t detection_threshold_cm;
    float sensor_distance_m;
    float last_speed_mps;
    uint8_t last_detected_sensors;
    uint8_t first_vehicle_sensor;
} stm32_sensor_node_t;

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
