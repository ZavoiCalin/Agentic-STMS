#ifndef MICS6814_H
#define MICS6814_H

#include "board_config.h"

/* Adjust to the actual wiring / ADC channels of your board. */
#define MICS_NH3_ADC_CHANNEL ADC_CHANNEL_12
#define MICS_NH3_PORT GPIOC
#define MICS_NH3_PIN GPIO_PIN_2
#define MICS_NO2_ADC_CHANNEL ADC_CHANNEL_13
#define MICS_NO2_PORT GPIOC
#define MICS_NO2_PIN GPIO_PIN_3

#define MICS_VREF 3.3f
#define MICS_ADC_MAX 4095.0f
/* Sensor module outputs 0-5 V; with a divider R1/R2 the ADC sees Vout*R2/(R1+R2).
   Set this to (R1+R2)/R2 (e.g. 1.5 for 10k/20k), or 1.0 if no divider. */
#define MICS_DIVIDER_RATIO 1.5f

#define MICS_JSON_SIZE 96

#ifdef __cplusplus
extern "C" {
#endif

void mics6814_gpio_init(void);
/* Reads CO/NH3/NO2 (ppm, same approximation as the Arduino sketch) and writes
   {"co":x.xx,"nh3":x.xx,"no2":x.xx}\n into buf. */
void mics6814_read_json(ADC_HandleTypeDef *hadc, char *buf, size_t buf_size);
void mics6814_send(UART_HandleTypeDef *huart, const char *json);

#ifdef __cplusplus
}
#endif

#endif
