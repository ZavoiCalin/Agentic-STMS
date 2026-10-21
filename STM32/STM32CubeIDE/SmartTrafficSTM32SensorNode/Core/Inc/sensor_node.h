#ifndef SENSOR_NODE_H
#define SENSOR_NODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SENSOR_NODE_JSON_SIZE 256U

typedef struct {
    int co_pin;
    int temp_pin;
    int rain_analog_pin;
    int rain_digital_pin;
    int trig_pin_1;
    int echo_pin_1;
    int trig_pin_2;
    int echo_pin_2;
    float sensor_distance_m;
    uint32_t detection_threshold_cm;
} sensor_board_config_t;

typedef struct {
    int co;
    float temperature_c;
    float humidity_pct;
    float average_speed_mps;
    int rain_analog;
    int rain_digital;
    uint32_t timestamp_ms;
} sensor_sample_t;

void sensor_node_init(sensor_board_config_t *cfg);
void sensor_node_sample(sensor_sample_t *sample, const sensor_board_config_t *cfg);
void sensor_node_format_json(const sensor_sample_t *sample, char *buffer, size_t buffer_size);
void sensor_node_bluetooth_send(const char *json_line);

#endif /* SENSOR_NODE_H */
