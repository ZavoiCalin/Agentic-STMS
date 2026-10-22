#include "traffic_analytics.h"

#include <stdio.h>
#include <string.h>

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static uint32_t select_dominant_lane(const lane_sample_t *lanes, size_t lane_count)
{
    uint32_t dominant_lane = 0;
    uint32_t highest_priority = 0;

    for (size_t i = 0; i < lane_count; ++i) {
        uint32_t priority = lanes[i].queue_length * 10U + lanes[i].vehicle_count;
        if (priority > highest_priority) {
            highest_priority = priority;
            dominant_lane = (uint32_t)i;
        }
    }

    return dominant_lane;
}

static traffic_state_t classify_state(float congestion_score, bool emergency_vehicle_detected)
{
    if (emergency_vehicle_detected) {
        return TRAFFIC_STATE_CRITICAL;
    }
    if (congestion_score >= 80.0f) {
        return TRAFFIC_STATE_CRITICAL;
    }
    if (congestion_score >= 60.0f) {
        return TRAFFIC_STATE_HEAVY;
    }
    if (congestion_score >= 35.0f) {
        return TRAFFIC_STATE_MODERATE;
    }
    if (congestion_score >= 12.0f) {
        return TRAFFIC_STATE_LIGHT;
    }
    return TRAFFIC_STATE_FREE;
}

void traffic_analytics_init(traffic_analytics_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->queue_factor = 0.45f;
    ctx->inflow_factor = 0.35f;
    ctx->occupancy_factor = 0.20f;
    ctx->task_jitter_ms = 1U;
    ctx->cycle_period_ms = 5000U;
    ctx->inference_active = true;
    ctx->emergency_mode = false;
    ctx->sample_counter = 0U;
}

void traffic_analytics_update(traffic_analytics_t *ctx, const lane_sample_t *lanes, size_t lane_count)
{
    if (ctx == NULL || lanes == NULL) {
        return;
    }

    size_t count = lane_count < TRAFFIC_LANE_COUNT ? lane_count : TRAFFIC_LANE_COUNT;

    for (size_t i = 0; i < count; ++i) {
        ctx->lanes[i] = lanes[i];
    }

    ctx->sample_counter += 1U;
    ctx->cycle_period_ms = 2500U + (ctx->sample_counter * 100U);
    ctx->task_jitter_ms = 1U + (ctx->sample_counter % 3U);
    ctx->inference_active = true;
    ctx->emergency_mode = false;

    for (size_t i = 0; i < count; ++i) {
        if (ctx->lanes[i].emergency_vehicle_detected) {
            ctx->emergency_mode = true;
        }
    }
}

traffic_decision_t traffic_analytics_evaluate(const traffic_analytics_t *ctx)
{
    traffic_decision_t decision = {0};

    if (ctx == NULL) {
        return decision;
    }

    float total_queue = 0.0f;
    float total_vehicles = 0.0f;
    float total_flow = 0.0f;
    float occupancy_average = 0.0f;
    float speed_average = 0.0f;
    bool emergency = false;

    for (size_t i = 0; i < TRAFFIC_LANE_COUNT; ++i) {
        const lane_sample_t *lane = &ctx->lanes[i];

        total_queue += (float)lane->queue_length;
        total_vehicles += (float)lane->vehicle_count;
        total_flow += (float)lane->flow_rate_vpm;
        occupancy_average += (float)lane->occupancy_percent;
        speed_average += (float)lane->average_speed_kmh;
        if (lane->emergency_vehicle_detected) {
            emergency = true;
        }
    }

    total_queue = clampf(total_queue, 0.0f, 120.0f);
    total_vehicles = clampf(total_vehicles, 0.0f, 200.0f);
    total_flow = clampf(total_flow, 0.0f, 320.0f);
    occupancy_average = clampf(occupancy_average / TRAFFIC_LANE_COUNT, 0.0f, 100.0f);
    speed_average = clampf(speed_average / TRAFFIC_LANE_COUNT, 0.0f, 80.0f);

    const float queue_estimate = (ctx->queue_factor * total_queue) +
                                  (ctx->inflow_factor * total_vehicles * 0.5f) +
                                  (ctx->occupancy_factor * occupancy_average * 0.7f);

    const float congestion_score = clampf(
        ((occupancy_average / 100.0f) * 55.0f) +
        ((total_flow / 150.0f) * 25.0f) +
        ((queue_estimate / 80.0f) * 20.0f) +
        ((1.0f - (speed_average / 80.0f)) * 10.0f),
        0.0f,
        100.0f);

    decision.queue_estimate = queue_estimate;
    decision.congestion_score = congestion_score;
    decision.state = classify_state(congestion_score, emergency || ctx->emergency_mode);
    decision.dominant_lane = select_dominant_lane(ctx->lanes, TRAFFIC_LANE_COUNT);
    decision.adaptive_cycle_enabled = (decision.state != TRAFFIC_STATE_FREE);
    decision.scheduling_period_ms = (uint32_t)(2000U + (congestion_score * 35.0f));
    decision.task_jitter_ms = ctx->task_jitter_ms;
    decision.predicted_wait_time_s = ((float)decision.scheduling_period_ms / 1000.0f) * (0.5f + (congestion_score / 100.0f));

    return decision;
}

void traffic_analytics_print_state(const traffic_decision_t *decision)
{
    const char *state_names[] = {
        "FREE",
        "LIGHT",
        "MODERATE",
        "HEAVY",
        "CRITICAL"
    };

    if (decision == NULL) {
        return;
    }

    printf("Traffic state: %s\n", state_names[decision->state]);
    printf("Queue estimate: %.1f veh\n", decision->queue_estimate);
    printf("Congestion score: %.1f%%\n", decision->congestion_score);
    printf("Dominant lane: %u\n", decision->dominant_lane);
    printf("Adaptive cycle: %s\n", decision->adaptive_cycle_enabled ? "enabled" : "disabled");
    printf("Scheduling period: %u ms\n", decision->scheduling_period_ms);
    printf("Predicted wait time: %.1f s\n", decision->predicted_wait_time_s);
}

void traffic_analytics_build_mqtt_payload(const traffic_decision_t *decision, char *buffer, size_t buffer_size)
{
    if (decision == NULL || buffer == NULL || buffer_size == 0U) {
        return;
    }

    snprintf(buffer,
             buffer_size,
             "{\"state\":\"%s\",\"queue_estimate\":%.1f,\"congestion_score\":%.1f,\"dominant_lane\":%u,\"adaptive_cycle\":%s,\"period_ms\":%u}",
             (decision->state == TRAFFIC_STATE_FREE) ? "FREE" :
                 (decision->state == TRAFFIC_STATE_LIGHT) ? "LIGHT" :
                 (decision->state == TRAFFIC_STATE_MODERATE) ? "MODERATE" :
                 (decision->state == TRAFFIC_STATE_HEAVY) ? "HEAVY" : "CRITICAL",
             decision->queue_estimate,
             decision->congestion_score,
             decision->dominant_lane,
             decision->adaptive_cycle_enabled ? "true" : "false",
             decision->scheduling_period_ms);
}
