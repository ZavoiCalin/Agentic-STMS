#ifndef TRAFFIC_ANALYTICS_H
#define TRAFFIC_ANALYTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TRAFFIC_LANE_COUNT 4U

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TRAFFIC_STATE_FREE = 0,
    TRAFFIC_STATE_LIGHT,
    TRAFFIC_STATE_MODERATE,
    TRAFFIC_STATE_HEAVY,
    TRAFFIC_STATE_CRITICAL
} traffic_state_t;

typedef struct {
    uint32_t vehicle_count;
    uint32_t occupancy_percent;
    uint32_t queue_length;
    uint32_t flow_rate_vpm;
    uint32_t average_speed_kmh;
    bool emergency_vehicle_detected;
} lane_sample_t;

typedef struct {
    float queue_estimate;
    float congestion_score;
    float predicted_wait_time_s;
    traffic_state_t state;
    uint32_t dominant_lane;
    bool adaptive_cycle_enabled;
    uint32_t scheduling_period_ms;
    uint32_t task_jitter_ms;
} traffic_decision_t;

typedef struct {
    lane_sample_t lanes[TRAFFIC_LANE_COUNT];
    float queue_factor;
    float inflow_factor;
    float occupancy_factor;
    uint32_t task_jitter_ms;
    uint32_t cycle_period_ms;
    bool inference_active;
    bool emergency_mode;
    uint32_t sample_counter;
} traffic_analytics_t;

void traffic_analytics_init(traffic_analytics_t *ctx);
void traffic_analytics_update(traffic_analytics_t *ctx, const lane_sample_t *lanes, size_t lane_count);
traffic_decision_t traffic_analytics_evaluate(const traffic_analytics_t *ctx);
void traffic_analytics_print_state(const traffic_decision_t *decision);
void traffic_analytics_build_mqtt_payload(const traffic_decision_t *decision, char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif
