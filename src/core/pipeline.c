#include "rtsafe/pipeline.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static bool finite_vector3(const float values[3])
{
    return isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]);
}

static float vector_magnitude(const float values[3])
{
    return sqrtf(
        values[0] * values[0] +
        values[1] * values[1] +
        values[2] * values[2]
    );
}

rtsafe_config_t rtsafe_default_config(void)
{
    rtsafe_config_t config;
    config.sample_period_us = 1000U;
    config.execution_budget_us = 800U;
    config.heartbeat_timeout_us = 100000U;
    config.force_stop_n = 120.0F;
    config.max_acceleration_mps2 = 60.0F;
    config.max_angular_rate_rps = 20.0F;
    config.max_encoder_delta_ticks = 1000;
    config.meters_per_tick = 0.0005F;
    config.lowpass_alpha = 0.2F;
    config.degraded_pwm_limit = 0.35F;
    config.healthy_samples_to_normal = 10U;
    config.recovery_samples = 20U;
    config.max_degraded_cycles = 8U;
    return config;
}

bool rtsafe_pipeline_init(rtsafe_pipeline_t *pipeline, const rtsafe_config_t *config)
{
    rtsafe_config_t selected;

    if (pipeline == NULL) {
        return false;
    }

    selected = config != NULL ? *config : rtsafe_default_config();
    if (selected.sample_period_us == 0U ||
        selected.execution_budget_us == 0U ||
        selected.heartbeat_timeout_us == 0U ||
        selected.force_stop_n <= 0.0F ||
        selected.max_encoder_delta_ticks <= 0 ||
        selected.meters_per_tick <= 0.0F ||
        selected.healthy_samples_to_normal == 0U ||
        selected.recovery_samples == 0U ||
        selected.max_degraded_cycles == 0U) {
        return false;
    }

    memset(pipeline, 0, sizeof(*pipeline));
    pipeline->config = selected;
    rtsafe_safety_init(&pipeline->safety);
    rtsafe_lowpass_init(&pipeline->acceleration_filter, selected.lowpass_alpha);
    rtsafe_lowpass_init(&pipeline->yaw_rate_filter, selected.lowpass_alpha);
    if (!rtsafe_moving_average_init(&pipeline->force_filter, 8U)) {
        return false;
    }
    rtsafe_kalman1d_init(&pipeline->velocity_filter, 0.01F, 0.2F);
    return true;
}

rtsafe_status_t rtsafe_pipeline_step(
    rtsafe_pipeline_t *pipeline,
    const rtsafe_input_t *input,
    uint64_t now_us,
    uint32_t execution_time_us
)
{
    rtsafe_status_t status;
    uint32_t faults = RTSAFE_FAULT_NONE;
    float acceleration = 0.0F;
    float yaw_rate = 0.0F;
    float force = 0.0F;
    float velocity = 0.0F;

    memset(&status, 0, sizeof(status));
    if (pipeline == NULL || input == NULL) {
        status.state = RTSAFE_STATE_SAFE_STOP;
        status.fault_flags = RTSAFE_FAULT_TIME_REGRESSION;
        return status;
    }

    acceleration = pipeline->acceleration_filter.value;
    yaw_rate = pipeline->yaw_rate_filter.value;
    force = pipeline->force_filter.count > 0U
        ? pipeline->force_filter.sum / (float)pipeline->force_filter.count
        : 0.0F;
    velocity = pipeline->velocity_filter.estimate;

    if (pipeline->has_previous_sample &&
        input->timestamp_us <= pipeline->previous_timestamp_us) {
        faults |= RTSAFE_FAULT_TIME_REGRESSION;
    }

    if (input->heartbeat_timestamp_us > now_us ||
        now_us - input->heartbeat_timestamp_us > pipeline->config.heartbeat_timeout_us) {
        faults |= RTSAFE_FAULT_HEARTBEAT_LOST;
    }

    if (!input->imu_valid ||
        !finite_vector3(input->acceleration_mps2) ||
        !finite_vector3(input->angular_rate_rps)) {
        faults |= RTSAFE_FAULT_IMU_INVALID;
    } else {
        acceleration = vector_magnitude(input->acceleration_mps2);
        yaw_rate = input->angular_rate_rps[2];
        if (acceleration > pipeline->config.max_acceleration_mps2 ||
            fabsf(yaw_rate) > pipeline->config.max_angular_rate_rps) {
            faults |= RTSAFE_FAULT_IMU_RANGE;
        }
        acceleration = rtsafe_lowpass_update(
            &pipeline->acceleration_filter,
            acceleration
        );
        yaw_rate = rtsafe_lowpass_update(&pipeline->yaw_rate_filter, yaw_rate);
    }

    if (!input->force_valid || !isfinite(input->force_n)) {
        faults |= RTSAFE_FAULT_FORCE_INVALID;
    } else {
        force = rtsafe_moving_average_update(&pipeline->force_filter, input->force_n);
        if (fabsf(input->force_n) >= pipeline->config.force_stop_n) {
            faults |= RTSAFE_FAULT_FORCE_LIMIT;
        }
    }

    if (!input->encoder_valid) {
        faults |= RTSAFE_FAULT_ENCODER_INVALID;
    } else if (pipeline->has_previous_sample) {
        const int64_t delta_ticks =
            (int64_t)input->encoder_ticks - (int64_t)pipeline->previous_encoder_ticks;
        const uint64_t delta_time_us =
            input->timestamp_us > pipeline->previous_timestamp_us
            ? input->timestamp_us - pipeline->previous_timestamp_us
            : 0U;

        if (delta_ticks > pipeline->config.max_encoder_delta_ticks ||
            delta_ticks < -pipeline->config.max_encoder_delta_ticks) {
            faults |= RTSAFE_FAULT_ENCODER_JUMP;
        } else if (delta_time_us > 0U) {
            const float measured_velocity =
                (float)delta_ticks * pipeline->config.meters_per_tick *
                (1000000.0F / (float)delta_time_us);
            velocity = rtsafe_kalman1d_update(
                &pipeline->velocity_filter,
                measured_velocity
            );
        }
    }

    if (execution_time_us > pipeline->config.execution_budget_us) {
        faults |= RTSAFE_FAULT_DEADLINE_MISS;
        ++pipeline->deadline_misses;
    }

    status.sequence = pipeline->sequence++;
    status.timestamp_us = input->timestamp_us;
    status.fault_flags = faults;
    status.filtered_acceleration_mps2 = acceleration;
    status.filtered_yaw_rate_rps = yaw_rate;
    status.estimated_velocity_mps = velocity;
    status.filtered_force_n = force;
    status.deadline_misses = pipeline->deadline_misses;
    status.state = rtsafe_safety_update(&pipeline->safety, faults, &pipeline->config);
    status.motor_enabled =
        status.state == RTSAFE_STATE_NORMAL || status.state == RTSAFE_STATE_DEGRADED;
    status.pwm_limit = status.state == RTSAFE_STATE_NORMAL
        ? 1.0F
        : (status.state == RTSAFE_STATE_DEGRADED
            ? pipeline->config.degraded_pwm_limit
            : 0.0F);

    if (input->encoder_valid) {
        pipeline->previous_encoder_ticks = input->encoder_ticks;
    }
    if (!pipeline->has_previous_sample ||
        input->timestamp_us > pipeline->previous_timestamp_us) {
        pipeline->previous_timestamp_us = input->timestamp_us;
    }
    pipeline->has_previous_sample = true;

    return status;
}
