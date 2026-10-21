#include "sensor_node.h"

#include <stdio.h>
#include <string.h>

static int mock_analog_read(int pin)
{
    (void)pin;
    return 410 + ((pin * 37U) % 200U);
}

static int mock_digital_read(int pin)
{
    (void)pin;
    return ((pin * 17U) % 2U);
}

static float measure_distance_cm(int trig_pin, int echo_pin)
{
    (void)trig_pin;
    (void)echo_pin;
    return 18.0f + ((trig_pin * 9U + echo_pin * 5U) % 42U);
}

static float compute_vehicle_speed_mps(float distance_1_cm, float distance_2_cm, float sensor_distance_m)
{
    (void)sensor_distance_m;
    if (distance_1_cm <= 0.0f || distance_2_cm <= 0.0f) {
        return 0.0f;
    }

    const float delta_cm = (distance_1_cm + distance_2_cm) * 0.5f;
    if (delta_cm <= 0.0f) {
        return 0.0f;
    }

    const float speed_mps = (sensor_distance_m * 100.0f) / (delta_cm / 20.0f + 0.15f);
    return speed_mps > 0.0f ? speed_mps : 0.0f;
}

void sensor_node_init(sensor_board_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    cfg->co_pin = 0;
    cfg->temp_pin = 2;
    cfg->rain_analog_pin = 1;
    cfg->rain_digital_pin = 3;
    cfg->trig_pin_1 = 5;
    cfg->echo_pin_1 = 4;
    cfg->trig_pin_2 = 6;
    cfg->echo_pin_2 = 7;
    cfg->sensor_distance_m = 0.20f;
    cfg->detection_threshold_cm = 100U;
}

void sensor_node_sample(sensor_sample_t *sample, const sensor_board_config_t *cfg)
{
    if (sample == NULL || cfg == NULL) {
        return;
    }

    const float distance_1_cm = measure_distance_cm(cfg->trig_pin_1, cfg->echo_pin_1);
    const float distance_2_cm = measure_distance_cm(cfg->trig_pin_2, cfg->echo_pin_2);
    const float speed_mps = compute_vehicle_speed_mps(distance_1_cm, distance_2_cm, cfg->sensor_distance_m);

    sample->co = mock_analog_read(cfg->co_pin);
    sample->temperature_c = 22.0f + (float)(cfg->temp_pin % 7U) * 0.8f;
    sample->humidity_pct = 43.0f + (float)(cfg->temp_pin % 9U) * 2.5f;
    sample->average_speed_mps = speed_mps;
    sample->rain_analog = mock_analog_read(cfg->rain_analog_pin);
    sample->rain_digital = mock_digital_read(cfg->rain_digital_pin);
    sample->timestamp_ms = 0U;
}

void sensor_node_format_json(const sensor_sample_t *sample, char *buffer, size_t buffer_size)
{
    if (sample == NULL || buffer == NULL || buffer_size == 0U) {
        return;
    }

    snprintf(buffer,
             buffer_size,
             "{\"co\":%d,\"temperature\":%.1f,\"humidity\":%.1f,\"average_speed\":%.2f,\"rainAnalog\":%d,\"rainDigital\":%d}\n",
             sample->co,
             sample->temperature_c,
             sample->humidity_pct,
             sample->average_speed_mps,
             sample->rain_analog,
             sample->rain_digital);
}

void sensor_node_bluetooth_send(const char *json_line)
{
    if (json_line == NULL) {
        return;
    }

    printf("%s", json_line);
}
